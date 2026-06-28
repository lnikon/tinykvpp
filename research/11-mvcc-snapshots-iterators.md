# 11 — Multi-version concurrency control, snapshots, and iterators

Depends on: chapter 01 (the sequence number, the comparator, the scan stub), chapter 05 (the merging
iterator and the version-drop rules are shared), chapter 06 (range filters help scans), chapter 03
(measurement). Follows the nine-section depth template.

## 1. Problem statement

frankie cannot do a consistent scan, and it cannot give a reader a stable view of the database. Three
facts in the current code show why.

First, `src/engine/engine.cpp:engine::scan` is a stub that ignores its arguments.

Second, the memtable keeps only the latest version of each user key. The comparator
`src/storage/memtable.hpp:internal_key_comparator` strips the sequence, timestamp, and tombstone
metadata and compares only the user-key bytes, so two writes of the same user key compare as equal and
the skiplist's insert replaces the old node (chapter 01, section 3). There is no second version to
show an older reader.

Third, although every entry already carries a sequence number (`engine::sequence_`, stored in the
internal key) and the `segment` struct already has a `reference_count_` for pinning files in use, none
of this is used to provide a snapshot.

So a read sees only the latest value, and a long scan would see writes that happened after it started,
which is not a consistent view.

## 2. Background and theory

**Multi-version concurrency control** (MVCC), introduced by Reed [Reed78] and developed by Bernstein
and Goodman [Bernstein83], keeps several versions of each key, each tagged with the sequence number of
the write that produced it. A read is performed *as of* a sequence number `S`: it returns, for each
key, the newest version whose sequence number is less than or equal to `S`. Because older versions are
not destroyed by newer writes, a reader holding `S` is unaffected by writes that happen after it, so it
sees a stable view. That stable view is a **snapshot**, and reading consistently from it is **snapshot
isolation** [Berenson95].

To make this efficient, the on-disk and in-memory ordering must change. Internal keys should be
ordered by user key ascending and, for equal user keys, by sequence number **descending**. Then all
versions of one key are adjacent, with the newest first. A read as of `S` seeks to the user key and
walks forward to the first version with sequence at most `S`. A forward scan over the whole structure
visits each user key's versions newest-first, so it can emit the first visible one and skip the rest.
This is exactly the ordering LevelDB and RocksDB use.

A read, whether a point `get` or a scan, must combine several sorted sources: the active memtable, the
immutable memtable, and the SSTables at every level. The tool is a **merging iterator**: it holds a
cursor into each source and repeatedly yields the smallest key across them, and when the same user key
appears in several sources it prefers the version from the newest source (and the highest sequence
number), then skips the shadowed older versions and any tombstone. This is the same merging iterator
that compaction uses (chapter 05); the only differences are that a read stops at the snapshot sequence
and does not write its output.

Old versions cannot be kept forever, or space amplification grows without bound. They are reclaimed by
compaction (chapter 05), but with a constraint that ties the two chapters together: a version may be
dropped only if no live snapshot can still see it. So the engine tracks the oldest sequence number any
live snapshot holds, and compaction keeps every version at or above it. This is the `drop_policy`
input chapter 05 referred to.

## 3. Design space

| Approach | Consistency | Version retention | Scan support | Complexity |
|----------|-------------|-------------------|--------------|------------|
| Latest-only (today) | None (last write wins) | One per key | None | Lowest |
| Merging iterator, no snapshot | Reads latest committed | One per key on read | Yes, but not stable | Low |
| Full MVCC + snapshots | Snapshot isolation | Multiple per key | Yes, stable | Medium |
| Copy-on-read snapshot | Snapshot isolation | Copies pinned data | Yes | High memory |

The recommended path is full MVCC with snapshots, because it is the design that makes both consistent
scans and consistent point reads possible and it is the industry-standard LSM approach; but a useful
intermediate, a merging iterator without snapshots, delivers scans of the latest committed data first
and can ship before the comparator change.

## 4. Notable researchers and key papers

- David P. Reed — the original multi-version concurrency control [Reed78].
- Philip Bernstein, Nathan Goodman — the theory of multiversion concurrency control [Bernstein83].
- Hal Berenson, Phil Bernstein, Jim Gray, Jim Melton, Elizabeth O'Neil, Patrick O'Neil — the
  definition and critique of snapshot isolation [Berenson95].
- Per-Ake Larson and colleagues — high-performance MVCC in main-memory databases (Hekaton)
  [Larson11].

## 5. Concrete design for this codebase

**Change the ordering.** Modify `internal_key_comparator` so that it compares the user key ascending
and, on a tie, the sequence number descending, instead of ignoring the metadata. This single change
turns the memtable from latest-only into multi-version: two writes of the same user key now compare as
different (different sequence), so both are kept, newest first.

**Snapshots.** Add a snapshot registry to the engine. Acquiring a snapshot captures the current
sequence number and registers it; releasing it removes it. The engine exposes:

```
// engine.hpp
snapshot     get_snapshot() noexcept;                 // captures current sequence, pins it
void         release_snapshot(snapshot) noexcept;
std::expected<std::string_view, core::status> get(std::string_view key, snapshot snap) noexcept;
iterator     scan(std::string_view start, std::string_view end, snapshot snap) noexcept;
```

The plain `get` becomes the special case of reading at the latest sequence. The registry exposes the
oldest live snapshot sequence to compaction's `drop_policy` (chapter 05).

**The merging iterator.** Implement an iterator over the active memtable, the immutable memtable, and
the segments from `segments_mgr` (chapter 05), using a heap keyed by (user key ascending, sequence
descending, source recency). For each user key it yields the newest version with sequence at most the
snapshot's, then advances past the rest of that key's versions, skipping a tombstone by returning
nothing for that key. `engine::scan` builds this iterator bounded by `[start, end)`.

**Read path now reaches disk.** With the merging iterator in place, `engine::get` finally consults
SSTables (closing the chapter 01, section 4 gap), because the point read is just a merging read that
stops at the first matching user key.

**Iterator pins its sources.** While an iterator or snapshot is live, the segments it reads are pinned
through `segment::reference_count_` so compaction does not delete them underneath it.

## 6. C++, memory, and concurrency mechanics

The comparator change is small in code but ripples through behavior, so it is the riskiest single edit
in Part II. The memtable now holds multiple versions, so it uses more memory and flushes sooner; the
flush (chapter 01, section 5) must apply the same version rules as a read (keep what snapshots can see,
drop the rest). Both must be covered by tests.

The sequence-descending order needs care in encoding. The internal key today stores the sequence as
raw bytes (`src/storage/memtable.cpp:internal_key::encode`) and the comparator is a custom function, so
the cleanest approach is to keep comparing the sequence numerically in the comparator (decode the eight
bytes and compare with the sense reversed) rather than relying on byte order. If any code path ever
compares internal keys by raw `memcmp` (for example a future radix memtable, chapter 08), then the
sequence must instead be stored in an order-preserving form, which for descending order means storing
`MAX_SEQUENCE - sequence` in big-endian; this is a concrete constraint chapter 08 must honor.

The snapshot registry and the segment reference counts are the first place ordinary reads and
background compaction share mutable state. On the single-threaded path they are plain integers. Once a
background compaction thread exists (chapter 13), the reference counts become `std::atomic` with
acquire/release ordering, and the "drop version only if below oldest live snapshot" check must read a
consistently published oldest-snapshot value; chapter 13 explains the memory-ordering requirement.

Iterator lifetime is an ownership problem: the iterator holds `string_view`s into memtable nodes and
into segment blocks, all of which must stay alive and unmoved for the iterator's life. The memtable
arena and the pinned segments provide that stability; the scratch arena (with its `realloc` move
hazard) must not back anything an iterator holds across steps.

## 7. Risks, alternatives, and interactions

The first risk is version bloat: a long-held snapshot prevents compaction from reclaiming any version
newer than it, so memory and disk grow until it is released. The engine should expose, and ideally
bound, the age of the oldest snapshot. The second risk is the correctness of the tombstone-plus-
snapshot interaction: a tombstone that a snapshot can still see must not be dropped, or that snapshot
would suddenly see a resurrected value. The third risk is scan cost: a scan over many sources is slow
without help, which is where range filters (chapter 06) and good block layout (chapter 09) matter.

The fallback is the no-snapshot merging iterator: it gives range scans of the latest committed data
without the comparator change or the registry, and full snapshots can be layered on afterward.

Interactions: this chapter shares the merging iterator and the version-drop rule with chapter 05,
finally connects `engine::get` to SSTables (chapter 01), benefits from chapter 06's range filters, and
is the point where the concurrency model of chapter 13 first becomes visible through shared
reference counts.

## 8. Experiment plan

**Hypothesis.** Sequence-ordered MVCC provides correct snapshot isolation (a snapshot never sees a
write newer than itself, and never sees a key it deleted resurrected), at a memory cost proportional to
how long snapshots are held, and supports range scans whose throughput is acceptable.

**Setup.** Engine-level driver (chapter 03) plus the differential test harness (chapter 19): run the
engine and a reference model (an ordered map of versioned entries) under the same randomized sequence
of writes, snapshots, point reads, and scans, and assert identical results.

**Baseline.** Today's latest-only behavior for the non-snapshot operations (to confirm no regression
on simple reads).

**Variants.** Snapshot reads under concurrent writes; long-held versus short-held snapshots; scans of
varying range width.

**Metrics.** Correctness (the differential test must pass for all seeds); scan throughput (entries per
second, cached and uncached); and version-retention memory and disk over time as a function of snapshot
age.

**Success criteria.** The differential test passes for all seeds, including deletes and overwrites
under live snapshots; scan throughput is acceptable for the target workload; version retention is
bounded when snapshots are released promptly.

**Killer result.** If the workload never needs consistent scans or stable reads, then the simpler
no-snapshot merging iterator suffices and full MVCC is not worth its memory and complexity.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section D (implement scan) and section Q (MVCC / snapshots).

1. Change `internal_key_comparator` to order by user key ascending, sequence descending; update the
   flush to apply the version rules.
2. Implement the merging iterator over memtable, immutable, and segments; first as a no-snapshot
   latest-committed iterator.
3. Add the snapshot registry and the snapshot-aware `get`/`scan`; expose the oldest live snapshot to
   compaction's `drop_policy` (chapter 05).
4. Pin segments under live iterators and snapshots via `segment::reference_count_`.
5. Build the differential test (chapter 19) and run the experiment in section 8; record results here.
