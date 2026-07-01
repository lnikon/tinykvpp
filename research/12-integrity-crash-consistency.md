# 12 — Integrity and crash consistency

Depends on: chapter 01 (the dead checksum fields, the footer bug, the error model), chapter 04 (fsync
and durability), chapter 05 (the manifest), chapter 19 (fault injection, which tests this). Follows
the nine-section depth template.

## 1. Problem statement

frankie can detect corruption in its write-ahead log but not in its SSTables, its on-disk format does
not round-trip, and it has no crash-safe way to install a set of files. Concretely:

- The WAL checksums each record and verifies it on read
  (`src/engine/wal.cpp:wal_entry::encode`/`decode`). That part is sound.
- The SSTable header and footer have `crc32_` fields (`src/storage/sstable_format.hpp`), but they are
  never set and never checked. An SSTable block that rots on disk is read as if valid.
- The footer does not round-trip. `src/storage/sstable_writer.cpp:get_footer` writes two fields in the
  order `index_size` then `index_offset`, only 8 of the struct's 20 bytes, while
  `src/storage/sstable_reader.hpp:segment::create` reads them in the opposite order and uses a wrong
  field name. The remaining 12 bytes are uninitialized on disk. This is `TASKS.md` section A.
- There is no manifest (chapter 05), so there is no atomic way to install the result of a flush or
  compaction, and no crash-safe record of which files make up the database.
- The serialization error type `serialization_error_k` is separate from `core::status` and is
  translated lossily at every boundary (chapter 01, section 7), which is `TASKS.md` section B.

## 2. Background and theory

Two distinct properties are at stake, and they are often confused.

**Integrity** is detecting that data on disk is not what was written. Storage devices suffer silent
corruption: bits flip, sectors are misdirected, and writes are lost, sometimes without the device
reporting an error. A large-scale study of this by Bairavasundaram and colleagues [Bairavasundaram08]
found such corruption is real and non-negligible, which is why every serious storage engine checksums
its data. A **checksum** is a short number computed from a block of bytes and stored with them; on
read it is recomputed and compared, and a mismatch means corruption. The checksum must be computed
over exactly the bytes that are written to disk and verified before those bytes are trusted, for each
unit that is read independently (a block, the index, the footer, a log record).

**Crash consistency** is recovering to a correct state after a crash that happens mid-write. The
hazards are the **torn write** (a record only partly persisted) and the partial multi-file change (some
files of a compaction installed, others not). The foundational technique is write-ahead logging,
formalized as ARIES by Mohan and colleagues [Mohan92]: the log record for a change is made durable
before the change is considered done, so recovery can always move forward from the log. For multi-file
changes, the technique is a **manifest**: an append-only log of version edits (chapter 05), where the
current set of files is the replay of the log, and installing a change is one durable append. Recovery
replays the manifest up to the last fully durable edit and discards a torn trailing edit. Pillai and
colleagues [Pillai14] showed that file systems differ in what they guarantee about write ordering and
atomicity, so an engine must not assume more than the minimum: only that a single `fdatasync` makes
prior writes to that file durable, and that careful ordering plus checksums, not the file system's
luck, provide consistency.

The two properties combine in the **fsync-ordering rule**: data must be durable before any pointer
that references it becomes durable. So an SSTable must be fully written and fdatasynced before the
manifest edit that adds it is fdatasynced; otherwise a crash could leave the manifest pointing at a
file that is not fully there.

**Checksum algorithm choice.** Plain CRC32 (the project's `src/core/crc32.hpp`, a software table
implementation) detects the common corruption patterns but is slow. **CRC32C** uses a polynomial
(Castagnoli [Castagnoli93]) that modern x86 CPUs compute with a single hardware instruction, so it is
far faster for the same detection strength. **xxHash** (and xxh3) is a fast non-cryptographic hash
usable as a checksum. For an engine on x86, CRC32C is the usual choice because it is both fast and a
true error-detecting code. The **granularity** is a tradeoff: per-block checksums match the read unit
and are the standard; per-entry is finer but costs more; per-file is too coarse to localize damage.

## 3. Design space

| Decision | Options | Recommended |
|----------|---------|-------------|
| Checksum algorithm | none (today, SSTables), CRC32 software, CRC32C hardware, xxh3 | CRC32C hardware |
| Granularity | per-file, per-block, per-entry | per-block (the read unit) |
| What is checksummed | uncompressed bytes vs on-disk bytes | the on-disk (compressed) bytes |
| Format definition | drifting docs vs one encode/decode | one `encode`/`decode` per record |
| Multi-file install | rename tricks vs version-edit log | version-edit log (manifest) |
| Recovery | WAL replay only (today) vs WAL + manifest | WAL + manifest |
| Error type | `serialization_error_k` + `status` vs merged | merged into `core::status` |

## 4. Notable researchers and key papers

- C. Mohan and colleagues — ARIES write-ahead logging and recovery [Mohan92].
- Lakshmi Bairavasundaram and colleagues — analysis of data corruption in the storage stack
  [Bairavasundaram08].
- Thanumalayan Pillai and colleagues — how file-system crash behavior differs and what applications
  may assume (the ALICE study) [Pillai14].
- Jayashree Mohan and colleagues — CrashMonkey, systematic testing of crash consistency [Mohan18].
- Guy Castagnoli and colleagues — the CRC32C polynomial [Castagnoli93]; Yann Collet — xxHash.

## 5. Concrete design for this codebase

**One source of truth for the format.** Introduce `encode`/`decode` pairs for the footer, the index
entry, and the block header, mirroring the existing `internal_key` and `wal_entry` pattern
(`src/storage/memtable.cpp`, `src/engine/wal.cpp`). The writer and reader both call the same pair, so
the field order and width cannot drift. Add `static_assert(sizeof(sstable_footer) == 20)` so a layout
change cannot silently break the format. This fixes the footer round-trip bug because there is only one
place that defines the layout. This is the central fix of `TASKS.md` section A.

**Checksums.** Compute a checksum over each block image, the index image, and the footer (over the
exact on-disk bytes), store it in the existing `crc32_` fields, and verify it on read before trusting
the bytes. Because chapter 09 may compress blocks, the checksum is computed over the compressed image
(section 6 of chapter 09), and verification happens before decompression.

**Algorithm.** Add a CRC32C implementation using the hardware instruction with a runtime feature
check, falling back to the existing software table when the instruction is unavailable, so the format
records which algorithm was used. This reuses the project's existing comfort with SIMD and CPU feature
detection (`src/core/simd.hpp`).

**Crash-safe install.** Use the manifest (chapter 05) as the version-edit log, with the fsync-ordering
rule: write and fdatasync the SSTable, then append and fdatasync the manifest edit that adds it.
Recovery replays the manifest to the last good edit (a torn trailing edit fails its checksum and is
discarded) and then replays the WAL into the memtable.

**Unify the error type.** Merge `serialization_error_k` into `core::status` so the serialization layer
returns the same status as everything else and the lossy translation at each boundary disappears
(`TASKS.md` section B). Adopt the contract policy from `TASKS.md` section B: assertions are for
programmer errors (a violated invariant), and `core::status` is for runtime conditions (I/O failure,
format corruption, end of file).

## 6. C++, memory, and concurrency mechanics

The hardware CRC32C is the `_mm_crc32_u64` intrinsic, guarded by a runtime check such as
`__builtin_cpu_supports("sse4.2")`, with the software `core::crc32` as the fallback; the chosen
algorithm is recorded in the format so a file written on one machine is read correctly on another.
The checksum operates on `std::span<const std::byte>`, the same type the existing serialization layer
uses.

The `encode`/`decode` pairs return a `std::string_view` into an arena buffer for `encode` (exactly as
`wal_entry::encode` does) and parse from a `std::span<const std::byte>` for `decode` (as
`buffer_reader` does). The `static_assert` on `sizeof(sstable_footer)` is a compile-time guard; the
contract policy distinguishes `FR_VERIFY`-style assertions (programmer error, may abort) from returned
`core::status` (runtime error, must be handled), which makes the difference between a bug and an
expected failure explicit at every call.

The read side is immutable and therefore free of concurrency concerns, but the manifest write is the
one ordering-sensitive operation: its append must be durable (chapter 04's `commit()`), and the
ordering of the SSTable fdatasync before the manifest fdatasync must be preserved even when chapter
13's execution model makes I/O asynchronous. With io_uring this means the manifest-edit submission
must be ordered after the SSTable-flush completion, which io_uring's linked-operation feature can
express, or which the engine can enforce by waiting for the flush completion before submitting the
edit.

## 7. Risks, alternatives, and interactions

The main runtime cost is checksum CPU on reads, which the hardware CRC32C minimizes; the experiment
measures it. The main correctness risk is the recovery logic itself, which is hard to get right by
inspection and must be tested by fault injection (chapter 19) that crashes at every fsync point and
checks that recovery always reaches a consistent state. The fsync-ordering rule is easy to violate
once I/O becomes asynchronous (chapter 13), so it needs an explicit test, not just careful coding.

There is no real fallback on integrity: an engine without checksums silently serves corrupted data,
which is unacceptable, so per-block CRC32C plus a manifest is treated as the baseline, not an
enhancement. The only genuine choice is the algorithm and granularity, settled above.

Interactions: the single-source-of-truth format fixes `TASKS.md` section A and is a precondition for
chapters 06 (filter offsets in the footer) and 09 (block format changes); the manifest is shared with
chapter 05; the durability ordering uses chapter 04; the recovery test uses chapter 19.

## 8. Experiment plan

**Hypothesis.** Per-block CRC32C detects corruption with negligible read overhead, and a manifest plus
the fsync-ordering rule recovers to a consistent state after a crash at any point.

**Setup.** Two experiments. For integrity: write SSTables, flip random bits in the files, and read them
back, asserting every corruption is detected and never silently served. For crash consistency: use the
deterministic fault-injection harness (chapter 19) to crash the engine at every fsync boundary during
flush and compaction, then recover and check, with the differential model (chapter 11), that the
recovered state is one of the valid pre- or post-operation states and never a mixture.

**Baseline.** Today's behavior: SSTable corruption undetected, no manifest.

**Variants.** CRC32 software versus CRC32C hardware for the overhead measurement; per-block versus
per-entry granularity.

**Metrics.** Corruption-detection rate (must be 100% for the patterns tested); checksum CPU per read,
cached so the disk does not hide it; and crash-recovery correctness across all injected crash points.

**Success criteria.** All injected corruptions are detected; all injected crashes recover to a
consistent state; CRC32C overhead is small enough to be acceptable on every read.

**Killer result.** There is none for integrity itself (it is mandatory); the only negative outcome
worth recording is if hardware CRC32C is unavailable on the target and the software fallback is too
slow, in which case xxh3 is evaluated as the algorithm.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section A (format), section B (error model), and section G (wire up the
dead crc32 fields).

1. Introduce `encode`/`decode` pairs for the footer, index entry, and block header; route writer and
   reader through them; add the `static_assert` on the footer size. This fixes the round-trip bug.
2. Add CRC32C (hardware with software fallback); checksum each block, the index, and the footer over
   the on-disk bytes; verify before trusting (and before decompressing, chapter 09).
3. Make the manifest install crash-safe with the fsync-ordering rule (SSTable durable before its
   manifest edit).
4. Merge `serialization_error_k` into `core::status` and adopt the assert-versus-status contract.
5. Build the corruption and crash-recovery experiments (chapter 19) and record results here.
