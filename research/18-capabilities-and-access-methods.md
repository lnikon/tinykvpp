# 18 — Capabilities and access methods

Depends on: chapter 05 (compaction, which drives TTL expiry), chapter 11 (scans and the sequence log),
chapter 14 (the committed log, for change streams), chapter 15 (transactions, for secondary-index
consistency), chapter 07 (the block index, which adaptive indexing generalizes). Follows the nine-section
depth template.

## 1. Problem statement

frankie is a plain ordered key-value store: it offers point operations and (once chapter 11 lands) range
scans, and nothing more. Many uses want more: similarity search over vectors, lookups by a non-key
attribute, queries over multiple dimensions, expiry of old data, a feed of changes, and indexes that
build themselves from the query pattern. This chapter surveys those capabilities, and its theme is that
several of them are cheap because they reuse machinery the engine already has or will have, while a few
are larger bets. None of them exist today.

## 2. Background and theory

**Vector and approximate-nearest-neighbor (ANN) search.** A vector is a list of numbers (an embedding)
that represents an item; similarity search finds the stored vectors closest to a query vector under a
distance such as Euclidean or cosine. Exact search is too slow at scale, so approximate methods are
used. **HNSW** [Malkov18HNSW] builds a layered navigable graph that a search descends to hop quickly
toward the query. **Inverted-file** methods cluster the vectors and search only the nearest clusters,
often combined with **product quantization** [Jegou11PQ] to compress vectors. This capability is in
high demand because of machine-learning embeddings.

**Secondary indexes.** A secondary index lets you look up records by a non-key attribute. It is
maintained as a second mapping, from attribute value to primary key, which is itself stored in the
key-value store. The hard part is keeping the base data and the index consistent: updating both
atomically (which needs transactions, chapter 15) or accepting that the index lags.

**Multi-dimensional and spatial indexes.** To query by more than one dimension (for example a 2-D
location), one option is a dedicated structure such as an **R-tree** [Guttman84] or a k-d tree. The
other, which fits an ordered key-value store with no new structure, is a **space-filling curve** such as
the Z-order (Morton) curve or the Hilbert curve: it interleaves the bits of the dimensions into a single
ordered key so that points close in space are usually close in the key order, after which an ordinary
range scan (chapter 11) answers spatial range queries. This is an elegant reuse: the multi-dimensional
query becomes a one-dimensional scan.

**Inverted indexes** map each term to the list of documents containing it, the basis of full-text
search; the postings lists are stored as key-value entries.

**Time-to-live (TTL) and expiry.** Each entry carries an expiry time, and expired entries are dropped.
The natural place to drop them is during compaction (chapter 05), which already rewrites and filters
data, so expiry is nearly free given compaction.

**Change data capture, streaming, and watch.** Applications often want to observe the stream of changes:
a subscription that delivers every write, or an etcd-style "watch" on a key range. The substrate already
exists: every write has a sequence number and is recorded in the log (chapters 11 and 14), so the change
stream is that log exposed to subscribers.

**Adaptive indexing and database cracking.** Instead of building an index up front, **database
cracking** [Idreos07Cracking] builds it incrementally as a side effect of queries: each query physically
partitions the data around the values it touched, so the structure converges toward an index for the
actual workload. It is the adaptive counterpart to the fixed block index of chapter 07.

## 3. Design space

| Capability | How it layers on the KV store | Reuses | Cost |
|------------|-------------------------------|--------|------|
| Vector / ANN | New index (HNSW/IVF) over vector values | SIMD distance | Large (memory, build) |
| Secondary index | Second KV mapping attribute to key | Transactions (chapter 15) | Medium (consistency) |
| Spatial / multi-dim | Z-order/Hilbert key encoding + range scan | Scan (chapter 11) | Small (elegant reuse) |
| Inverted index | Term to postings as KV entries | Scan, compression | Medium |
| TTL / expiry | Per-entry expiry dropped in compaction | Compaction (chapter 05) | Small |
| Change stream / watch | Expose the committed log | Log, sequence (chapters 11, 14) | Small-medium |
| Adaptive indexing | Incremental block index from queries | Block index (chapter 07) | Medium |

The cheap, high-leverage capabilities are spatial-via-Z-order, TTL-via-compaction, and change-stream-
via-log, because they reuse existing machinery. Vector search is the largest and most in-demand new
capability and the clearest differentiator. Secondary and inverted indexes are medium bets gated on
demand.

## 4. Notable researchers and key papers

- Yury Malkov, Dmitry Yashunin — HNSW for approximate nearest-neighbor search [Malkov18HNSW].
- Herve Jegou, Matthijs Douze, Cordelia Schmid — product quantization [Jegou11PQ].
- Antonin Guttman — the R-tree [Guttman84].
- Stratos Idreos, Martin Kersten, Stefan Manegold — database cracking [Idreos07Cracking].
- The Morton (Z-order) and Hilbert space-filling curves, classical, used in UB-trees and many spatial
  systems.

## 5. Concrete design for this codebase

**Spatial via space-filling curves (cheapest first).** Provide a key-encoding helper that interleaves
the bits of multiple dimensions into one ordered key (Z-order to start, Hilbert if locality needs to be
better), then answer spatial range queries with the existing scan (chapter 11). No new index structure
is needed; this is pure reuse and a good demonstration of the ordered-key design's flexibility.

**TTL via compaction.** Add an optional expiry field to the entry encoding (coordinated with chapter 09
and chapter 12's format) and have the compaction drop policy (chapter 05) discard expired entries, the
same mechanism that drops tombstones.

**Change stream via the log.** Expose the committed sequence of writes (chapters 11 and 14) as a
subscribable feed and an etcd-style watch over a key range, delivered through coroutine streams (chapter
13). This reuses the log that already exists rather than building new bookkeeping.

**Vector search.** Add an ANN index (HNSW) over vector-valued entries, with the vectors stored as values
and the graph as a separate structure in an arena or memory-mapped file. Distance computation uses the
SIMD infrastructure (`src/core/simd.hpp`, chapter 17). This is a self-contained, large feature and the
main differentiator.

**Secondary indexes.** Maintain an attribute-to-key mapping as additional key-value entries, updated
within the same transaction as the base write (chapter 15) for a consistent index, or asynchronously for
a lagging one.

**Adaptive indexing.** As a variant of the block index (chapter 07), build the index incrementally from
the queries actually seen, converging to the workload.

## 6. C++, memory, and concurrency mechanics

The Z-order encoding is bit interleaving over the dimension bytes, producing a key the existing
comparator orders correctly; no structural change. The HNSW graph is nodes and neighbor lists in an
arena (or memory-mapped for large indexes), with vector distances computed by SIMD; its build and search
are CPU-bound and parallelize across the sharding of chapter 13. Secondary-index updates ride the
transaction machinery of chapter 15, so the base write and the index write commit together. The change-
stream and watch use coroutine-based streams (chapter 13) over the committed log, with back-pressure so
a slow subscriber does not stall the engine. TTL adds a field to the entry format, which must be
coordinated with the single-source-of-truth encoding of chapter 12.

## 7. Risks, alternatives, and interactions

The overarching risk is scope creep: each of these is a feature in its own right, and adding them
dilutes focus, so each should be justified by a real use case. Vector indexes consume significant memory
and have build cost; secondary indexes raise consistency questions that only transactions answer well;
adaptive indexing adds complexity to the read path. The cheap reuses (spatial, TTL, change stream) carry
little risk because they ride existing mechanisms.

The fallback is to ship only the cheap reuses and treat vector and secondary indexes as separately
scoped projects.

Interactions: spatial and adaptive indexing build on chapters 11 and 07; TTL builds on chapter 05;
change streams build on chapters 11 and 14; secondary indexes build on chapter 15; vector distance uses
chapter 17's SIMD.

## 8. Experiment plan

**Hypothesis.** The space-filling-curve, TTL, and change-stream capabilities add useful function at
negligible structural cost by reusing existing machinery; vector search delivers acceptable recall and
latency at a known memory cost.

**Setup.** Engine-level driver (chapter 03) extended per capability: spatial range queries over Z-order
keys; a TTL workload measuring that expired data is reclaimed; a change-stream subscriber measuring
delivery latency and back-pressure; and, for vectors, a standard ANN dataset measuring recall against
latency.

**Baseline.** The plain KV operations for the reuse cases; brute-force exact search for the ANN recall
reference.

**Variants.** Z-order versus Hilbert locality; synchronous versus asynchronous secondary indexes; HNSW
parameter settings trading recall against latency and memory.

**Metrics.** Spatial-scan efficiency (fraction of scanned keys that match); TTL reclamation over time;
change-stream delivery latency under back-pressure; ANN recall versus latency and memory.

**Success criteria.** The reuse capabilities work with no measurable regression to core operations;
vector search reaches a target recall at acceptable latency and memory for the intended dataset.

**Killer result.** If the use case is plain key-value access, none of these are built; the cheap reuses
remain available as low-cost options if a need appears.

## 9. Implementation checklist

Part V (capabilities and access methods).

1. Add a space-filling-curve key encoder and answer spatial range queries with the chapter 11 scan.
2. Add a per-entry expiry field (coordinate with chapters 09 and 12) and expire in the chapter 05 drop
   policy.
3. Expose the committed log as a change stream and a watch via coroutine streams (chapter 13).
4. Build an HNSW vector index over vector values using SIMD distance (chapter 17).
5. Add secondary indexes maintained transactionally (chapter 15); evaluate adaptive indexing as a
   chapter 07 variant. Record results here.
