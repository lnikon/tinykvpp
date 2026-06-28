# 10 — Key-value separation

Depends on: chapter 01 (the inline value layout), chapter 02 (write amplification), chapter 05
(compaction is what rewrites values), chapter 04 (the append-only file the value log reuses), chapter
03 (measurement). Follows the nine-section depth template.

## 1. Problem statement

frankie stores each value inline with its key. The memtable node holds the value bytes
(`src/storage/skiplist.cpp:skiplist_node` lays out key then value), and an SSTable data-block entry is
`[key][value]` (`src/storage/sstable_writer.cpp:append`). The consequence appears once compaction
exists (chapter 05): every time a key is merged down a level, its value is rewritten with it. So write
amplification scales with value size. For a store with large values, the cost of rewriting values
during compaction dominates everything, even though the values themselves never changed.

## 2. Background and theory

The fix, introduced by WiscKey [Lu16WiscKey], is **key-value separation**. Store the values in a
separate append-only file, the **value log** (often written vLog), and keep in the LSM-tree only the
keys plus a small pointer to where each value lives in the value log. A pointer is a fixed, small
record: which value-log file, the offset, and the length. Compaction now merges keys and pointers, not
values, so the work it does per key is constant regardless of how large the value is. Write
amplification becomes independent of value size, which is the whole point.

Nothing is free. Separation has three costs.

The first is an extra read on point lookups. A `get` now finds the pointer in the LSM-tree and then
does one more, random, read into the value log to fetch the value. That extra read is cheap if the
value log is cached and expensive if it is not, which is why the experiment must measure both regimes.

The second is loss of scan locality. With inline values, a range scan reads values in sorted order,
sequentially. With separation, the values are scattered through the value log in insertion order, so a
scan that needs values must do many random reads. For a scan-heavy, large-value workload this can
erase the benefit.

The third is garbage collection. When a key is overwritten or deleted, the space its old value
occupies in the value log becomes garbage, but the value log is append-only, so the space is not
reclaimed automatically. A garbage collector must walk the value log, check each value against the
LSM-tree to see whether it is still the live value for its key, copy the live ones to the end of the
log, and free the old region. If the garbage collector is too aggressive it reintroduces the very
write amplification that separation removed, so its policy matters.

The standard refinement, used by RocksDB's BlobDB and others, is a **size threshold**: small values
stay inline (where the extra read and the garbage collection are not worth it) and only values above
the threshold go to the value log (where the write-amplification saving dominates). This keeps the
common small-value case fast and applies separation only where it pays. HashKV [Chan18HashKV] is a
further refinement that groups values by a hash of the key to make garbage collection cheaper.

## 3. Design space

| Approach | Write amp | Point-read cost | Scan cost | GC complexity | Space |
|----------|-----------|-----------------|-----------|---------------|-------|
| Inline (today) | Scales with value size | One read | Sequential, good | None | Tight after compaction |
| Full separation (WiscKey) | Independent of value size | Two reads | Random, poor | High | Garbage until GC |
| Threshold separation | Saved for large values | Two reads for large only | Mixed | Medium | Medium |
| Separation + hash-grouped GC | As above | As above | As above | Lower GC cost | Medium |

The recommended path is **threshold separation**: keep small values inline, separate only large ones,
and implement a simple liveness-checking garbage collector, then refine the garbage-collection policy
only if it shows up as a cost.

## 4. Notable researchers and key papers

- Lanyue Lu, Thanumalayan Sankaranarayana Pillai, Andrea Arpaci-Dusseau, Remzi Arpaci-Dusseau —
  WiscKey, the original key-value separation design [Lu16WiscKey].
- Helen H. W. Chan and colleagues — HashKV, hash-grouped value management for cheaper garbage
  collection [Chan18HashKV].
- RocksDB BlobDB — the threshold-based separation used in practice.

## 5. Concrete design for this codebase

**The value log.** A value log is an append-only file, so it reuses `src/core/fs.hpp:append_only_file`
directly, the same type the WAL uses. The engine holds a current value-log file and rolls to a new one
when it grows past a size bound, so garbage collection can free whole old files.

**The pointer.** Define a value pointer as `{file_id: u32, offset: u64, size: u32}`. When a value
exceeds `config.value_separation_threshold_`, the engine appends the value to the value log, obtains a
pointer, and stores the pointer (tagged so the reader knows it is a pointer, not an inline value) in
the memtable and ultimately the SSTable. Small values are stored inline exactly as today.

**The read.** The SSTable reader and the memtable, on finding a pointer-tagged entry, resolve it with
one `random_access_file::read` into the indicated value-log file, into an arena buffer.

**The garbage collector.** A background task, structured like compaction (chapter 05), that reads from
the head of the oldest value-log file, and for each value looks up its key in the LSM-tree: if the
live pointer for that key still points here, the value is copied to the current value-log tail and the
LSM pointer is updated; otherwise it is garbage and skipped. When a whole old file is fully processed
it is deleted. To make the liveness check possible, each value-log record also stores its key (or a
back-reference), so the garbage collector knows which key to check.

**Crash consistency.** A value must be durable in the value log before the pointer to it becomes
visible, otherwise a crash could leave a pointer to a value that is not there. So the ordering is:
append and commit the value to the value log (chapter 04's `commit()`), then write the pointer through
the normal WAL-and-memtable path. The WAL can log the pointer rather than the full value, which also
shrinks the WAL for large values; this couples to chapter 12.

`config.value_separation_threshold_` controls the inline-versus-separated boundary, with a default
chosen so that typical small values stay inline.

## 6. C++, memory, and concurrency mechanics

The value pointer is a small POD encoded with the existing `endian_integer` helpers
(`src/core/serialization/endian_integer.hpp`) so its on-disk byte order is fixed. The tag that
distinguishes an inline value from a pointer can be a reserved bit in the existing per-entry encoding,
which means the block format change coordinates with chapter 12's single-source-of-truth encode and
chapter 09's key encoding.

Resolved values are read into the segment's or a request's arena; the same lifetime rules as chapter
01 apply (the buffer must outlive its use, and the scratch arena's `realloc` move hazard means a
resolved value held across another scratch allocation must live in a stable arena).

Garbage collection introduces the same shared-segment concern as compaction: a value-log file being
garbage-collected must not be deleted while a reader is resolving a pointer into it. This reuses the
reference-counting and epoch machinery from chapter 05 and chapter 13; the count is a plain integer on
the single-threaded path and `std::atomic` once a background thread runs the garbage collector.

## 7. Risks, alternatives, and interactions

The dominant risks are the extra read on point lookups (bad when the value log is uncached) and the
loss of scan locality (bad for range-heavy large-value workloads); both are measured directly in the
experiment. Garbage collection is the subtle part: a naive collector can reintroduce write
amplification, so its aggressiveness must be tuned, and its correctness (never resurrecting a deleted
value, never losing a live one) must be tested with the differential and fault-injection harness
(chapter 19).

The fallback is threshold separation with a conservative threshold, so that only genuinely large
values are separated and the common case is unchanged. If even that does not pay off for the target
workload, inline storage stays.

Interactions: write-amplification savings show up in chapter 05's measurements; the pointer tag
couples to chapter 09 and chapter 12's block format; the durability ordering uses chapter 04; the
garbage collector reuses chapter 05's and chapter 13's reference-counting.

## 8. Experiment plan

**Hypothesis.** For large values, key-value separation makes write amplification roughly independent
of value size, at the cost of one extra read per point lookup and reduced scan locality; for small
values it is not worth the indirection.

**Setup.** Engine-level driver (chapter 03) with compaction enabled (chapter 05), across a sweep of
value sizes (small to large) and a realistic mixed distribution. Run write-heavy load, point reads,
and range scans in cached and uncached regimes.

**Baseline.** Today's inline storage.

**Variants.** Full separation; threshold separation at a few thresholds; with and without a running
garbage collector.

**Metrics.** Write amplification versus value size; point-read latency (showing the indirection cost,
cached and uncached); scan throughput (showing locality loss); garbage-collector write amplification
and space reclaimed over time.

**Success criteria.** Write amplification flattens with value size under separation; threshold
separation keeps small-value point reads at baseline; the garbage collector reclaims space without
its own write amplification exceeding the saving.

**Killer result.** If the workload's values are mostly small, or scans dominate, separation costs more
than it saves and inline storage stays.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section N (Key-value separation).

1. Add a value-log abstraction over `append_only_file`, with file rolling.
2. Define the value-pointer encoding and the inline-versus-pointer tag, coordinated with chapters 09
   and 12.
3. Separate values above `config.value_separation_threshold_` on write, with the value-durable-before-
   pointer-visible ordering (chapter 04).
4. Resolve pointers on read in the memtable and SSTable reader.
5. Implement a liveness-checking garbage collector with reference-counted file lifetimes.
6. Run the experiment in section 8 and record results here.
