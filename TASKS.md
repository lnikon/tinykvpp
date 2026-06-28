# Tasks

Suggested order: A → B → D → C → E/F/G → H. Group A unblocks the SSTable reader currently
in progress, and its encode/decode work pulls the strong-typing task (B) in for free.

> Deep research write-ups — theory, prior art, C++/memory/concurrency design, and experiment
> plans — live in `RESEARCH.md` and `research/`. Each research section below links its chapter.

## +>. Immediate.
- [ ] Refactor buffer_reader/buffer_writer.

## A. Correctness — SSTable read path & wire format

- [ ] Fix footer round-trip: field order (writer `[size][offset]` vs reader `[offset][size]`),
      20-vs-8 byte size mismatch, and 12 uninitialized bytes flushed to disk
- [ ] One source of truth for the wire format: `sstable_footer::encode/decode` + index-entry
      `encode/decode` (mirror `internal_key`) — folds in strong typing and kills the drift/bug class
- [ ] `segment::create`: make it a `static` factory, fix `footer_.index_size_` → `sst_footer.index_size_`,
      honor the short-read contract, return the correct `.error()` object
- [ ] Resolve `segment` vs empty `sstable_reader` ownership (one parser, one handle)
- [ ] `engine::get` must consult SSTables after flush (extends rotation work in D)
- [ ] Reconcile sstable_format.hpp doc with actual encoding (varint key len; emit `entry_count`)
- [ ] Round-trip (write→read) + truncated/corrupt-buffer fuzz tests

## B. Error model

- [ ] Merge `serialization_error_k` into `core::status` — removes lossy translation at every boundary
- [ ] Strong typing to stop arg swaps: `byte_offset`/`byte_count`, key-view vs value-view
      (a strong offset/size type would have made the footer swap uncompilable)
- [ ] Contract policy: assert = programmer error, status = IO/format/runtime;
      add `static_assert(sizeof(sstable_footer) == …)`

## C. Memory / arena

- [ ] Arena OOM null-checks in `create`/`allocate` (`aligned_alloc` unchecked → null deref);
      then the call-site `out_of_memory` branches become live instead of dead code
- [ ] Decide arena.cpp commented `default_block_size_ = capacity`; document the regrow-leak in `append`
- [ ] One arena per engine OR `<arena, memtable>` pairs?
- [ ] Shared arena between memtable and skiplist
- [ ] Shared linked-list allocator per-engine. Memtables allocate a whole node from it.
- [ ] Memory leak tracing allocator

## D. Engine / flush / rotation

- [ ] Move flush orchestration out of `engine` into a writer-owned `flush(iter, file)`;
      dedupe the two near-identical flush blocks
- [ ] Finish rotation lifecycle: reset immutable after flush, define when an SSTable replaces it (in progress)
- [ ] `maybe_rotate_memtable` path build: detect snprintf truncation, fix `%lu` portability
- [ ] Implement `scan` (currently a stub)
- [ ] Monotonic clock for timestamp — field exists but is never set

## E. Conventions & boilerplate

- [ ] Code style document (function, class, file, dir naming, organization): pick one enum-naming
      convention (`_k` vs `k` vs none) and one factory-vs-ctor rule (unify `buffer_reader`/`buffer_writer`)
- [ ] Macros for rule-of-six to dedupe the move-only boilerplate
- [ ] Finish auditing copy/move semantics across all classes

## F. Config

- [ ] Extend centralized config: derive paths from `root_dir_path_`, add a manifest,
      move `sstable_file_prefix_`/`sstable_id_` out of the engine (in progress)

## G. Observability & integrity

- [ ] Logging module — replace scattered `std::println` with a logging seam
- [ ] OpenTelemetry support (superset of the logging module)
- [ ] Wire up the dead `crc32_` header/footer fields
- [ ] Document the concurrency model (single-threaded?) — currently unstated

## H. Build

- [ ] Update build section with mandatory/optional steps; optimize build duration;
      build tests only when `BUILD_TESTS` is set, same for benchmarks
- [ ] Keep CMake clean. `-Wsign-conversion` breaks simd.h; introduce per-profile flags
      so `-O3 -g -fno-omit-frame-pointer` go into RelWithDebugInfo

## I. Research — index structures (future)

> Deep dive: `research/07-block-index.md`.

Context: the block index is a *sparse* index — one `index_entry` per data block (smallest key →
`block_offset`/`block_size`), not per key. Current plan: sorted `std::span<index_entry>` +
`lower_bound` (O(log n), one arena alloc, zero key copies). That is the right default; everything
below is exploratory and gated on benchmarks showing index lookup is actually hot.

### Can lookups be O(1) on already-allocated (arena) memory?

- Yes for **exact-match**, with no per-node malloc and keys kept as `string_view`s into the raw block:
  - Flat open-addressing hash table (linear probing / Robin Hood) — one contiguous slot array, O(1) avg.
  - Minimal perfect hash (CHD / BBHash / PTHash) — SSTables are immutable, so all keys are known at
    build time → collision-free, one slot per key, true O(1) worst case, most compact.
- **But this does not fit the block-index query.** `get(key)` needs the block whose range *contains*
  the key = predecessor (greatest index key ≤ target). Hashing destroys order → no predecessor.
  Making hash work would require a *dense* index (one entry per key), which bloats the index and
  defeats the block-based design.
- Better-than-log while keeping order (all in-place in one arena buffer; none guaranteed O(1)):
  - Eytzinger (BFS/heap) layout — still O(log n) but branch-free + cache-optimal, ~2–4× faster than
    sorted binary search. Cheapest real win; same predecessor query, just a different fill order.
  - Interpolation search — O(log log n) avg on ~uniform keys, O(n) worst case, zero extra memory.
  - Learned index (piecewise-linear / RMI) — O(1) position predict + bounded local search;
    approximate, worst case bounded by model error. Coefficients + sorted array, both in the arena.
- Reserve perfect hashing for genuinely exact-match, immutable-key uses (filters, point-lookup-only
  structures), not the block index.

### Tasks

- [ ] Benchmark first. Micro-bench index lookup (sorted binary search vs Eytzinger vs interpolation)
      across realistic entry counts, measured with the subsequent block read both cached and uncached,
      to confirm whether index lookup is ever the bottleneck. Do not optimize until it is.
- [ ] Non-virtual uniform index interface so strategies are swappable at zero runtime cost: a
      `concept` (e.g. `BlockIndex`) constraining `find(key) -> std::optional<index_entry>`, build/
      construct-from-arena, and ordered iteration for scans. Compile-time polymorphism (concept-checked
      template or CRTP), not vtables.
- [ ] Bridge runtime selection without virtual dispatch: hold the chosen index as a closed-set
      `std::variant<sorted, eytzinger, …>` + `std::visit` — keeps the config knob below while staying
      non-virtual.
- [ ] Config knob `config.index_kind_` (sorted | eytzinger | interpolation | hash | learned),
      threaded through `sstable_writer`/reader. Default = sorted. Persist the chosen kind in the
      footer/manifest so each segment is read back with the structure it was written with.
- [ ] Gate predecessor-incapable strategies (hash, MPHF) to a dense/exact-match index variant only,
      so they cannot be selected for the sparse block index.

## J. Benchmark harness (prerequisite for the research below)

> Deep dive: `research/03-benchmark-methodology.md`.

Every research section is gated on benchmarks, but there is no engine-level workload driver yet —
only skiplist/memtable micro-benchmarks. Build this first; it unblocks I and K-Q.

- [ ] Engine-level workload generator: read-heavy / write-heavy / mixed, uniform vs Zipfian keys,
      configurable value-size distribution.
- [ ] Report write/read/space amplification and p50/p99 latency.
- [ ] Reuse as the common gate (cached vs uncached, per policy).

Suggested research order: J (harness) -> M (group commit) -> K (compaction) -> L (filters)
-> N/O/P as benchmarks justify. R is longer-term.

## K. Compaction and levels

> Deep dive: `research/05-compaction-and-levels.md`.

Nothing compacts yet; flush drops SSTables into `segments/`. Intent already exists:
`tmp/segments/level_*`, the manifest/`segments_mgr` sketch in `next.txt`, and
`segment::reference_count_` (track in-use segments so compaction does not delete them early).

- [ ] Leveling vs tiering vs lazy leveling (Dostoevsky) — the concrete RUM tradeoff.
- [ ] Delete-aware compaction so tombstones are reclaimed under a retention bound (Lethe).
- [ ] Compaction I/O scheduling to avoid client p99 spikes (SILK).
- [ ] Gate: write/read/space amplification per policy on the J harness.

## L. Filters

> Deep dive: `research/06-filters.md`.

The footer reserves `bloom_offset_`/`bloom_size_` but nothing is built, and `engine::get` does
not probe SSTables yet (see A). SSTables are immutable, so all keys are known at build time — the
same property section I reserves for perfect hashing.

- [ ] Point filters: Bloom, then XOR / binary-fuse and Ribbon (smaller, faster, build-time only).
- [ ] Per-level bit allocation instead of uniform (Monkey). Depends on levels from K.
- [ ] Range filters for scan (SuRF, Rosetta); Bloom does not help ranges.
- [ ] Gate: false-positive disk reads for absent keys, by level depth.

## M. Async I/O and execution model

> Deep dive: `research/04-durability-write-path.md`, `research/13-concurrency-and-cpp-memory-model.md`.

The README's stated design (io_uring, explicit-state-machine loop, "no zoo of threads") is not
built: `fs.hpp` is blocking pread/pwrite, `open_flag::direct` is unused, and `wal_writer::append`
fsyncs on every put/del.

- [ ] WAL group commit: batch ops into one fsync. Cheapest large win. Measure throughput vs
      durability latency vs bounded data-loss window.
- [ ] io_uring for flush/compaction: batched SQEs, registered buffers, O_DIRECT, linked write+fsync.
- [ ] Execution model: single-threaded loop vs thread-per-core / shared-nothing (Seastar/Scylla).
      The skiplist has no atomics, so sharding is more natural than a concurrent skiplist.

## N. Key-value separation

> Deep dive: `research/10-key-value-separation.md`.

`kv_entry` stores values inline and data blocks are `[key][value]`, so compaction rewrites values
repeatedly. WiscKey keeps only keys in the LSM and puts large values in a separate log (a value log
is essentially another `append_only_file`).

- [ ] Inline-vs-vlog size threshold, vlog garbage collection, scan-locality cost.
- [ ] Gate: write amplification vs value-size distribution.

## O. Memtable structures

> Deep dive: `research/08-memtable-structures.md`.

The write-path mirror of section I. The memtable is a single-threaded arena skiplist today;
`skiplist_benchmark.cpp` and `memtable_benchmark.cpp` already exist to extend.

- [ ] Skiplist vs ART (radix, cache-friendly for string keys) vs B+-tree vs Masstree vs
      hash-skiplist (prefix workloads).
- [ ] Gate: insert + point-get + scan throughput across key length and skew.

## P. Block compression and key encoding

> Deep dive: `research/09-compression-and-key-encoding.md`.

`sstable_data_block_header` carries `compression_type`/`uncompressed_size`/`compressed_size` and the
enum has `lz4`, all currently dead.

- [ ] Compression: lz4 vs zstd (with trained dictionaries) vs snappy; block-size tradeoff.
- [ ] Prefix/delta key encoding with restart points (sorted keys per block); the restart array
      doubles as an intra-block index, interacting with section I.
- [ ] Gate: space amplification and decompress latency, cached vs uncached.

## Q. Other near-term directions

> Deep dive: `research/11-mvcc-snapshots-iterators.md` (MVCC), `research/12-integrity-crash-consistency.md`
> (crash consistency + checksums), `research/21-design-continuum.md` (adaptive tuning).

- [ ] MVCC / snapshot isolation for scan. `sequence_` is stamped on every entry and
      `segment::reference_count_` is ready for it. Constraint: `internal_key_comparator` compares user
      keys only, so the memtable keeps only the latest version per key; real snapshots need ordering by
      (user_key asc, sequence desc) and version retention.
- [ ] Crash consistency and the manifest as a version-edit log (LevelDB MANIFEST): atomic multi-file
      install when compaction swaps N inputs for M outputs. Extends F.
- [ ] Checksum algorithm choice once the dead crc32 fields are wired (see G): CRC32C (SSE4.2, SIMD
      already in use) or xxh3; granularity per-block vs per-entry.
- [ ] Robust/adaptive tuning under workload uncertainty (Endure), building on a cost model.

## R. Broader horizons (longer-term)

> Deep dive: `research/13`–`research/21` (Parts III–VII of `RESEARCH.md`): concurrency, distributed
> systems, hardware, capabilities, testing methodology, and the design continuum.

A map of farther-out directions, from the node out to the cluster and the methods. Lower resolution
than the sections above; each is a research program in itself.

- Concurrency and execution: safe memory reclamation (epoch-based, hazard pointers, RCU) as the
  prerequisite for any lock-free structure; C++ coroutines as the unit for the io_uring loop;
  thread-per-core.
- Architecture alternatives to LSM: Bw-tree / B-epsilon tree / FASTER (write-optimized, lock-free);
  COW B-tree + mmap (LMDB) as the counterpoint to explicit I/O + arena; the data-structure design
  continuum (parameterize the structure instead of picking one — generalizes sections I, K, O).
- Distributed system (the stated end goal): replication and consensus (Raft / Paxos / EPaxos, or
  Dynamo-style leaderless quorums); consistency models and the CAP/PACELC tradeoff; partitioning and
  membership (consistent hashing, SWIM/gossip); time and transactions (hybrid logical clocks,
  TrueTime, Calvin, Percolator). The WAL is already a replication log.
- Correctness and methodology (adopt early): deterministic simulation testing (FoundationDB style) —
  `fs.hpp` and `core::status` are ready-made seams for a simulated disk and fault injection; TLA+
  specs for recovery/consensus; differential testing against std::map or RocksDB; Jepsen once replicated.
- Hardware frontier: zoned storage / ZNS SSDs (append-only, immutable SSTables map onto zone append);
  persistent memory / CXL; RDMA and kernel-bypass; computational storage and GPU offload;
  NUMA-awareness and more SIMD.
- Capabilities and access methods: vector / ANN search (HNSW); secondary, spatial, and inverted
  indexes; transactions, TTL, change-data-capture / watch; adaptive indexing (database cracking).
- Cross-cutting: security (encryption at rest, searchable encryption, TEEs); multi-tenancy and QoS; a
  real block cache and eviction policy (ARC, W-TinyLFU, S3-FIFO); allocator design; energy.
- Theory: the external-memory and cache-oblivious models that underpin the cached-vs-uncached
  methodology; online/competitive analysis; RUM lower bounds.

## Done

- [x] random_access_file/append_only_file abstractions
- [x] Support tombstones
- [x] Engine/DB: fat struct keeping arenas, memtables, sequence number, wal, etc. together
- [x] Specify copy/move semantics for classes (consistent rule-of-5; audit pending in E)
- [x] Implement simple memtable rotation (active + immutable; correctness gaps tracked in A & D)
- [x] Put/get/delete in engine interface (scan still a stub, tracked in D)
