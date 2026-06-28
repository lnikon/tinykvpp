# 07 — The block index

Depends on: chapter 01 (the index the writer builds), chapter 02 (the cache-aware/cache-oblivious
distinction and the cached-versus-uncached rule), chapter 03 (the micro-benchmark setup). Follows the
nine-section depth template. This chapter expands `TASKS.md` section I with the full theory, the C++
mechanics, and the experiment.

## 1. Problem statement

Each SSTable has a **block index** that maps a key to the data block that would contain it, so a read
does not scan every block. frankie's index is **sparse**: one entry per data block, holding that
block's smallest key and the block's offset and size. The writer builds it in
`src/storage/sstable_writer.cpp` (the `index_entry` struct and `get_index`), but the reader that
would search it is unimplemented (`src/storage/sstable_reader.cpp`, chapter 01, section 8).

The query the index must answer is a **predecessor query**: given a target key, find the data block
whose key range contains it, which is the block with the greatest smallest-key that is less than or
equal to the target. The current plan, stated in `TASKS.md` section I, is the right default: store the
entries as a sorted array and find the block with `std::lower_bound`, which is `O(log n)` in the
number of blocks, needs one arena allocation, and copies no keys because entries hold
`std::string_view`s into the raw index image.

The open question is whether that `O(log n)` search is ever a bottleneck worth improving, and if so,
with what. This chapter lays out the design space, but it inherits section I's discipline: do not
optimize this until a benchmark shows index search is actually hot, because in most cases the disk
read of the block that follows the search dominates it completely.

## 2. Background and theory

Binary search over a sorted array is `O(log n)` comparisons, but its memory access pattern is poor:
each probe jumps to a far-apart location, and on a large index each jump is likely a cache miss. The
research on in-memory search is largely about keeping the same logarithmic bound while making the
access pattern friendly to the CPU caches. This is the cache-aware versus cache-oblivious distinction
from chapter 02, section 9, applied to the index rather than the disk.

**Eytzinger layout.** Store the sorted keys not in sorted order but in the order of a breadth-first
walk of the balanced binary search tree over them (the root first, then its two children, then their
four children, and so on; this is the same layout an implicit binary heap uses). A search then walks
`index = 2*index` or `2*index + 1`, which is branch-predictable and lets the next few candidate
positions be prefetched, because they are computable in advance. The number of comparisons is still
`log n`, but the constant is much smaller because the cache behaves. Khuong and Morin measured this
carefully and found Eytzinger several times faster than sorted binary search for in-cache data
[KhuongMorin17].

**Interpolation search.** If the keys are roughly uniformly distributed, you can guess the target's
position by linear interpolation between the endpoints instead of always splitting in the middle.
This is `O(log log n)` on uniform data, but degrades to `O(n)` on skewed data, so it is only safe
when the key distribution is known to be benign.

**Learned indexes.** A learned index replaces the search with a model that predicts the target's
position directly, then does a small local search to correct the prediction. The model can be a simple
piecewise-linear function fitted to the sorted keys (the PGM-index, Ferragina and Vinciguerra
[Ferragina20PGM]) or a hierarchy of small models (the recursive model index, Kraska and colleagues
[Kraska18]). The prediction is `O(1)` and the local search is bounded by the model's error. The
model's coefficients plus the sorted array both live in the arena.

**Why hashing cannot serve this query.** A hash table answers "is key K present" in `O(1)`, but it
destroys order, so it cannot answer "what is the greatest key less than or equal to K". The block
index needs the predecessor, so an order-destroying structure is disqualified for it. Hashing and
minimal perfect hashing (CHD, BBHash, PTHash) are reserved for genuinely exact-match, immutable uses
such as a dense point-lookup index or the filters in chapter 06. If you wanted `O(1)` here you would
need a **dense** index (one entry per key, not per block), which bloats the index and defeats the
block-based design. So the realistic better-than-`log n` options are the order-preserving ones above,
none of which is guaranteed `O(1)`.

## 3. Design space

| Structure | Search cost | Predecessor? | Extra memory | Branch-free | Build cost |
|-----------|-------------|--------------|--------------|-------------|------------|
| Sorted + `lower_bound` (default) | O(log n) | Yes | None | No | None (already sorted) |
| Eytzinger | O(log n), small constant | Yes | None (reorder) | Yes | One reorder pass |
| Interpolation | O(log log n) avg, O(n) worst | Yes | None | Partly | None |
| Learned (PGM) | O(1) + bounded local | Yes | Model coefficients | Partly | Fit the model |
| Hash | O(1) | No | Slots | N/A | Build table |
| Minimal perfect hash | O(1) | No | ~3 bits/key | N/A | Build, immutable only |

The default stays sorted plus `lower_bound`. Eytzinger is the cheapest real win if the index turns
out to be hot in cache, because it keeps the same query and the same memory and only changes the fill
order. Interpolation and learned indexes are conditional bets that need their assumptions
(uniformity, fittability) to hold. Hash and minimal perfect hash are gated to a separate dense,
exact-match variant and may never be selected for the sparse block index.

## 4. Notable researchers and key papers

- Paul-Virak Khuong, Pat Morin — array layouts for comparison-based searching, including Eytzinger
  [KhuongMorin17].
- Tim Kraska, Alex Beutel, Ed Chi, Jeffrey Dean, Neoklis Polyzotis — the case for learned index
  structures [Kraska18].
- Paolo Ferragina, Giorgio Vinciguerra — the PGM-index, a piecewise-linear learned index with worst-
  case guarantees [Ferragina20PGM].
- Fabiano Botelho, Rasmus Pagh, Nivio Ziviani (CHD); Antoine Limasset and colleagues (BBHash);
  Giulio Ermanno Pibiri, Roberto Trani (PTHash) — minimal perfect hashing, reserved for exact-match.

## 5. Concrete design for this codebase

The design makes the index strategy a swappable, compile-time choice with a small runtime selector,
exactly as `TASKS.md` section I specifies, so that all strategies share one interface at zero runtime
cost and the chosen one is recorded with each segment.

**A `BlockIndex` concept.** It constrains the operations the reader needs: find the block for a key,
build from an arena over the writer's entries, and iterate in order for scans (chapter 11).

```
template <typename I>
concept BlockIndex = requires(const I ci, std::string_view key, core::arena &arena,
                              std::span<const index_entry> entries) {
  { ci.find(key) }            -> std::same_as<std::optional<index_entry>>; // predecessor block
  { I::build(arena, entries) } -> std::same_as<std::expected<I, core::status>>;
  { ci.begin() }; { ci.end() };                                           // ordered iteration
};
```

**Strategies** are separate types satisfying the concept: `sorted_index`, `eytzinger_index`,
`interpolation_index`, `learned_index`. Each holds an arena-backed contiguous layout and keys as
`std::string_view`s into the raw index image, so building one copies no key bytes.

**Runtime selection without virtual dispatch.** Hold the chosen index as a closed set,
`std::variant<sorted_index, eytzinger_index, interpolation_index, learned_index>`, and dispatch with
`std::visit`. This keeps the config knob below while avoiding a vtable; the compiler generates a small
jump table over a fixed set of types, and each strategy's `find` is inlinable.

**Config and persistence.** Add `config.index_kind_` (sorted, eytzinger, interpolation, hash,
learned), default sorted, threaded through the writer and reader. The chosen kind is stored in the
footer or manifest so each segment is read back with the structure it was written with; you cannot
read an Eytzinger image as a sorted array.

**Gating.** The predecessor-incapable strategies (hash, minimal perfect hash) are only selectable for
a separate dense, exact-match index variant, never for the sparse block index, so the type system and
the config validation together make an invalid choice impossible.

## 6. C++, memory, and concurrency mechanics

This is a compile-time-polymorphism exercise, which is why the project's existing comparator concept
(`src/storage/skiplist.hpp:Comparator`) and `EndianInteger` concept
(`src/core/serialization/concepts.hpp`) are the right precedents to follow rather than introducing
abstract base classes. The `BlockIndex` concept gives the same swappability with no runtime cost; the
`std::variant` plus `std::visit` gives the runtime knob with a jump table instead of a vtable. CRTP
(the curiously recurring template pattern) is an alternative to the concept if shared code needs to be
factored into a base, but the concept alone is simpler and preferred unless duplication appears.

Memory: every strategy is one arena allocation holding a contiguous layout, with keys as
`std::string_view`s into the raw bytes (zero copy), matching how the writer already stores
`index_entry::smallest_key_` as a view (`src/storage/sstable_writer.hpp`). The Eytzinger build is a
single reorder pass into a new arena buffer. The learned-index build fits the model and stores its
coefficients alongside the sorted array.

Performance mechanics: Eytzinger benefits from explicit prefetch intrinsics on the computed next
positions and from being branch-free; comparisons reuse the SIMD comparator
(`src/core/simd.hpp`). Because an SSTable and its index are immutable after the write, the index is
read-only and shared by all readers with no synchronization, so there is no concurrency concern here
at all (the same pleasant property as chapter 06).

## 7. Risks, alternatives, and interactions

The dominant risk is premature optimization, which is exactly why section I leads with "benchmark
first". The disk read of the data block almost always dwarfs the index search, so improving the
search may buy nothing measurable. Interpolation search's `O(n)` worst case is a real hazard on
skewed keys and must be guarded by a fallback to binary search. Learned indexes add build complexity
and depend on the keys being fittable; their worst case is bounded only by the model error, which
must be stored and respected. Persistence is a correctness concern: the index kind must travel with
the segment, or a reader will misparse it.

The fallback is the default itself: sorted plus `lower_bound`. It is correct, simple, and almost
always good enough. Everything else is conditional.

Interactions: the ordered-iteration requirement in the concept exists for chapter 11's scans; the
prefix key encoding in chapter 09 adds an intra-block index (restart points) that complements this
inter-block index; and chapter 21 treats the index strategy as one of the parameters in the design
continuum.

## 8. Experiment plan

**Hypothesis.** For realistic block counts, the index search is not the bottleneck when the following
block read is uncached; when the block is cached, Eytzinger is measurably faster than sorted binary
search, while interpolation and learned indexes win only under their assumptions.

**Setup.** Extend the existing Google Benchmark micro-benchmarks (`test/skiplist_benchmark.cpp` is the
template) to build each index over a range of entry counts (from hundreds to millions of blocks) and
time `find`. Run it twice: once with the subsequent block read simulated as a cached access, once as
an uncached access, per chapter 03's cached-versus-uncached rule.

**Baseline.** Sorted plus `lower_bound`.

**Variants.** Eytzinger; interpolation on uniform and on skewed keys; a learned (PGM) index.

**Metrics.** Nanoseconds per `find`; cache misses per `find` (via `perf`); and the fraction of total
lookup time the search represents once the block read is included, in each cache regime.

**Success criteria.** The experiment answers the gating question: it states, with numbers, whether
index search is ever a meaningful fraction of lookup time, and if so which strategy wins in which
regime.

**Killer result.** If, across realistic entry counts and in both cache regimes, the index search is a
negligible fraction of lookup time, then the default sorted array is final and no other strategy
should be implemented. This is the most likely outcome, and recording it would close the question for
good.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section I (Research — index structures).

1. Run the gating micro-benchmark first (sorted versus Eytzinger versus interpolation), cached and
   uncached, and record the answer to "is index search ever hot" in this chapter.
2. Only if it is hot: define the `BlockIndex` concept and implement `sorted_index` and
   `eytzinger_index`.
3. Add the `std::variant` selector and `config.index_kind_`, and persist the kind in the
   footer/manifest.
4. Gate hash and minimal perfect hash to a separate dense, exact-match variant.
5. Implement the SSTable reader's search on top of `BlockIndex` (the part missing in
   `src/storage/sstable_reader.cpp`).
