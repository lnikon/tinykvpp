# 06 — Filters: skipping tables that do not hold the key

Depends on: chapter 02 (why filters change point-read cost), chapter 05 (filters are allocated across
levels, so levels must exist), chapter 12 (the footer must round-trip before a filter region can be
trusted), chapter 03 (measurement). Follows the nine-section depth template.

## 1. Problem statement

A point read in an LSM-tree may have to look in many SSTables, most of which do not contain the key.
Chapter 02 showed that a per-table **filter** cuts the expected disk reads for an absent key from
`O(L)` to `O(L · s)`, where `s` is the filter's false-positive rate. frankie has reserved space for
this and built nothing. The footer struct `src/storage/sstable_format.hpp:sstable_footer` carries
`bloom_offset_` and `bloom_size_`, and the format comment in that file says "no support for blooms
yet". The read path `src/engine/engine.cpp:engine::get` does not consult SSTables at all yet (chapter
01, section 4), so there is nothing to filter for; but the moment chapter 05's reader and manifest
make SSTable reads real, absent-key reads will pay full read amplification unless a filter exists.

There is a property of this codebase that makes the filter design unusually favorable, and it is the
same property chapter 07 exploits for the index: SSTables are immutable, so every key in a table is
known at the moment the table is written. That allows filter constructions that are impossible for a
mutable set.

## 2. Background and theory

A **filter** is a small structure that, given a key, answers "definitely not present" or "possibly
present". It never says "not present" for a key that is present (no false negatives), but it may say
"possibly present" for a key that is absent (a false positive, at rate `s`). On a "definitely not",
the read skips the table's data block entirely; on a "possibly present", it reads the block and finds
the truth. So a filter never causes a wrong answer; it only sometimes fails to save a disk read.

The classic construction is the **Bloom filter** [Bloom70]. It is a bit array of `m` bits and `k`
hash functions. To insert a key, set the `k` bits the hashes point to. To query, check those `k`
bits; if any is zero the key is absent. For `n` keys the false-positive rate is approximately
`(1 - e^{-kn/m})^k`. The optimal number of hash functions is `k = (m/n) · ln 2`, and at that optimum
the bits per key needed for a false-positive rate `p` is `m/n ≈ 1.44 · log2(1/p)`. Concretely, about
10 bits per key gives roughly a 1% false-positive rate. The whole filter is small enough to keep in
memory while the data stays on disk, which is the entire point.

The **immutability insight**: a Bloom filter supports insertion into a growing set, but an SSTable's
key set never grows after it is written. Filters that require knowing all keys up front are therefore
usable here, and they are strictly better on space and query speed. The **XOR filter** and its
successor the **binary fuse filter** [GrafLemire20] are built from the full key set in one pass and
use about 1.23x and 1.13x fewer bits than a Bloom filter at the same false-positive rate, with faster
queries, at the cost of being build-only (not updatable). The **ribbon filter** [Dillinger21], used
in RocksDB, trades a little build time for space close to the information-theoretic minimum. This is
the same reasoning chapter 07 uses to reserve minimal perfect hashing for immutable, exact-match
uses; a filter is exactly such a use.

The **Monkey insight** [Dayan17Monkey]: when filters are spread across the levels of an LSM-tree, the
naive choice gives every level the same bits per key. That is not optimal. Because a lookup probes
one filter per level and the deeper levels hold far more data, the total expected disk I/O for a fixed
total filter memory is minimized by giving the larger, deeper levels *more* bits per key (a lower
false-positive rate) and the small, shallow levels fewer. Monkey derives the optimal allocation and
shows it cuts the expected point-read cost meaningfully for the same memory budget. This is why
chapter 06 depends on chapter 05: you cannot allocate across levels that do not exist.

A final distinction: a Bloom-style filter answers only **point** membership ("is key K here?"). It
says nothing useful about a **range** ("are there any keys between A and B here?"), which a scan
(chapter 11) needs. **Range filters** fill that gap: **SuRF** [Zhang18SuRF] stores a succinct trie of
key prefixes that can answer range-emptiness, and **Rosetta** [Luo20Rosetta] is a range filter built
from a hierarchy of Bloom filters tuned for the workload.

## 3. Design space

Bits-per-key figures are at a 1% false-positive rate, approximate.

| Filter | Bits/key | Query speed | Build cost | Updatable | Range queries |
|--------|----------|-------------|------------|-----------|---------------|
| Bloom | ~10 | Good (k probes) | Cheap | Yes | No |
| Blocked Bloom | ~10 | Best (one cache line) | Cheap | Yes | No |
| Cuckoo | ~9 | Good | Medium | Yes (bounded) | No |
| XOR | ~8.1 | Very good | One pass, may retry | No | No |
| Binary fuse | ~9 raw, denser variants | Very good | One pass | No | No |
| Ribbon | near-optimal | Good | Higher | No | No |
| SuRF | varies | Good | Medium | No | Yes |
| Rosetta | varies | Good | Medium | No | Yes |

The recommended path: start with a **blocked Bloom filter** (all of one key's bits in a single cache
line, so a query is one memory access; due to Putze, Sanders, and Singler) as the simplest correct
choice, then evaluate a **binary fuse filter** to exploit immutability for space and speed. Add a
**range filter** only when chapter 11's scans show absent-range reads are a measured cost.

## 4. Notable researchers and key papers

- Burton H. Bloom — the Bloom filter [Bloom70].
- Bin Fan, David Andersen, Michael Kaminsky, Michael Mitzenmacher — cuckoo filters [Fan14].
- Thomas Mueller Graf, Daniel Lemire — XOR and binary fuse filters [GrafLemire20].
- Peter C. Dillinger, Stefan Walzer — ribbon filters [Dillinger21].
- Niv Dayan, Manos Athanassoulis, Stratos Idreos — Monkey, optimal filter allocation across levels
  [Dayan17Monkey].
- Huanchen Zhang and colleagues — SuRF, a succinct range filter [Zhang18SuRF].
- Siqiang Luo and colleagues — Rosetta, a robust range filter [Luo20Rosetta].

## 5. Concrete design for this codebase

The filter is built at flush time, stored in the SSTable's reserved region, and consulted on read.

**Build.** Extend `src/storage/sstable_writer.cpp` so that, as keys stream through `append`, their
hashes are collected, and at finalization a filter is built over all of them and serialized into the
bloom region. Its offset and size are recorded in the footer's `bloom_offset_`/`bloom_size_`. This
must wait for chapter 12 to make the footer round-trip correctly, otherwise the recorded offsets are
not trustworthy.

**Interface.** A compile-time `Filter` concept, mirroring chapter 07's approach, so different filters
are swappable without virtual dispatch:

```
template <typename F>
concept Filter = requires(const F cf, std::string_view key, std::span<const std::byte> image) {
  { cf.maybe_contains(key) } -> std::convertible_to<bool>;
  { F::build_into(/* keys */, /* arena */) } -> std::same_as<std::expected<std::string_view, core::status>>;
  { F::load(image) } -> std::same_as<F>;
};
```

**Read.** When a segment is opened (chapter 05's `segments_mgr`), its filter image is loaded into the
segment's arena and parsed once. `engine::get`, before reading a segment's data block, calls
`maybe_contains(key)` and skips the segment on a "definitely not".

**Per-level allocation (Monkey).** Add a config function `bits_per_key(level)` that returns more bits
for deeper levels per [Dayan17Monkey], default to a Monkey-optimal schedule, and record the chosen
schedule in the manifest so a segment is read back consistently. Uniform allocation is the special
case and the baseline to beat in the experiment.

## 6. C++, memory, and concurrency mechanics

A filter needs a fast, well-distributed hash of the key. The existing `src/core/crc32.hpp` is a
checksum, not a hash, and is a poor choice for filter bit selection; use a real non-cryptographic
hash such as xxh3, computed once per key and split into the `k` bit positions (the standard
double-hashing trick derives all `k` positions from two hash words, avoiding `k` separate hashes).

The filter's storage is a byte image in the segment's arena (chapter 01, section 6), exposed as
`std::span<const std::byte>`. After build, a filter is immutable and read-only, so there is **no
concurrency concern at all**: many readers can probe it without synchronization, which is one reason
the immutable-SSTable design is pleasant. A blocked Bloom filter is laid out so that the bits for one
key fall in a single 64-byte cache line, turning a query into one cache miss instead of `k`; SIMD can
test the `k` bits within that line in parallel.

The build-only filters (XOR, fuse) require the full key set in memory at build time. The writer
already sees every key, but it currently keeps only each block's smallest key for the index, not all
keys; building such a filter means retaining all key hashes (8 bytes each) for the duration of the
flush, in an arena, which is cheap. XOR/fuse construction can fail for a given random seed and retry
with another; the build loop must handle that, returning `core::status` only if it exhausts retries.

## 7. Risks, alternatives, and interactions

The main risk is spending memory on filters that the workload does not benefit from: if reads are
almost always for present keys, or the working set is cached, filters help little. The experiment in
section 8 measures this directly. The build-only filters add build-time complexity (retries) and
cannot be updated, which is fine for immutable SSTables but means they cannot be used for the
memtable. The range filters are a larger investment justified only once scans are shown to read empty
ranges.

The simplest fallback is a classic Bloom or blocked Bloom filter with uniform bits per key: well
understood, easy to get right, and already most of the benefit. Monkey allocation and fuse filters
are refinements layered on top.

Interactions: this chapter needs chapter 12 (footer correctness) before it can store offsets,
chapter 05 (levels) before Monkey allocation means anything, and chapter 11 (scans) before range
filters are worth building.

## 8. Experiment plan

**Hypothesis.** A per-table filter cuts absent-key disk reads roughly in proportion to its
false-positive rate, and Monkey allocation reduces total expected reads versus uniform allocation at
equal memory.

**Setup.** Engine-level driver (chapter 03) with levels formed (chapter 05). Workload: YCSB-C
(read-only) with a controlled fraction of reads targeting absent keys, uniform and Zipfian key
distributions, uncached regime so disk reads are visible.

**Baseline.** No filter (full read amplification for absent keys), then uniform-allocation Bloom.

**Variants.** Blocked Bloom and binary fuse, each at several bits-per-key; uniform versus Monkey
allocation across levels.

**Metrics.** Measured false-positive rate; disk reads per absent-key lookup; filter memory; build
time per table; and point-read p99.

**Success criteria.** Measured false-positive rate matches theory; absent-key disk reads fall as
predicted; Monkey beats uniform at equal memory; fuse matches Bloom's false-positive rate at fewer
bits per key.

**Killer result.** If absent-key reads are rare in the target workload, or the data is cached so disk
reads are free, the filter's memory is better spent elsewhere (for example a block cache, chapter 19).

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section L (Filters).

1. After chapter 12 fixes the footer, have `sstable_writer` collect key hashes and build a filter
   into the bloom region, recording `bloom_offset_`/`bloom_size_`.
2. Define the `Filter` concept; implement blocked Bloom first, then binary fuse.
3. Load and parse a segment's filter on open; consult it in `engine::get` before reading the block.
4. Add `bits_per_key(level)` with a Monkey-optimal default, recorded in the manifest.
5. Run the experiment in section 8 and record results here.
