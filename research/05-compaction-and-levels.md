# 05 — Compaction and the level structure

Depends on: chapter 01 (flush and the segment layout), chapter 02 (leveling/tiering theory and the
amplifications), chapter 11 (the merging iterator this chapter reuses), chapter 03 (measurement).
Follows the nine-section depth template.

## 1. Problem statement

frankie writes SSTables but never merges them. `src/engine/engine.cpp:engine::maybe_rotate_memtable`
flushes the immutable memtable into a new file under `config.sstable_dir_path_` and stops there.
There is no compaction, so three things grow without bound: the number of SSTables, the read
amplification (a read would have to consult every table), and the space amplification (every
overwritten value and every tombstone stays on disk forever).

The intent to have levels exists in the tree but is not wired up. The directories
`tmp/segments/level_1` through `tmp/segments/level_5` exist. The file `next.txt` sketches a plan: a
manifest that stores the latest segment id, a `segments_mgr` that scans the `segments/` topology and
caches it, and an engine that uses the manifest to find the path of the next segment to search. The
`segment` struct in `src/storage/sstable_reader.hpp` already carries a `reference_count_` field with
the comment that compaction must track in-use segments so it does not delete them too early. None of
this is implemented.

Because there is also no manifest and no in-memory registry of segments, even a finished SSTable is
invisible to reads (chapter 01, section 5). So compaction and the manifest are entangled: you cannot
usefully compact files that nothing tracks, and you cannot safely read files whose set can change
under you without an atomic record of what that set is.

## 2. Background and theory

Compaction is the process that bounds read and space amplification by merging sorted runs and
discarding data that is no longer needed. Chapter 02 derived why it is necessary and what leveling,
tiering, and lazy leveling cost; this section is about how it actually works and what makes it
correct.

**The merge.** Compaction takes several SSTables, each sorted by key, and produces one or more
output SSTables also sorted by key. The mechanism is a k-way merge: read the smallest unconsumed key
across all inputs, emit it, advance that input, repeat. Because each input is sorted, the output is
sorted in a single streaming pass with memory proportional to the number of inputs, not their size.

**What gets dropped.** During the merge, when the same user key appears in several inputs, only the
version with the highest sequence number is kept; the older ones are discarded, which is how
overwrites are reclaimed. A tombstone (chapter 01, section 3) is kept as long as there may still be
an older value for that key in a table not part of this merge; once the merge includes the last (and
oldest) place that key could live, the tombstone and the data it shadows can both be dropped.

**The tombstone-correctness hazard.** Dropping a tombstone too early is a correctness bug, not just
an inefficiency: if a tombstone is removed while an older value for the same key still exists in a
deeper level, that older, deleted value becomes visible again. The deleted data is resurrected. So a
tombstone may only be discarded by a compaction that includes the deepest level where the key could
appear. This constraint shapes which compactions are allowed to reclaim deletes, and it is the
starting point of the Lethe work [Sarkar20Lethe].

**Triggers.** A compaction is started when a level violates an invariant: it has too many bytes
(leveling) or too many runs (tiering). The choice of which files within the level to compact, and how
much to compact at once, is the policy.

**Atomic installation and the manifest.** A compaction replaces a set of input tables with a set of
output tables. This replacement must be atomic with respect to readers and to crashes: a reader must
see either the old set or the new set, never a mixture, and a crash in the middle must recover to one
consistent set. The standard mechanism is a **manifest**: an append-only log of version edits, where
each edit records "add table X at level L" or "remove table Y". The current set of tables is the
result of replaying the manifest. Installing a compaction is then a single durable append of one edit
that adds the outputs and removes the inputs. LevelDB and RocksDB both work this way. The manifest is
shared ground with chapter 12 (crash consistency), because it is the thing that makes multi-file
changes crash-safe.

**Back-pressure and write stalls.** Compaction competes with foreground writes for disk bandwidth. If
writes outrun compaction, level 0 grows, reads slow down, and eventually the engine must slow or
pause writes to let compaction catch up: a **write stall**. Scheduling compaction so that it keeps up
without causing latency spikes is itself a research topic [Balmau19SILK].

## 3. Design space

| Policy | Write amp | Read amp | Space amp | Notes |
|--------|-----------|----------|-----------|-------|
| Leveling (LevelDB/RocksDB) | High O(T·L) | Low | Low ~1 | One run per level; merges eagerly |
| Tiering / size-tiered (Cassandra) | Low O(L) | High | High O(T) | Several runs per level; merges lazily |
| Lazy leveling (Dostoevsky) | Low | Low on point reads | Medium | Tier small levels, level the last |
| Universal (RocksDB) | Medium | Medium | Medium | Merges by size ratios across all runs |
| FIFO / TTL | Lowest | N/A | Lowest | Drop oldest; only for time-series/cache data |

The recommended default is **leveling**, because it gives low read and space amplification, which
matches a general-purpose store, and because it is the policy the filter work in chapter 06 (Monkey)
is most directly analyzed against. Lazy leveling is the first variation to try once leveling works,
because it keeps the read and space behavior while cutting write amplification. The policy should be
a configuration choice, not a hard-coded constant, exactly as chapter 21 argues.

## 4. Notable researchers and key papers

- Patrick O'Neil and colleagues — the original LSM-tree and the level idea [ONeil96].
- Sanjay Ghemawat and Jeff Dean — LevelDB, the reference leveled implementation and the
  manifest/version-edit design.
- Siying Dong and colleagues — RocksDB, including the study of space amplification and the choice of
  compaction styles [Dong17RocksDB].
- Niv Dayan and Stratos Idreos — Dostoevsky and lazy leveling [Dayan18Dostoevsky].
- Subhadeep Sarkar, Tarikul Islam Papon, Dimitris Staratzis, Manos Athanassoulis — Lethe, delete-
  and tombstone-aware compaction with retention guarantees [Sarkar20Lethe].
- Oana Balmau and colleagues — SILK, scheduling compaction and flushing to prevent latency spikes
  [Balmau19SILK].

## 5. Concrete design for this codebase

Three new components, mapping onto the `next.txt` sketch.

**A manifest.** A new type, `manifest`, owning an append-only file (reuse
`src/core/fs.hpp:append_only_file`) of version edits. Each edit is a small encoded record:
`add(level, segment_id, smallest_key, largest_key, file_size)` or `remove(level, segment_id)`. On
startup the manifest is replayed to reconstruct the current set of segments per level. Installing a
flush or a compaction is one `append` of an edit followed by one `fdatasync` (chapter 04's
`commit()` applies here). The manifest also subsumes the engine's current ad-hoc `sstable_id_`
counter (chapter 01, section 1), which moves out of the engine as `TASKS.md` section F notes.

**A segments manager.** A new type, `segments_mgr`, holding the in-memory level structure: for each
level, an ordered collection of `segment` handles (the struct in `src/storage/sstable_reader.hpp`).
It answers "which segments could contain key K, in newest-to-oldest order" for the read path
(chapter 11), and "which segments should be compacted next" for the compactor. It uses
`segment::reference_count_` to keep a segment alive while a read or compaction is using it, so that a
completed compaction does not delete a file out from under an in-flight reader.

**A compactor.** A function `compact(inputs, output_level)` that builds a merging iterator (chapter
11) over the input segments, streams it through an `sstable_writer` (reusing
`src/storage/sstable_writer.cpp`), applies the drop rules from section 2 (keep highest sequence per
key; drop tombstones only when the deepest level for the key is in the merge), writes the outputs,
and installs the result with one manifest edit. It runs as background work; in the single-threaded
model (chapter 13) it is driven cooperatively by the main loop between batches of foreground work, or
on a dedicated I/O-bound thread once chapter 13 chooses an execution model.

Proposed signatures:

```
// manifest.hpp
std::expected<manifest, core::status> open(std::filesystem::path path) noexcept;
std::expected<void, core::status>     apply(const version_edit &edit) noexcept; // append + commit
std::vector<segment_meta>             segments_at(std::uint32_t level) const noexcept;

// compactor.hpp
std::expected<compaction_result, core::status>
compact(std::span<segment_handle> inputs, std::uint32_t output_level, const drop_policy &policy) noexcept;
```

The drop policy is where snapshots (chapter 11) couple in: a version may only be dropped if no live
snapshot can still see it, so `drop_policy` carries the oldest live snapshot sequence number, and the
compactor keeps any version at or above it.

## 6. C++, memory, and concurrency mechanics

The k-way merge needs a priority structure over the input cursors. The two standard choices are a
binary heap (`std::priority_queue` of cursors keyed by current key) and a tournament/loser tree; the
loser tree does fewer comparisons per element and is worth trying once a benchmark shows the merge is
hot, but the heap is the correct first implementation. Comparisons use the existing SIMD comparator
(`src/core/simd.hpp`) on the user-key portion of the internal key.

Memory during a merge is bounded: one block per input held at a time, plus the output block being
built, all arena-allocated (chapter 01, section 6). The compactor should use its own arena that it
resets per output block, exactly as `sstable_writer` already resets its block arena in
`record_data_block`.

The reference counting on segments is the one place concurrency intrudes even before the engine is
multi-threaded, because a background compactor and a foreground reader may touch the same segment. If
compaction runs on the main loop (no other thread), the count can be a plain integer. If it runs on
another thread, the count must be `std::atomic` with acquire/release ordering on the increments and
decrements, and the delete-when-zero step must be ordered after the last use (chapter 13 explains
why). Designing the count as atomic from the start costs nothing on the single-threaded path and
avoids a later rewrite.

The manifest append must be made durable with the same `commit()` discipline as the WAL (chapter
04), and recovery must tolerate a torn final edit (chapter 12): a half-written trailing edit is
discarded, and the last fully durable version is the truth. This is why the manifest is an append-only
log of edits rather than a rewritten snapshot file: appends are easy to make crash-safe, whole-file
rewrites are not.

## 7. Risks, alternatives, and interactions

The largest risk is the tombstone-correctness hazard in section 2; the mitigation is the explicit
drop rule and a differential test (chapter 19) that checks deleted keys never reappear after
compaction. The second risk is the transient space spike during a compaction, which needs free disk
headroom on the order of the size of the data being merged; the engine must account for this so it
does not run the disk out of space mid-merge. The third risk is write stalls; the mitigation is rate
control and scheduling along the lines of SILK [Balmau19SILK].

Interactions are heavy: this chapter needs the merging iterator and snapshot rules from chapter 11,
shares the manifest with chapter 12, is the precondition for the per-level filter sizing in chapter
06 (Monkey needs levels to allocate across), benefits from the asynchronous I/O in chapter 04, and is
the structure that chapter 21's design continuum parameterizes.

A simpler fallback, if full leveling is too much at once, is a minimal "level 0 to level 1"
compaction that merges all of level 0 into a single level-1 run whenever level 0 has too many files.
That alone bounds read amplification for the common case and exercises the manifest, the merging
iterator, and the drop rules, after which generalizing to more levels and other policies is
incremental.

## 8. Experiment plan

**Hypothesis.** Leveling keeps read and space amplification low and bounded as data grows, at a known
write-amplification cost; tiering and lazy leveling move along the predicted RUM tradeoff.

**Setup.** Engine-level driver (chapter 03). Load far more data than memory so levels actually form,
then run mixed workloads (YCSB-A write-heavy and YCSB-B read-heavy), reported per the chapter 03
output contract, in both cached and uncached regimes.

**Baseline.** The current no-compaction behavior (which will show read and space amplification
climbing without bound; this is the control that demonstrates the problem).

**Variants.** Leveling, tiering, and lazy leveling, each across a couple of size ratios `T`.

**Metrics.** The three amplifications over time; sustained throughput; and p99 write latency to
detect write stalls.

**Success criteria.** Read and space amplification stay bounded under leveling as the dataset grows;
the policies order as the theory in chapter 02 predicts; no unbounded write stalls under a steady
write rate the device can sustain.

**Killer result.** If, for the target workload and hardware, the no-compaction read path is already
fast enough because the working set fits in cache, then compaction's value is only in space
reclamation, and the cheaper FIFO/space-only policy may suffice. The experiment's cached-versus-
uncached split is what reveals this.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section K (Compaction and levels) and section F (manifest, config).

1. Implement the manifest (append-only version edits, replay on startup, durable `apply`), and move
   `sstable_id_` and the path derivation out of the engine into it.
2. Implement `segments_mgr`: the in-memory per-level segment set, the newest-to-oldest lookup for
   reads, and the candidate selection for compaction; wire `segment::reference_count_`.
3. Implement the merging iterator (chapter 11) and the compactor with the section 2 drop rules,
   threading the oldest-live-snapshot sequence number through `drop_policy`.
4. Start with the minimal level-0-to-level-1 compaction (section 7 fallback), then generalize to
   leveling, then add tiering and lazy leveling behind a config knob.
5. Add the tombstone-resurrection differential test (chapter 19).
6. Run the experiment in section 8 and record results here.
