# 19 — Testing and verification

Depends on: chapter 01 (the fs and status seams), chapter 03 (the harness), chapter 11 (the differential
model), chapter 12 (crash consistency), chapter 13 (concurrency), chapters 14 and 15 (consensus and
transactions). Follows the nine-section depth template. This is the cross-cutting correctness chapter; it
is referenced by the experiment section of almost every other chapter.

## 1. Problem statement

frankie has unit tests and micro-benchmarks (the files under `test/`), and `TASKS.md` section A asks for
round-trip and fuzz tests of the wire format. That is the right floor, but it is far below what the hard
parts of this program need. Crash consistency (chapter 12), concurrency (chapter 13), consensus (chapter
14), and transactions (chapter 15) fail only under specific, rare interleavings of events and faults,
and a failure that cannot be reproduced cannot be debugged. There is no fault injection, no deterministic
reproduction, and no consistency checking today. The project's stated value in `README.md` of being
"well-covered" requires methods beyond unit testing, and the central claim of this chapter is that the
most important of those methods should be adopted early, before the concurrency and distribution that
make bugs unreproducible are built.

## 2. Background and theory

**Deterministic simulation testing** is the highest-leverage method for a system like this, pioneered for
FoundationDB [FoundationDB21]. The whole system is run on a single thread against simulated time, a
simulated disk, and a simulated network, with every source of randomness driven by one seed. Because
everything is deterministic, a test can inject faults (crashes, partitions, slow disks, message
reordering) and, when something breaks, reproduce the exact failure by replaying the seed. This converts
the impossible task of debugging a rare distributed race into a deterministic replay. Its one demand is
architectural: all nondeterminism (time, I/O, randomness, scheduling) must flow through seams the
simulator can control, which is why it must be designed in early rather than retrofitted.

**Formal methods** check the design, not the code. **TLA+** with its **PlusCal** notation [Lamport] lets
you specify a protocol precisely and model-check it: the checker explores the reachable states and
reports any that violate a safety property (something bad never happens) or a liveness property
(something good eventually happens). Raft, parts of Amazon's services, and many consensus protocols have
been specified and checked this way; it finds design bugs that no amount of testing the implementation
would reach, because it explores states exhaustively within a small bound.

**Property-based testing**, from the QuickCheck lineage, generates many random inputs and checks that
stated properties hold, rather than checking fixed examples. **Metamorphic testing** checks relationships
between outputs that must hold even when the exact output is unknown; for this engine, "a compaction must
not change the logical key-value state" is a metamorphic property. **Differential testing** runs the
engine against a reference oracle and compares; the natural oracle here is an in-memory ordered map (and,
for richer behavior, an established store such as RocksDB), and chapter 11 already specifies a
differential test of MVCC against a versioned-map model.

**Jepsen** [Kingsbury] is the standard for black-box distributed consistency testing: it drives a real
cluster through operations while injecting partitions and crashes, records the history, and checks it
against a consistency model with a checker such as Knossos or Elle, which can find linearizability and
isolation violations. It is how the guarantees of chapters 14 and 15 are validated.

**Fuzzing** feeds coverage-guided random input to a target to find crashes and undefined behavior;
libFuzzer and AFL are the common tools. The decode paths for the wire formats (the SSTable and WAL
parsers, chapters 01 and 12) are exactly the kind of byte-parsing code fuzzing is best at, which is the
"truncated/corrupt-buffer fuzz tests" of `TASKS.md` section A.

**Sanitizers and concurrency testing.** AddressSanitizer, UndefinedBehaviorSanitizer, and
ThreadSanitizer catch memory errors, undefined behavior, and data races at run time and should be on in
continuous integration. For the rare interleavings that ThreadSanitizer may not happen to hit, controlled
concurrency testing such as probabilistic concurrency testing (PCT) systematically explores schedules.

**Crash-consistency testing.** Tools and techniques such as ALICE [Pillai14] and CrashMonkey [Mohan18]
enumerate the disk states a crash could leave and check that recovery handles each, which is the
systematic version of the crash-injection experiment chapter 12 needs.

## 3. Design space

| Method | What it catches | Cost | When to adopt |
|--------|-----------------|------|---------------|
| Unit tests | Local logic errors | Low | Already present |
| Fuzzing | Parser crashes, corruption handling | Low | With chapter 12 |
| Differential vs model | Logical correctness | Low-medium | With chapter 11 |
| Property/metamorphic | Invariant violations | Medium | Ongoing |
| Sanitizers (ASan/UBSan/TSan) | Memory/UB/race bugs | Low (CI time) | Now |
| Deterministic simulation | Distributed/fault bugs, reproducibly | High (architecture) | Early, before chapters 13-16 |
| TLA+ model checking | Protocol design bugs | Medium | With chapters 12, 14, 15 |
| Jepsen | Consistency violations in the real system | Medium | With chapters 14, 15 |

## 4. Notable researchers and key papers

- The FoundationDB team — FoundationDB and its deterministic simulation testing approach
  [FoundationDB21].
- Leslie Lamport — TLA+ and PlusCal [Lamport].
- Kyle Kingsbury — Jepsen and the Knossos/Elle consistency checkers [Kingsbury].
- Thanumalayan Pillai and colleagues — ALICE crash-consistency analysis [Pillai14].
- Jayashree Mohan and colleagues — CrashMonkey [Mohan18].

## 5. Concrete design for this codebase

The good news is that the seams deterministic simulation needs already largely exist. All device I/O goes
through `src/core/fs.hpp` (`random_access_file`, `append_only_file`), and all errors are values of
`core::status`, so a simulated backend behind `fs.hpp` can inject I/O errors, short writes, reordering,
and crashes deterministically without touching the storage code. Time goes through
`core::wall_clock_ms()` (which chapters 14 and 15 are already replacing with a clock the engine
controls), and the one uncontrolled randomness is the skiplist's `std::random_device` seed
(`src/storage/skiplist.hpp`), which must become a seed the test injects.

Concretely:

1. **Make nondeterminism injectable.** Route time, randomness, and I/O through interfaces the test
   supplies: a clock source, a seed source, and the `fs.hpp` backend. This is a small, high-value
   refactor that pays off across every later chapter.
2. **Build a simulated `fs.hpp` backend** that models a disk with controllable latency, fault injection
   at every operation, and a crash that discards un-fdatasynced data, so that chapter 12's crash-
   consistency experiment and chapter 04's data-loss-window experiment run deterministically.
3. **Build the differential harness** (chapter 11): run the engine and an in-memory versioned-map model
   under the same seeded operation stream and assert identical results, including under injected crashes
   and (later) concurrency.
4. **Add fuzz targets** for the SSTable and WAL decode paths (chapters 01, 12), so truncated and
   corrupt buffers are handled as `core::status` errors, never as crashes or undefined behavior.
5. **Turn on sanitizers** (ASan, UBSan, and TSan once threads exist) in continuous integration.
6. **Specify the protocols in TLA+** (chapter 12's recovery, chapter 14's Raft, chapter 15's
   transactions) and model-check them before trusting the implementations.
7. **Run Jepsen** against the distributed engine (chapters 14, 15) to check the advertised consistency
   under partitions and crashes.

## 6. C++, memory, and concurrency mechanics

The injectable seams are interfaces (or templates, following the project's concept style) that default to
the real implementation and are swapped for a deterministic one in tests; this keeps the production path
unchanged. The simulated `fs.hpp` backend records every write and models fdatasync so that a simulated
crash can present exactly the durable prefix, which is what makes crash recovery (chapter 12) testable.
Fuzz targets compile the decode functions standalone and assert they return `core::status` rather than
abort. ThreadSanitizer requires that all shared accesses go through atomics or locks (chapter 13), so it
also serves as an enforcement mechanism for chapter 13's discipline: a missing atomic shows up as a
reported race. Deterministic simulation requires the engine to be runnable on one thread with cooperative
scheduling, which is exactly the single-threaded-loop execution model chapter 13 recommends first, so the
two reinforce each other.

## 7. Risks, alternatives, and interactions

The main cost is architectural: deterministic simulation demands that nondeterminism be injectable, which
is cheap to add now and expensive to retrofit after concurrency and distribution exist, hence the
"adopt early" insistence. TLA+ checks the design but not the code, so it complements rather than replaces
implementation testing; Jepsen checks the running system but does not localize the bug, so it complements
the differential and deterministic methods that do.

There is no real fallback for correctness here: the alternative to these methods is shipping distributed
storage bugs that corrupt or lose data and cannot be reproduced. The only choice is ordering, and the
ordering is the subject of chapter 20.

Interactions: this chapter provides the test machinery that the experiment section of every other chapter
relies on, and its "adopt early" claim is the central sequencing argument of chapter 20.

## 8. Experiment plan

This chapter's "experiment" is to validate that the methods find planted bugs and reproduce them.

**Hypothesis.** With nondeterminism injectable, the deterministic simulator reproduces any failure from
its seed; fuzzing finds malformed-input crashes; the differential harness finds logical divergences; and
TLA+ finds a planted protocol-design flaw.

**Setup.** Plant known bugs (a missing fsync, an off-by-one in the footer decode, a tombstone dropped too
early, a Raft safety-rule omission) and confirm each method detects the relevant class and that the
deterministic simulator reproduces the failure exactly from its recorded seed.

**Metrics.** Detection (each planted bug is found by the intended method); reproducibility (the simulator
replays the failure deterministically); and the time to first failure for the fuzzers.

**Success criteria.** Every planted bug is detected by at least one method, and every simulated failure
is deterministically reproducible from its seed.

**Killer result.** None; correctness methodology is mandatory. The only adjustment is dropping a method
that finds nothing the others do not, to control test-suite cost.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section A (round-trip and fuzz tests) and the broader correctness needs of
Part IV.

1. Make time, randomness, and I/O injectable through seams that default to the real implementations.
2. Build the simulated `fs.hpp` backend with fault and crash injection.
3. Build the differential harness against a versioned-map model (chapter 11).
4. Add fuzz targets for the SSTable and WAL decode paths; turn on ASan/UBSan (and TSan with threads).
5. Specify recovery, Raft, and transactions in TLA+; run Jepsen against the distributed engine.

## Appendix: cache-replacement policy as its own research thread

This program groups one more cross-cutting research topic here, because it is methodology-adjacent and
gated on measurement rather than being a core data-structure choice: which block-cache replacement policy
to use. frankie has no block cache today (chapter 01, section 6 notes it relies on the operating-system
page cache), so this is greenfield.

A block cache holds recently read data blocks in memory so a repeated read does not go to disk. The
research question is the replacement policy: when the cache is full, which block to evict. The classic
policies are **LRU** (evict the least recently used), which is simple but fooled by a large scan that
floods the cache, and **LFU** (evict the least frequently used). Better policies balance recency and
frequency: **ARC** [MegiddoModha03] adapts between them; **LIRS** [JiangZhang02] uses reuse distance;
**W-TinyLFU** [Einziger] combines a frequency sketch with a small recency window and is what the Caffeine
cache uses; and **S3-FIFO** [Yang23] is a recent design built from simple FIFO queues that matches or
beats the adaptive policies on real traces while being far simpler to implement. The right choice depends
on the workload, especially its skew (chapter 03's Zipfian versus uniform), so it is decided by
measurement: build a block cache behind the read path, implement two or three policies behind a common
interface (the project's concept style), and measure hit rate and tail latency under the real traces from
chapter 03. S3-FIFO is the recommended first implementation because it is simple and strong; ARC and
W-TinyLFU are the comparisons. This belongs in the same harness as the rest of this chapter because, like
the correctness methods, it is only meaningful against realistic, reproducible workloads.
