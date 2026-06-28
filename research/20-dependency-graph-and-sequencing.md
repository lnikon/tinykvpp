# 20 — Dependency graph and recommended sequencing

This is a synthesis chapter, not a research chapter, so it does not follow the nine-section template. It
collects the dependencies stated at the top of every other chapter into one graph, explains each edge in
plain terms, and gives a single recommended order of work with the reasoning. Read chapter 01 through
chapter 03 first; this chapter assumes them.

## 1. The graph

Each arrow means "do the source before the target", because the source either unblocks the target or is
needed to measure it.

```
                         03 benchmark harness
                                  |
        +------------------+------+------+--------------------+-----------------+
        |                  |             |                    |                 |
        v                  v             v                    v                 v
04 durability        05 compaction   07 block index    08 memtable        09 compression
(group commit,            |          (gated on bench)   structures         + key encoding
 io_uring)                |                                                      |
        |                 +--> 06 filters (Monkey needs levels)                 |
        |                 +--> 12 crash consistency (manifest, footer) <--------+
        |                 +--> 11 mvcc / snapshots / scan
        |                          |
        |                          +--> 10 key-value separation (needs compaction to show write-amp)
        v
17 hardware (io_uring deep, ZNS, NUMA, SIMD)

19 testing & methodology  ===adopt before===>  13 concurrency  ===>  14, 15, 16 distributed
                                                     |
18 capabilities, 21 design continuum: independent, longer horizon
```

## 2. Why each edge exists

**03 before everything.** Every research chapter ends in an experiment, and every experiment uses the
engine-level workload harness from chapter 03. Without it, nothing can be measured, so no optimization
can be justified or rejected. The harness is the first thing to build.

**03 to 04 (durability).** Group commit's value is a throughput number that only the harness can
produce; and the per-write fsync is the most visible cost, so this is the cheapest large win and a good
first result while the harness is fresh.

**12 before 06 (filters).** A filter's location is recorded in the SSTable footer, and the footer does
not round-trip today (chapter 01, section 8). The single-source-of-truth encoding in chapter 12 must fix
that before chapter 06 can store filter offsets the reader can trust.

**05 before 06 (Monkey allocation).** Monkey's per-level filter sizing only has meaning once levels
exist, and levels come from compaction in chapter 05.

**05 and 12 share the manifest.** Compaction needs an atomic, crash-safe way to install its output
(chapter 05), and that mechanism is the manifest, which is also the crash-consistency backbone of
chapter 12. They are built together.

**11 with 05.** The merging iterator and the version-drop rule are the same in a read (chapter 11) and in
a compaction (chapter 05); building one builds most of the other. Chapter 11 is also what finally
connects `engine::get` to SSTables.

**05 before 10 (key-value separation).** Separation's benefit is lower write amplification, and write
amplification is produced by compaction, so it cannot be measured until chapter 05 exists.

**19 before 13 and Part IV — the central claim.** This is the single most important sequencing decision
in the program. The bugs introduced by concurrency (chapter 13) and distribution (chapters 14 to 16)
appear only under specific interleavings of events and faults, and such bugs are nearly impossible to
debug unless they can be reproduced exactly. Deterministic simulation testing (chapter 19) makes them
reproducible, but only if all nondeterminism flows through controllable seams, which is cheap to arrange
before concurrency and distribution exist and very expensive to retrofit afterward. So the testing
methodology must be adopted before, not after, the features that make it necessary. FoundationDB's
experience is the precedent: they built the simulator first and credit it for the system's reliability.

**13 before 14, 15, 16.** A distributed node is a concurrent node, so the execution model and the memory-
ordering discipline of chapter 13 must be settled before the distributed work that relies on them.

**Independent / longer horizon.** Chapter 07 (block index), chapter 08 (memtable structures), chapter 17
(hardware), chapter 18 (capabilities), and chapter 21 (design continuum) do not block the main line. Two
of them (07 and 08) are explicitly gated on benchmarks showing the structure is hot, and may end with the
conclusion that the current default is fine.

## 3. The recommended order

In one line: harness, then durability, then the compaction-and-correctness core, then filters, then the
conditional structural work, with the testing methodology adopted before any concurrency or distribution.

1. **Chapter 03** — build the workload harness. Nothing can be judged without it.
2. **Chapter 04** — group commit (and later io_uring). The cheapest large throughput win.
3. **Chapter 12** — fix the footer round-trip and the single-source-of-truth format, add checksums, and
   build the manifest. This unblocks reading SSTables correctly and is needed by 05, 06, and 11.
4. **Chapter 05 + chapter 11** — compaction with levels and the merging iterator, snapshots, and scans.
   This is the core that bounds read and space amplification and finally makes flushed data readable.
5. **Chapter 06** — filters, including Monkey allocation across the levels from step 4.
6. **Chapters 07, 08, 09, 10** — the conditional structural work (index layout, memtable structure,
   compression and key encoding, key-value separation), each gated on its experiment.
7. **Chapter 19** — stand up deterministic simulation, the differential harness, fuzzing, and
   sanitizers. In practice this starts in parallel with step 1 (the injectable seams are cheap early) and
   must be in place before step 8.
8. **Chapter 13** — choose and build the execution model (single loop, then thread-per-core).
9. **Chapters 14, 15, 16** — replication, consistency and transactions, partitioning and membership.
10. **Chapters 17, 18, 21** — hardware backends, new capabilities, and the design-continuum view, as
    measured need and interest dictate.

## 4. The five highest-value directions, and the tension to respect

If effort is scarce, these five give the most, in this order: the workload harness (chapter 03, because
it unblocks all judgment), group commit (chapter 04, because it is the cheapest large win), the
compaction-and-manifest core (chapters 05, 11, 12, because it makes the engine a real LSM store rather
than a write-only log), deterministic simulation testing (chapter 19, because it is cheap now and
priceless later), and filters (chapter 06, because they make point reads competitive).

The tension to respect is the one in the central claim: it is tempting to defer testing methodology until
there is something complex to test, but by then the cost of making the system deterministically testable
has multiplied. Adopt chapter 19's seams early, while the engine is still simple, even though their
payoff is mostly in the later chapters. Sequencing, here, matters more than any single optimization.
