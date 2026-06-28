# RESEARCH.md — the frankie/tinykvpp research program

This document is the entry point to the research program for this project. It is not a backlog
and it is not marketing. It is a technical reference that explains, in plain language and in a
fixed order, every research direction the project can take, why each one matters, what the prior
art is, who produced that prior art, and how each direction would be built in this specific
codebase.

`TASKS.md` remains the short, actionable checklist. This document is the depth behind it. Where a
`TASKS.md` line says "do X", a chapter here says what X is, why it is worth doing, what the
alternatives are, and how to measure whether it worked.

## Who this is written for

The reader is assumed to be a strong modern C++ programmer. That assumption lets the text skip
explanations of language basics such as RAII, templates, move semantics, and `std::expected`.

The reader is **not** assumed to know database internals, distributed systems theory, or the
research literature. Every such concept is defined the first time it appears, and again in the
glossary. When a chapter uses an advanced or subtle C++ feature whose behavior is easy to get
wrong (the C++ memory model, `std::memory_order`, lock-free memory reclamation, coroutines), that
feature is explained where it is used, because correctness there depends on details that even
experienced C++ programmers rarely need.

## How to read this document

Read Part I first. It is the foundation. Chapter 01 walks through the system as it exists today,
file by file, so that later chapters can point at real code instead of speaking in the abstract.
Chapter 02 teaches the theory of log-structured merge-trees (the family of data structure this
project belongs to) from first principles. Chapter 03 defines how every later experiment is
measured. After Part I, the remaining chapters can be read in any order; each one names its
dependencies at the top.

If you only want the recommended path of action, read Part I, then chapter 20 (the dependency
graph and recommended sequencing).

## Conventions used in every chapter

- **Code references** are written as `path:symbol`, for example `src/engine/engine.cpp:engine::put`.
  These are clickable in most terminals and editors. They always point at code that exists in this
  repository at the time of writing.
- **Literature references** are written as an inline key in square brackets, for example
  `[Dayan17Monkey]`. Every key resolves to a full citation (authors, title, venue, year) in
  `research/bibliography.md`.
- **Defined terms** are written in bold the first time they appear in a chapter, and are listed
  with a plain definition in `research/00-glossary.md`.
- **Prose style**: short sentences, stated in linear order, with the reasoning made explicit. The
  text does not rely on the reader inferring an unstated point.

## The depth template

Every chapter in Parts II through VI follows the same nine sections, in this order. The template
exists so that no chapter quietly skips the design space, the prior art, the concrete C++ design,
or the way to measure success.

1. **Problem statement.** What is missing or wrong today, pointing at exact files, functions, and
   fields in this repository.
2. **Background and theory.** The idea explained from first principles, including a cost analysis
   in the I/O model defined in chapter 02.
3. **Design space.** The named algorithms and technologies that solve the problem, compared in a
   table whose axes are time complexity, memory cost, effect on read/write/space amplification, fit
   with immutable on-disk files, and implementation cost.
4. **Notable researchers and key papers.** Names, years, and venues, with inline citation keys.
5. **Concrete design for this codebase.** Proposed C++ API signatures, on-disk format changes, the
   memory-management approach (which arena, what alignment, what owns what and for how long), and
   the concurrency approach, grounded in named existing files.
6. **C++, memory, and concurrency mechanics.** The language-level detail: which features are used,
   how memory is allocated and freed, what lifetime and aliasing concerns arise, and how the design
   integrates with the existing `core::status` and `noexcept` error model.
7. **Risks, alternatives, and interactions.** How it can fail, a simpler fallback if it does not
   pay off, and which other chapters it couples to.
8. **Experiment plan.** A hypothesis, the workload (drawn from chapter 03), the baseline, the
   metrics, the success criteria, and the specific result that would prove the idea not worth it.
9. **Implementation checklist.** Concrete steps, cross-referenced to the matching `TASKS.md`
   section.

## Map of the document

Part I — Foundations (read first):
- `research/00-glossary.md` — plain definitions of every term used anywhere in the program.
- `research/01-architecture-walkthrough.md` — the current system, file by file, with the data
  paths traced through real code.
- `research/02-lsm-theory.md` — log-structured merge-trees from first principles; the
  read/write/space amplification tradeoff; the external-memory cost model.
- `research/03-benchmark-methodology.md` — how every experiment in this program is measured.

Part II — Core storage-engine research (the chapters most tightly tied to the current code):
- `research/04-durability-write-path.md` — the write-ahead log, fsync, group commit, asynchronous
  I/O with io_uring.
- `research/05-compaction-and-levels.md` — leveling, tiering, lazy leveling, the manifest,
  delete-aware compaction.
- `research/06-filters.md` — Bloom filters and their modern successors; per-level bit allocation;
  range filters.
- `research/07-block-index.md` — the in-SSTable index structure (expands `TASKS.md` section I).
- `research/08-memtable-structures.md` — skiplist versus radix tree, B+-tree, and others.
- `research/09-compression-and-key-encoding.md` — block compression and prefix key encoding.
- `research/10-key-value-separation.md` — storing large values outside the tree (WiscKey).
- `research/11-mvcc-snapshots-iterators.md` — multi-version reads, snapshots, and range scans.
- `research/12-integrity-crash-consistency.md` — checksums, torn writes, and crash recovery.

Part III — The node as a concurrent machine:
- `research/13-concurrency-and-cpp-memory-model.md` — execution models, the C++ memory model,
  lock-free structures, and safe memory reclamation.

Part IV — The distributed system (the project's stated end goal):
- `research/14-replication-and-consensus.md` — Raft, Paxos, and leaderless replication.
- `research/15-consistency-time-transactions.md` — consistency models, clocks, and transactions.
- `research/16-partitioning-and-membership.md` — sharding, hashing, and failure detection.

Part V — Hardware frontier and new capabilities:
- `research/17-hardware-frontier.md` — zoned storage, persistent memory, io_uring, RDMA, GPUs.
- `research/18-capabilities-and-access-methods.md` — vector search, secondary indexes, change
  streams, adaptive indexing.

Part VI — Methodology and correctness (cross-cutting):
- `research/19-testing-and-verification.md` — deterministic simulation testing, TLA+, Jepsen,
  differential testing, and cache-replacement research.

Part VII — Synthesis:
- `research/20-dependency-graph-and-sequencing.md` — what depends on what, and the recommended order.
- `research/21-design-continuum.md` — the unifying view that these choices are points in one space.

Back matter:
- `research/bibliography.md` — the full reference list.

## Dependency graph and recommended sequencing

The arrows mean "should come before", because the earlier item either unblocks the later one or is
needed to measure it. This is the same graph drawn in detail in chapter 20.

```
03 benchmark harness ──┬──> 04 durability (group commit)
                       │
                       ├──> 05 compaction ──> 06 filters (Monkey needs levels)
                       │                  └──> 12 crash consistency (manifest)
                       │
                       ├──> 07 block index
                       ├──> 08 memtable structures
                       ├──> 09 compression / key encoding
                       ├──> 10 key-value separation
                       └──> 11 mvcc / snapshots / scan

19 testing & methodology ──> (adopt before) ──> 13 concurrency ──> 14,15,16 distributed
17 hardware, 18 capabilities, 21 design continuum: independent, longer horizon
```

The single most important sequencing claim in this program is this: the methodology in chapter 19,
specifically deterministic simulation testing, should be adopted before the concurrency work in
chapter 13 and the distributed work in Part IV, not after. The reason is given in chapters 19 and
20. In short, the bugs that those features introduce are not reliably reproducible without a
deterministic test harness, and retrofitting such a harness onto an already-concurrent,
already-distributed system is far harder than building it first.

## Status

This is a living document. Chapters are written to be revised as the code changes and as
experiments produce results. When a chapter's experiment is run, its results should be recorded in
that chapter so the next reader does not repeat the work.
