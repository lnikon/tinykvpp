# 01 — Architecture walkthrough: the system as it exists today

This chapter describes the code that exists in this repository right now. It exists so that every
later chapter can point at a real function or field instead of describing an idea in the abstract.
Read it before any Part II chapter.

The description is in linear order. It starts with the shape of the whole engine, then follows the
four data paths (startup, write, read, flush) through the actual functions, then explains the three
cross-cutting subsystems those paths rely on (memory, errors, serialization), then the on-disk
formats, then the concurrency model, and finally the known rough edges that the research chapters
will refer to.

Throughout, the project's namespace is `frankie`. The on-disk and in-memory names sometimes say
"segment" where this document says "SSTable"; they mean the same on-disk sorted file.

## 1. The shape of the engine

frankie is a **log-structured merge-tree** (LSM-tree) key-value store. Chapter 02 explains that
family in full; the one-sentence version is: writes are absorbed in memory and appended to a log,
the in-memory table is periodically written to disk as an immutable sorted file, and a background
process later merges those files. The point of the design is to turn the random writes a key-value
store receives into the sequential writes that disks are good at.

The engine is a single C++ object, `frankie::engine::engine`, defined in `src/engine/engine.hpp`.
It owns, as plain members, everything needed to serve requests:

- `config_` — the configuration (`src/core/config.hpp:config`): paths and capacities.
- `scratch_arena_` — a scratch memory buffer for temporary work (explained in section 6).
- `memtable_active_` — the in-memory table that currently absorbs writes.
- `memtable_immutable_` — an optional second in-memory table, holding data that is being flushed.
- `sequence_` — a monotonically increasing counter; every write gets the next value.
- `wal_` — the write-ahead log writer.
- `sstable_file_prefix_` and `sstable_id_` — the naming state for the next SSTable file.

The engine is move-only: its copy constructor and copy assignment are deleted, and its move
operations are defaulted (`src/engine/engine.hpp`). This is consistent across the codebase; almost
every owning type is move-only, because the owned resources (file descriptors, arena blocks) must
not be copied.

The public interface is small and every method returns `std::expected<T, core::status>` so that
errors are values, not exceptions:

```
static std::expected<engine, core::status> create(core::config config) noexcept;
std::expected<void, core::status>            put(std::string_view key, std::string_view value) noexcept;
std::expected<std::string_view, core::status> get(std::string_view key) noexcept;
std::expected<void, core::status>            del(std::string_view key) noexcept;
void                                          scan(std::string_view start, std::string_view end) noexcept; // stub
```

The directory layout matches the module boundaries:

- `src/core/` — primitives with no knowledge of the storage engine: the arena allocators, the file
  abstractions, the status type, the serialization helpers, the SIMD key comparison, the CRC32
  routine, and a wall-clock helper.
- `src/storage/` — the storage data structures: the skiplist, the memtable, and the SSTable writer,
  reader, and format definitions.
- `src/engine/` — the orchestration: the engine itself and the write-ahead log.

## 2. The startup and recovery path

Everything begins at `src/engine/engine.cpp:engine::create`. It runs in this order.

First it creates an empty active memtable sized by `config.memtable_capacity_` (the default is
64 MiB, `src/core/config.hpp:kDefaultMemtableCapacity`).

Second it opens a write-ahead log reader on `config.wal_path_` by calling
`src/engine/wal.cpp:wal_reader::open`. Two outcomes are treated as a clean first start rather than
errors: `status_code::not_found` means no log file exists yet, and `status_code::eof` means the log
file exists but is empty. Any other failure is returned to the caller, because silently continuing
could lose data that is on disk.

Third, if a non-empty log exists, it replays it. The reader has already slurped the whole file into
memory at open time (section 7 explains why), and `wal_reader::read` hands back one decoded entry at
a time. For each entry the engine calls `memtable_active.put(...)` to re-apply the write, and it
tracks the largest sequence number it sees. The loop ends when `read` returns `status_code::eof`;
any other terminating error is propagated.

Fourth it opens a write-ahead log writer with `wal_writer::open`.

Fifth it sets `sequence_` to the largest recovered sequence number plus one, so that new writes
continue numbering where the recovered log left off.

The important property of this path is that recovery is just "replay the log into a fresh memtable".
There is no SSTable-level recovery yet, because nothing reads SSTables back yet (section 10).

## 3. The write path (put and delete)

A write enters at `src/engine/engine.cpp:engine::put`. The steps, in order:

1. It takes the next sequence number with `get_next_sequence()`, which post-increments `sequence_`.
2. It appends a record to the write-ahead log with `wal_.append({...})`. The record carries the
   operation (`put`), the sequence number, the key, the value, and a tombstone flag of `false`. If
   the append fails it returns `status_code::io_error`.
3. It applies the write to the active memtable with `memtable_active_.put(key, value, sequence,
   false)`.
4. It calls `maybe_rotate_memtable()` to flush the memtable to disk if it is now full (section 5).

`engine::del` is identical except the operation is `del`, the value is empty, and the tombstone flag
is `true`. A delete is therefore a write of a **tombstone**: a record that marks the key as deleted.
Nothing is erased in place. This is the only way to delete in an append-only design, and it is why
chapter 05 (compaction) must later remove the tombstone and the data it shadows.

The ordering of steps 2 and 3 matters and is correct: the log is made durable before the in-memory
state changes, so a crash after step 2 but before step 3 loses nothing (recovery replays the log),
and a crash after step 3 cannot have skipped step 2.

One cost is visible here and is the subject of chapter 04: `wal_writer::append` in
`src/engine/wal.cpp` calls `file_.append_fsync`, which forces the record to stable storage on every
single write. That is the strongest possible durability, and also the slowest possible, because each
write waits for the disk.

### Inside memtable::put

`src/storage/memtable.cpp:memtable::put` builds an **internal key**, encodes it, and inserts it.

The internal key (`src/storage/memtable.hpp:internal_key`) is the user's key plus three pieces of
metadata: the sequence number (8 bytes), a timestamp (8 bytes, set from `core::wall_clock_ms()`),
and the tombstone flag (1 byte). Its `encode` method (`src/storage/memtable.cpp:internal_key::encode`)
lays these out as `[user_key bytes][sequence][timestamp][tombstone]` into the scratch arena and
returns a `std::string_view` over them.

That encoded view is then handed to `skiplist_.insert(ikey_encoded, value)`. This is safe even
though the scratch arena is transient, because the skiplist immediately copies the bytes into a node
of its own durable arena (section 4 explains the node layout). After insertion the memtable bumps
its entry count and its byte counter.

### The skiplist

The memtable's ordered storage is a **skiplist** (`src/storage/skiplist.hpp`,
`src/storage/skiplist.cpp`). A skiplist is a linked list with several levels of forward pointers; a
search starts at the top level and drops down, skipping over large spans, giving logarithmic search
and insertion without the rebalancing a tree needs.

Two details matter for later chapters.

The node memory layout is a single flat allocation. `src/storage/skiplist.cpp:skiplist_node::create`
asks the arena for one block sized by `layout_size(height, key_size, value_size)` and lays out, in
order, the node header, then the array of `height` forward pointers, then the key bytes, then the
value bytes, all contiguous. The accessors (`forward()`, `key()`, `value()`) compute their spans by
pointer arithmetic from `this`. This is deliberately cache-friendly: one allocation, no indirection
to separate key or value buffers.

The comparator is pluggable through a template parameter constrained by the `Comparator` concept
(`src/storage/skiplist.hpp`). The memtable instantiates the skiplist with
`internal_key_comparator` (`src/storage/memtable.hpp`), which strips the trailing metadata from both
keys and compares only the user-key portion, using SIMD (`src/core/simd.hpp:simd_compare_sse2`).

The consequence of comparing only the user key is important and is the starting point of chapter 11:
two writes of the same user key with different sequence numbers compare as equal. In
`src/storage/skiplist.hpp:skiplist::insert`, an equal key takes the "replace" branch, which links in
a new node and abandons the old one. So within a single memtable only the newest version of a key
survives. The memtable is not multi-version; it is last-writer-wins per user key.

## 4. The read path (get)

A read enters at `src/engine/engine.cpp:engine::get`. The logic is short:

1. Look in the active memtable with `memtable_active_.get(key)`. If found and it is a tombstone,
   return `status_code::not_found`; if found and live, return the value.
2. Otherwise, if an immutable memtable exists, look there with the same tombstone handling.
3. Otherwise return `status_code::not_found`.

`src/storage/memtable.cpp:memtable::get` builds a lookup internal key with sequence and timestamp set
to zero, encodes it, and calls `skiplist_.get`. Because the comparator ignores the metadata, the
lookup finds the single stored version of that user key regardless of its sequence number. The
returned encoded key is decoded back into an `internal_key` so the caller learns the stored sequence,
timestamp, and tombstone flag.

The gap to notice, and the subject of `TASKS.md` section A and chapter 11, is step 3: `engine::get`
**never consults SSTables**. Once a memtable has been flushed and dropped, its data is on disk but
unreachable by `get`. Closing this gap needs a working SSTable reader and a way to know which
SSTables exist (a manifest, chapter 05).

## 5. The flush and rotation path

`src/engine/engine.cpp:engine::maybe_rotate_memtable` is called after every write. Its job is to move
the active memtable aside and write it to disk once it is full.

It first checks fullness: it compares `memtable_active_.bytes_allocated()` (which returns the
skiplist's accumulated node bytes) against `memtable_active_.capacity()`. If the table is not yet
over capacity it returns immediately.

When the table is full it does the following, in order:

1. Move the active memtable into `memtable_immutable_` and construct a fresh active memtable.
2. Build the output file name `sst_<id>` by formatting `sstable_id_` into a scratch buffer with
   `std::snprintf`, and create the file with `random_access_file::create_exclusive` under
   `config.sstable_dir_path_`.
3. Create an `sstable_writer` (`src/storage/sstable_writer.cpp`).
4. Iterate the immutable memtable in sorted order (the skiplist's iterator), calling
   `writer->append(ikey, value)` for each entry. Whenever `writer->is_data_block_complete()` becomes
   true, take the finished block with `writer->get_data_block()`, write it to the file at the running
   offset, record its offset and size with `writer->record_data_block(...)`, and advance the offset.
5. After the loop, flush any trailing partial block the same way.
6. Serialize the index with `writer->get_index()`, write it after the last data block, and remember
   its offset and size.
7. Serialize the footer with `writer->get_footer(...)` and write it after the index.
8. Increment `sstable_id_` and truncate the write-ahead log with `wal_.truncate()`, because the data
   it protected is now on disk.

Two things about this path are unfinished and are tracked in `TASKS.md` sections A and D.

First, the rotation lifecycle is incomplete. After the flush, `memtable_immutable_` is not cleared,
and the newly written SSTable is not registered anywhere. There is no manifest and no in-memory list
of segments, so even though the bytes are on disk, no later `get` can find them. The flushed data is
effectively write-only until the reader and manifest exist.

Second, the footer encoding has a round-trip bug, described in section 8.

## 6. The memory model

The codebase manages memory with two arena allocators and almost no use of the general-purpose
heap in hot paths. Understanding them is required for every chapter that proposes a new data
structure, because the question "which arena does this live in, and who frees it" recurs constantly.

### The block arena

`src/core/arena.hpp` and `src/core/arena.cpp` implement `core::arena`, a **bump allocator** backed
by a singly linked list of heap blocks.

`arena::create(capacity)` allocates the first block with `std::aligned_alloc` aligned to 64 bytes
(`kBlockAlignment`). Each block is an `arena_block` header (just a `next_` pointer) followed by the
usable bytes; `arena_block::data()` returns the address just past the header.

`arena::allocate(requested, alignment)` rounds the current offset up to the requested alignment,
and if the current block cannot satisfy the request it allocates a new block, pushes it on the front
of the list, and resets the offset. It then returns `current_->data() + offset` and advances the
offset by the requested size. It never frees individual allocations; that is the nature of a bump
allocator.

`arena::destroy()` walks the list and frees every block.

Three details will be referenced later:

- There is **no null check** after `std::aligned_alloc` in either `create` or `allocate`. If the
  system is out of memory, the allocator returns a null block and the next use dereferences it. This
  is `TASKS.md` section C; it is why the `out_of_memory` branches at the call sites are currently
  dead code.
- The growth size of overflow blocks is the default 32 KiB (`kDefaultBlockSize`), **not** the
  capacity passed to `create`. The line that would set `default_block_size_ = capacity` is commented
  out in `arena::create`. So an arena created with a 64 MiB capacity has a 64 MiB first block, but if
  it ever overflows, it grows by `max(32 KiB, requested)` blocks. For the memtable this is usually
  invisible (the first block is large), but it is a surprise waiting for any new caller.
- The alignment argument must be a power of two no larger than 64; this is checked with debug
  assertions, because a block only guarantees 64-byte alignment.

### The scratch arena

`src/core/scratch_arena.hpp` and `src/core/scratch_arena.cpp` implement `core::scratch_arena`, a
single growable buffer for short-lived work. `allocate(size)` grows the buffer with `std::realloc`
when needed (doubling, or jumping straight to the needed size) and returns a pointer into it.
`reset()` sets the offset back to zero without freeing, so the buffer is reused. `destroy()` frees
it.

The sharp edge here, which explains a recurring pattern in the codebase, is that `std::realloc` may
**move** the buffer. Any pointer obtained before a growth can therefore dangle after it. The safe
pattern, used everywhere the scratch arena appears, is: call `reset()`, make a single allocation,
use it immediately, and do not hold it across another allocation. This is exactly what
`internal_key::encode` and `wal_entry::encode` do: each calls `arena.reset()` at the top and
produces one encoded buffer. As with the block arena, the `realloc` result is not null-checked
(there is a TODO at the call site).

### Ownership across moves

The memtable owns its arena by value but the skiplist inside it stores a **raw pointer** to that
arena. When a memtable is moved (which happens on every rotation, section 5), the arena's heap
blocks keep their addresses, but the address of the arena object changes. So the skiplist's pointer
would dangle. The fix is in `src/storage/memtable.cpp`: both the move constructor and move assignment
call `skiplist_.rebind_arena(&arena_)` to re-point the skiplist at the moved-to memtable's arena.
This is the kind of detail that any concurrent or reorganizing redesign (chapters 08 and 13) must
preserve.

## 7. The error model

Errors are values. The core type is `core::status` (`src/core/status.hpp`): a status code plus a
borrowed C-string message. It is small (asserted to be at most 16 bytes) and trivially copyable, so
it is cheap to pass by value. The codes are coarse and grouped by what the caller can do about them:
`ok`, `not_found`, `io_error`, `invalid_argument`, `corrupted`, `eof`, `out_of_memory`. A
default-constructed status means success, and `operator bool` returns true on success.

Functions return `std::expected<T, core::status>`. The helper `core::unexpected(...)` wraps a code
or a status into the `std::unexpected` form.

There is a second, parallel error enum that the serialization layer uses:
`core::serialization_error_k` (`src/core/serialization/common.hpp`), with values like
`buffer_overflow_k` and `truncated_file_k`. It is converted to a `core::status` at each boundary,
which loses information. Merging the two is `TASKS.md` section B and is discussed in chapter 12.

## 8. The serialization layer and the on-disk formats

### The byte-level helpers

`src/core/serialization/buffer_writer.hpp` and `buffer_reader.hpp` implement a cursor over a byte
span with chaining methods that stop at the first error. They can write and read fixed-width
integers, variable-length integers, length-prefixed strings, and raw bytes.

A **variable-length integer** (varint) stores a number in as few bytes as possible: seven bits of
payload per byte, with the top bit set to mean "more bytes follow"
(`buffer_writer.hpp:write_varint`, `buffer_reader.hpp:read_varint`).

Fixed-width integers are written through `endian_integer`
(`src/core/serialization/endian_integer.hpp`), a wrapper that stores a value in a chosen byte order
(the aliases such as `le_uint32_t` are little-endian). It uses `std::byteswap` only when the host
order differs from the target, and it is recognized by the `EndianInteger` concept
(`src/core/serialization/concepts.hpp`). This guarantees the on-disk byte order is fixed regardless
of the machine.

### The write-ahead log record

The WAL record format is defined and implemented in `src/engine/wal.cpp:wal_entry::encode`. In
order, a record is:

```
[record_len: u32][crc32: u32][operation: u8][sequence: u64][tombstone: u8]
[key_len: u32][value_len: u32][key bytes][value bytes]
```

`record_len` counts every byte after the `record_len` and `crc32` fields. The CRC32 is computed over
those same bytes (everything after the two header fields) so that a reader can verify integrity
before trusting the lengths. `wal_entry::decode` reads `record_len` and the checksum, recomputes the
checksum, checks the lengths against the buffer, and only then exposes the key and value as views
into the buffer. This is why the reader slurps the whole file at open time (section 2): the views
must point at stable memory for the lifetime of the read loop.

### The SSTable format

The on-disk SSTable is laid out, in order: a sequence of data blocks, then the index region, then
(eventually) a filter region, then a fixed footer. The intended layout is documented at the top of
`src/storage/sstable_format.hpp`, and the structures are defined there:

- `sstable_data_block_header` is 20 bytes and carries `entry_count_`, `uncompressed_size`,
  `compressed_size`, a `compression_type` (`none` or `lz4`), padding, and a `crc32_`.
- `sstable_footer` carries `index_offset_`, `index_size_`, `bloom_offset_`, `bloom_size_`, and a
  `crc32_` (five 32-bit fields, 20 bytes).

A data block on disk, as actually produced by `src/storage/sstable_writer.cpp:get_data_block`, is the
length-prefixed header followed by the length-prefixed body, where the body is a sequence of entries
each written as `write_string(ikey)` then `write_string(value)` (so each is a varint length followed
by bytes). The index region, from `get_index`, is a sequence of entries each written as the
length-prefixed smallest key, then the block offset and block size as little-endian 64-bit integers.

Two correctness problems live here and are tracked in `TASKS.md` section A.

First, the documented per-entry format in `sstable_format.hpp` says the key length is a `u32`, but the
writer actually emits a varint, and the documented index region says it begins with an
`entry_count`, which the writer does not emit. The document and the code disagree.

Second, and more serious, the footer does not round-trip. `get_footer` in the writer emits only two
fields, in the order `index_size` then `index_offset`, as little-endian 32-bit integers (8 bytes
total). The reader, in `src/storage/sstable_reader.hpp:segment::create`, reads two fields in the
opposite order, `index_offset` then `index_size`, from a fixed-size buffer, and elsewhere uses the
wrong field name (`footer_.index_size_` instead of the locally decoded `sst_footer.index_size_`).
The `sstable_footer` struct is 20 bytes but only 8 are written, so the remaining bytes are whatever
was in the buffer. Chapter 12 proposes a single `encode`/`decode` pair as the one source of truth to
make this class of bug impossible.

The SSTable reader itself is a work in progress: `src/storage/sstable_reader.cpp` has empty function
bodies, and the `segment` parser in the header returns an empty `{}` before finishing. So the read
side of the format is not yet functional.

## 9. The concurrency model

The engine is **single-threaded**. There are no locks and no atomic variables anywhere in the data
structures. The skiplist seeds its height random-number generator from `std::random_device`
(`src/storage/skiplist.hpp`) but otherwise uses ordinary, non-atomic memory. The arenas are not
thread-safe.

This is a deliberate starting point, consistent with the project's stated intent (in `README.md`)
to build "no zoo of threads" and instead use a single main loop driven by an explicit state machine
plus asynchronous I/O. The concurrency model is, however, never written down in the code, which is
`TASKS.md` section G. Chapter 13 takes up the question of what the concurrency model should become,
and chapter 04 takes up the asynchronous-I/O half of the README's plan.

## 10. Known rough edges referenced by later chapters

This is a single place to find the gaps that the research chapters build on. Each is grounded in
the code above.

- `engine::get` does not read SSTables (section 4). Needs a working reader and a manifest.
- The rotation lifecycle does not clear the immutable memtable or register the new SSTable
  (section 5).
- The SSTable footer does not round-trip, and the documented format disagrees with the writer
  (section 8).
- The SSTable reader (`src/storage/sstable_reader.cpp`) is unimplemented (section 8).
- The block arena does not check `std::aligned_alloc`, and the scratch arena does not check
  `std::realloc`, for null (section 6).
- The block arena's overflow growth uses 32 KiB blocks, not the creation capacity (section 6).
- `src/core/dynamic_array.hpp` is currently broken and unused: `append` returns a value from a
  `void` function, `increase_capacity` casts the buffer to `TEntry` instead of `TEntry*`, copies
  the wrong number of bytes (`size_` elements counted as bytes), and calls `std::free` on memory the
  arena owns. It is a useful concrete example of why arena ownership rules need to be explicit, and
  chapter 08 revisits the idea of a proper growable arena-backed array.
- The `scan` method is a stub (`engine::scan`); chapter 11 designs the real iterator.
- The `crc32_` fields in the SSTable header and footer are never set or checked (chapter 12), and
  the monotonic clock for the entry timestamp exists as a field but is set from wall-clock time, not
  a monotonic source.

With this grounding in place, the remaining chapters can speak precisely. Chapter 02 now steps back
from the code and explains the theory that says why an LSM-tree is shaped the way it is, and what the
costs of that shape are.
