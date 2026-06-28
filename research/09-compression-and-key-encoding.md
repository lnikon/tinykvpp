# 09 — Block compression and key encoding

Depends on: chapter 01 (the data-block format and the dead header fields), chapter 02 (space
amplification and the cached/uncached rule), chapter 03 (measurement), chapter 12 (checksum ordering).
Follows the nine-section depth template.

## 1. Problem statement

frankie stores data blocks uncompressed and stores each key in full. The block header
`src/storage/sstable_format.hpp:sstable_data_block_header` already has the fields for compression:
`compression_type` (an enum with `none` and `lz4`), `uncompressed_size`, and `compressed_size`. They
are all dead: `src/storage/sstable_writer.cpp:get_data_block` always sets `compression_type` to
`none` and writes `compressed_size` equal to `uncompressed_size`. Keys are written in full by
`write_string(ikey)` for every entry, even though the keys within a block are sorted and usually share
long common prefixes. Both are missed space savings, and space is one of the three amplifications
(chapter 02).

## 2. Background and theory

There are two independent ways to make a block smaller, and they compose.

**Block compression.** Before writing a data block, run it through a general-purpose compressor; on
read, decompress it. The benefit is fewer bytes on disk and fewer bytes transferred. The cost is CPU
time to decompress on every read, and a coupling to block size: a larger block compresses better
(more context for the compressor) but costs more on a point read, because the whole block must be
decompressed to retrieve one key. The choice of compressor is a speed-versus-ratio tradeoff. **lz4**
is very fast and gives a modest ratio. **zstd** gives a better ratio with tunable effort levels and
supports trained dictionaries. **snappy** is fast and simple, between the two on ratio. All three are
mature, widely used in LSM stores, and available as libraries.

**Dictionary compression** is a refinement of zstd for the common case of many small, similar values
(for example serialized records that share field names). A dictionary is trained once on a sample of
the data and then primes the compressor so that even a tiny block compresses well, because the shared
content lives in the dictionary instead of being repeated in each block. The dictionary is stored once
per file or per level.

**Key prefix encoding.** Because the keys in a block are sorted, adjacent keys usually share a long
prefix. Instead of storing each key in full, store, for each key, the length of the prefix it shares
with the previous key and only the differing suffix. This is **front coding**, a classic technique
from information retrieval [WittenMoffatBell]. To keep the block searchable, insert a **restart
point** every few keys where the full key is stored again; the list of restart-point offsets is a
small array at the end of the block. A search within the block binary-searches the restart points,
then scans forward from the nearest one. This is exactly the LevelDB block format. The restart-point
array is, in effect, an intra-block index, which complements the inter-block index of chapter 07.

For sorted string keys the prefix saving is often large and is pure win on space with little CPU cost,
which is why it is usually worth doing even when block compression is not.

## 3. Design space

| Technique | Space saving | Compress speed | Decompress / read cost | Complexity |
|-----------|--------------|----------------|------------------------|------------|
| No compression (today) | None | N/A | None | None |
| lz4 | Modest | Very fast | Very fast | Low |
| snappy | Modest | Fast | Fast | Low |
| zstd | Good (tunable) | Medium | Fast | Low |
| zstd + trained dictionary | Good even for tiny blocks | Medium | Fast | Medium |
| No key encoding (today) | None | N/A | None | None |
| Shared-prefix + restart points | Large for sorted strings | Cheap | Cheap | Medium |
| Front coding (no restarts) | Largest | Cheap | Sequential only | Medium |

The recommended path: add **shared-prefix encoding with restart points** first, because it is cheap
and a large win for sorted string keys and it improves the block format that chapter 12 is fixing
anyway. Then add **block compression** with **lz4** as the default and **zstd** as the better-ratio
option, with dictionaries reserved for workloads of many small similar values.

## 4. Notable researchers and key papers

- Yann Collet — lz4 and zstd (Zstandard), including dictionary training.
- Google — snappy.
- Sanjay Ghemawat, Jeff Dean — the LevelDB block format with shared-prefix encoding and restart
  points.
- Ian Witten, Alistair Moffat, Timothy Bell — front coding and compressed indexes [WittenMoffatBell].

## 5. Concrete design for this codebase

Two changes, one to the block body format and one to the block image.

**Key encoding in the block builder.** Change `src/storage/sstable_writer.cpp:append` so that, instead
of `write_string(ikey)` for every entry, it writes, per entry, the shared-prefix length, the
suffix length, the suffix bytes, then the value, and records a restart point (the byte offset of a
full key) every `R` entries (a tunable, for example 16). At block finalization, append the restart-
point offset array and its count. This new layout must be reflected in the one-source-of-truth block
`encode`/`decode` that chapter 12 introduces, so the documented format and the code agree (which they
do not today, chapter 01, section 8).

**Block compression in the block image.** Change `get_data_block` so that, after the block body is
built, it is compressed with the configured codec, the header's `compression_type`,
`uncompressed_size`, and `compressed_size` are set truthfully, and the compressed bytes are what gets
written. On read, the SSTable reader decompresses into an arena buffer before searching the block.

Interfaces, following the project's concept style:

```
template <typename C>
concept BlockCodec = requires(std::span<const std::byte> in, std::span<std::byte> out) {
  { C::max_compressed_size(in.size()) } -> std::same_as<std::size_t>;
  { C::compress(in, out) }   -> std::same_as<std::expected<std::size_t, core::status>>;
  { C::decompress(in, out) } -> std::same_as<std::expected<std::size_t, core::status>>;
};
```

Add `config.compression_` (none, lz4, zstd) and `config.block_size_` (today's fixed 4096 in
`sstable_writer_config`). A zstd dictionary, when used, is trained at flush or compaction time and
stored in the file (or the manifest, per level), with its location recorded in the footer.

## 6. C++, memory, and concurrency mechanics

lz4 and zstd are C libraries; add them through the existing Conan dependency setup (the project
already uses Conan, per `README.md`). Compression needs an output buffer sized by the codec's
`compressBound`-style upper bound; allocate it from the writer's block arena (which already exists and
is reset per block in `record_data_block`). Decompression on the read side allocates the
uncompressed buffer from the segment's arena using the header's `uncompressed_size`, which is now
trustworthy.

The block image is immutable after it is written, so the read side has no concurrency concern, the
same property as chapters 06 and 07. Decompression is pure CPU and parallelizes trivially across
independent blocks if the read path ever becomes concurrent (chapter 13).

The ordering relative to the checksum (chapter 12) matters and must be fixed: the checksum must be
computed over the bytes that are actually written to disk, which are the compressed bytes. So the
order is encode the body, compress, then checksum the compressed image; on read it is verify the
checksum, then decompress. Checksumming the uncompressed bytes would fail to detect corruption that
happens to the compressed bytes on disk.

O_DIRECT (chapter 04) imposes block-size alignment on the written image; compression changes the
image size per block, so an O_DIRECT writer must pad each block up to the alignment, which slightly
reduces the space saving and must be accounted for in the experiment.

## 7. Risks, alternatives, and interactions

The main risk is CPU cost on hot reads: if reads are frequent and the data is already cached,
decompression is pure overhead with no I/O saving to offset it. The cached-versus-uncached split in
the experiment exposes this directly. A second risk is incompressible data: values that are already
compressed (images, encrypted blobs) will not shrink, so block compression buys nothing and only key
encoding helps. Dictionary staleness is a third: a dictionary trained on old data may compress new
data poorly, so it must be retrained periodically.

The fallback is conservative and still valuable: shared-prefix key encoding with no block compression
captures a large, cheap space saving on sorted string keys with negligible CPU cost. Block
compression is then an opt-in layer for workloads whose data is compressible and whose reads are not
CPU-bound.

Interactions: this chapter changes the block format that chapter 12 is making the single source of
truth, its restart-point array complements chapter 07's inter-block index, and its space savings show
up in chapter 05's space-amplification measurements.

## 8. Experiment plan

**Hypothesis.** Shared-prefix encoding meaningfully reduces on-disk size for sorted string keys at
negligible CPU cost; block compression reduces size further for compressible values, at a read-CPU
cost that is acceptable when reads are disk-bound and possibly not when they are cached.

**Setup.** Engine-level driver (chapter 03) with realistic value-size distributions (small with a
heavy tail) and both compressible and incompressible value corpora. Run point reads and short scans
in both cached and uncached regimes, across block sizes (4 KiB, 16 KiB, 64 KiB).

**Baseline.** Today's uncompressed, full-key blocks.

**Variants.** Prefix encoding alone; lz4; zstd at a couple of levels; zstd with a trained dictionary
on the small-similar-values corpus.

**Metrics.** On-disk size and space amplification; compression and decompression CPU per block; point-
read and scan latency in each cache regime; and the effect of block size on each.

**Success criteria.** Prefix encoding reduces size with no measurable read regression; block
compression reduces size on the compressible corpus with an acceptable read-CPU cost in the uncached
regime; the experiment yields a recommended default codec and block size.

**Killer result.** If the target data is incompressible and reads are cached, block compression is
pure cost and only prefix encoding is kept.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section P (Block compression and key encoding).

1. Add shared-prefix key encoding with restart points to the block body, reflected in chapter 12's
   single-source-of-truth block `encode`/`decode`.
2. Define the `BlockCodec` concept; integrate lz4 and zstd via Conan; wire the dead header fields
   truthfully and compress the block image in `get_data_block`.
3. Decompress in the SSTable reader; checksum over the compressed image (coordinate with chapter 12).
4. Add `config.compression_` and `config.block_size_`; add optional zstd dictionary training and
   storage.
5. Run the experiment in section 8 and record the recommended defaults here.
