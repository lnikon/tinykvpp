# 08 — Memtable data structures

Depends on: chapter 01 (the skiplist memtable and its arena), chapter 02 (the cache-aware idea),
chapter 03 (the in-memory micro-benchmarks), chapter 13 (concurrency, referenced for the concurrent
variants). Follows the nine-section depth template. This is the write-path counterpart to chapter 07:
chapter 07 is about the on-disk index, this is about the in-memory table.

## 1. Problem statement

frankie's memtable is a skiplist backed by an arena (`src/storage/skiplist.hpp`,
`src/storage/skiplist.cpp`, `src/storage/memtable.cpp`). It is single-threaded, it stores each node
as one flat arena allocation `[header][forward pointers][key][value]`, and it orders entries by the
user-key portion of the internal key using the SIMD comparator. It works and is reasonably
cache-conscious. The open question is whether a different structure would serve the write path better,
on three axes the skiplist is known to be middling at: cache behavior under point lookups, scan
locality, and the path to concurrency.

This is exploratory, not a defect. The skiplist is a fine default. The chapter exists because the
memtable is on the hottest path (every write and many reads touch it), and because the project already
has the micro-benchmarks (`test/skiplist_benchmark.cpp`, `test/memtable_benchmark.cpp`) to settle the
question with data rather than opinion.

## 2. Background and theory

A memtable must support four operations well: insert a key-value pair, look up a key, iterate in
sorted order (for flushing and for scans, chapter 11), and report its size so the engine knows when to
flush (chapter 01, section 5). It must do all of this against an arena allocator that cannot free
individual entries, and it should have a credible path to being made concurrent later (chapter 13).

**Skiplist** [Pugh90]. A linked list with several levels of forward pointers, where each node's height
is chosen randomly. Search drops from the top level, skipping ahead, giving expected `O(log n)` search
and insert with no rebalancing. Its weakness is that each level-step follows a pointer to a
far-apart node, so a search incurs several cache misses; the flat node layout in this codebase
mitigates the per-node cost but not the inter-node jumps. Its strength is simplicity and an easy
concurrent version.

**B+-tree.** A balanced tree whose nodes are sized to a cache line (or a page) and hold many keys
each, so a search touches few cache lines. It is `O(log n)` with a small constant and excellent scan
locality because leaves are contiguous and linked. Its cost is rebalancing: inserts split nodes, which
in an arena that cannot free means either tolerating dead nodes or using a compacting arena. The
cache-conscious variants (CSB+-tree, Rao and Ross) reduce pointer overhead further.

**Adaptive radix tree (ART)** [Leis13ART]. A trie keyed by successive bytes of the key rather than by
comparisons. It uses no key comparisons at all; it walks down the key's bytes. Its cleverness is that
each internal node adapts its representation to how many children it has (small arrays for few
children, up to a full 256-slot array for many), keeping it compact and cache-friendly, and it
path-compresses long single-child chains. For string keys, which is exactly what this store has, ART
is often the fastest in-memory ordered index, because it replaces `log n` cache-missing comparisons
with a short walk over the key's bytes. Its sensitivity is to key length and distribution: very long
keys mean deeper walks.

**Masstree** [Mao12Masstree]. A trie of B-trees, designed from the start for concurrency on multicore
machines. It is the structure to study if the chosen execution model (chapter 13) is shared-memory
multithreading rather than thread-per-core.

**Hash-skiplist hybrid.** RocksDB offers a memtable that hashes on a key prefix and keeps a small
skiplist per bucket, which is fast for workloads whose scans are bounded within a prefix. It trades
general ordered scans for speed on prefixed access.

The fundamental split is comparison-based structures (skiplist, B+-tree) versus radix structures
(ART), and the second axis is the concurrency story, which chapter 13 governs.

## 3. Design space

| Structure | Insert | Point get | Scan locality | Cache behavior | Concurrency | Arena fit |
|-----------|--------|-----------|---------------|----------------|-------------|-----------|
| Skiplist (today) | O(log n) | O(log n) | Medium | Medium (pointer jumps) | Easy lock-free | Good (no frees) |
| B+-tree | O(log n) | O(log n) | Excellent | Good | Harder | Needs splits/frees |
| ART | O(k) in key length | O(k) | Good | Good (no comparisons) | Medium (ROWEX) | Good |
| Masstree | O(k) | O(k) | Good | Good | Built-in | Specialized |
| Hash-skiplist | O(1)+ | O(1)+ | Poor (only in-prefix) | Good | Easy | Good |

The recommended exploration order: keep the skiplist as the default and baseline; implement an
arena-backed **ART** as the main contender, because the workload is string-keyed and ART is the
strongest candidate there; consider B+-tree only if scan locality turns out to dominate; defer
Masstree to whenever chapter 13 selects shared-memory concurrency.

## 4. Notable researchers and key papers

- William Pugh — skip lists [Pugh90].
- Viktor Leis, Alfons Kemper, Thomas Neumann — the adaptive radix tree [Leis13ART], and its
  concurrency extension ROWEX.
- Yandong Mao, Eddie Kohler, Robert Morris — Masstree [Mao12Masstree].
- Jun Rao, Kenneth Ross — cache-conscious B+-trees (CSB+-tree) [RaoRoss00].

## 5. Concrete design for this codebase

Mirror chapter 07: make the structure swappable behind a compile-time concept, so the engine can be
built with any memtable and the benchmark can compare them on equal footing.

```
template <typename M>
concept MemtableIndex = requires(M m, const M cm, std::string_view ikey, std::string_view value,
                                 core::arena *arena) {
  { m.insert(ikey, value) };
  { cm.get(ikey) }            -> std::same_as<std::expected<std::pair<std::string_view, std::string_view>, error>>;
  { cm.bytes_allocated() }    -> std::same_as<std::uint64_t>;
  { m.rebind_arena(arena) };
  { cm.begin() }; { cm.end() };
};
```

The existing skiplist already satisfies almost all of this (`src/storage/skiplist.hpp`), so the first
step is to factor that concept out and confirm the skiplist models it. Then implement
`art_memtable` as a second model: an arena-backed adaptive radix tree whose nodes are flat arena
allocations, following the same placement-new, pointer-arithmetic style as
`src/storage/skiplist.cpp:skiplist_node::create`.

One subtlety connects to chapter 11. The skiplist orders by the user-key portion and keeps only the
latest version per key (chapter 01, section 3). ART keys are the raw key bytes, so an ART memtable
must decide how to represent versions: either key on the user key and store the single latest version
(matching today's behavior) or key on the full internal key (user key followed by an order-preserving
encoding of the sequence number) to keep multiple versions, which is what real MVCC (chapter 11)
wants. This choice must be made consistently with chapter 11.

Because the memtable is one per engine, runtime selection can be a simple config switch plus a
template instantiation, or a `std::variant` as in chapter 07 if multiple kinds must coexist; the
template form is simplest here.

## 6. C++, memory, and concurrency mechanics

The model to copy is the skiplist's flat node: one arena allocation per node, fields and variable-
length data laid out contiguously, accessed by pointer arithmetic from `this`
(`src/storage/skiplist.cpp`). ART's adaptive nodes (the 4-, 16-, 48-, and 256-child variants) are each
a fixed-size flat allocation, which suits the arena well, and growing a node from one variant to a
larger one is an allocate-and-copy, leaving the old node dead in the arena, which is acceptable
because the arena frees everything at flush time.

Any new structure must preserve the move-and-rebind discipline from chapter 01, section 6: the
structure stores a raw pointer to the memtable's arena, and the memtable's move operations call
`rebind_arena` so the pointer survives the move. This is easy to forget and is exactly the kind of
detail that a concept cannot enforce, so it belongs in the structure's tests.

Concurrency is the axis with the largest consequences and is deferred to chapter 13, but the choice
of structure constrains it. A skiplist has a well-known lock-free form. ART has a concurrency
extension (ROWEX). A B+-tree is harder to make concurrent. If chapter 13 chooses thread-per-core
sharding instead, then each core owns a single-threaded memtable and no concurrent structure is needed
at all, which is the simplest outcome and an argument for keeping the structure single-threaded and
sharding above it. The `std::memory_order` discussion that any lock-free version needs is in chapter
13.

## 7. Risks, alternatives, and interactions

ART's risk is sensitivity to key length and distribution; pathological keys (very long, or sharing
huge prefixes) make it deep, and the experiment must include such keys. B+-tree's risk is the split
machinery against a non-freeing arena. The overarching risk is spending effort here when the memtable
is not the bottleneck; the existing benchmarks make that cheap to check first.

The fallback is the skiplist, which is already implemented, correct, and adequate. This chapter is a
conditional improvement gated on measurement.

Interactions: the version-representation choice couples to chapter 11 (MVCC), the concurrency choice
to chapter 13, and the ordered-iteration requirement serves both flushing (chapter 05) and scans
(chapter 11).

## 8. Experiment plan

**Hypothesis.** For this store's string keys, ART beats the skiplist on point lookups and inserts in
the cached regime (the only regime a memtable has, since it is in memory), while the skiplist remains
competitive on ordered scans and is simpler to make concurrent.

**Setup.** Extend `test/memtable_benchmark.cpp` and `test/skiplist_benchmark.cpp` to drive insert,
point-get, and ordered-scan against each structure, varying key length (short, medium, long) and key
distribution (uniform, Zipfian, shared-prefix). The memtable is in memory, so this is purely the
cached regime.

**Baseline.** The current skiplist.

**Variants.** ART; optionally B+-tree; hash-skiplist for the prefix-scan workload.

**Metrics.** Operations per second for each operation; cache misses per operation (via `perf`); and
memory overhead per entry.

**Success criteria.** A clear ordering of the structures per operation and key shape, with a
recommendation of which to make the default for the project's expected key distribution.

**Killer result.** If the skiplist is within a small margin of ART across the realistic key shapes,
then the added complexity of a second structure is not justified and the skiplist stays, with effort
redirected to the concurrency question (chapter 13), which matters more.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section O (Memtable structures).

1. Factor out the `MemtableIndex` concept and confirm the skiplist models it.
2. Implement an arena-backed ART satisfying the concept, preserving the move-and-rebind discipline.
3. Decide the version representation jointly with chapter 11 (latest-only versus full internal key).
4. Extend the existing benchmarks to compare structures across key length and distribution.
5. Run the experiment in section 8 and record the recommendation here.
