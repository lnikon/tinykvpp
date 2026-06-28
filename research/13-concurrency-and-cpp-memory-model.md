# 13 — Concurrency and the C++ memory model

Depends on: chapter 01 (the single-threaded code and the arenas), chapter 04 (async I/O), chapter 05
and chapter 11 (the shared reference counts that first cross threads). Follows the nine-section depth
template. This chapter is deliberately heavy on C++ and memory-model detail, because correctness in a
concurrent storage engine depends on details that even experienced C++ programmers rarely need.

## 1. Problem statement

frankie is single-threaded. There are no atomics and no locks in the data structures, and the arenas
are not thread-safe (chapter 01, section 9). The project intends, per `README.md`, a single main loop
with asynchronous I/O rather than a pool of threads. But two pressures push on that. First, a single
thread cannot use more than one CPU core for the data path, so to scale on a multicore machine the
engine needs some concurrency model. Second, features already designed in earlier chapters introduce
shared mutable state across activities that will eventually run concurrently: the segment reference
counts (chapters 05 and 11) and the snapshot registry (chapter 11) are touched by both foreground
reads and background compaction. The engine therefore needs a decided, written-down concurrency model
(`TASKS.md` section G asks for exactly this), and if any shared memory is involved it needs to be
correct under the C++ memory model.

## 2. Background and theory

### 2.1 Three execution models

**Single-threaded event loop with asynchronous I/O.** One thread runs a loop that submits I/O and
processes completions, overlapping I/O with computation but using one core for the data path. This is
the README's model and the simplest. It removes all shared-memory concurrency from the data path,
because there is only one thread touching the structures; the only concurrency is between the
application and the kernel, mediated by io_uring (chapter 04). It scales vertically on one core and
relies on the disk, not the CPU, being the limit.

**Thread-per-core, shared-nothing.** The keyspace is partitioned into shards, and each core owns one
shard together with its own memtables, SSTables, and arenas. Cores do not share data structures; they
communicate by passing messages. This is the Seastar and ScyllaDB model [Kivity]. Its great advantage
here is that each shard runs the existing single-threaded code unchanged, so no concurrent data
structure is needed at all; the concurrency is pushed up to the routing layer that sends each request
to the right shard. Its disadvantage is load imbalance (a hot shard overloads one core) and the
difficulty of operations that span shards (a scan across the whole keyspace, or a transaction touching
two shards, chapter 15).

**Shared-memory multithreading.** Several threads share the same structures, coordinated by locks or
by lock-free algorithms. This extracts the most from a machine but is by far the hardest to get right,
because it requires reasoning under the C++ memory model.

### 2.2 The C++ memory model, from first principles

When two threads access the same memory and at least one writes, and the accesses are not synchronized,
the program has a **data race**, and a data race is undefined behavior in C++: the compiler is allowed
to assume it never happens, so the program may do anything. This is not a theoretical worry; compilers
exploit the assumption to reorder and cache memory operations. So any memory shared between threads
must be accessed through synchronization, and the standard tool is `std::atomic`.

The model is built on an ordering relation called **happens-before**. Within one thread, statements are
**sequenced-before** each other in program order. Across threads, an atomic operation can
**synchronize-with** another, creating a happens-before edge that makes one thread's prior writes
visible to the other. If a write happens-before a read, the read sees that write; if neither happens
before the other and they conflict, it is a race.

Atomic operations take a `std::memory_order` argument that selects how much ordering they create, from
strongest and slowest to weakest and fastest:

- `seq_cst` (sequentially consistent) is the default. All `seq_cst` operations across all threads
  appear in one single global order. It is the easiest to reason about and the most expensive, because
  it often requires a full hardware fence.
- `acquire` (on a load) and `release` (on a store) form the workhorse pair. A release store
  synchronizes-with an acquire load that reads its value, and everything the storing thread did before
  the release becomes visible to the loading thread after the acquire. This is exactly what is needed
  to publish a data structure: build it, then release-store a pointer to it; the reader acquire-loads
  the pointer and then safely reads the structure.
- `acq_rel` combines both for read-modify-write operations.
- `relaxed` provides atomicity (no torn values) but no ordering at all. It is correct only when the
  ordering is provided some other way, or when only the atomicity matters (for example incrementing a
  reference count while you already hold a reference).

The practical rule for this codebase: default to `seq_cst` until a benchmark shows an atomic is hot,
then weaken it only with an explicit argument for why the weaker order is still correct. Most of the
engine's shared state (reference counts, the snapshot registry) is not on the hottest path, so
`seq_cst` is fine and the weakening is rarely needed.

### 2.3 Lock-free structures and the reclamation problem

A structure is **lock-free** if some thread always makes progress even if others stall, and
**wait-free** if every thread makes progress in a bounded number of steps. Lock-free structures avoid
the latency spikes that a thread holding a lock and then being descheduled can cause. They are built
from atomic compare-and-swap operations.

They have two classic hazards. The **ABA problem**: a compare-and-swap sees a pointer change from A to
B and back to A and wrongly concludes nothing changed. The deeper and more important one is **safe
memory reclamation**: in a lock-free structure a thread may be about to read a node that another thread
has just unlinked and wants to free; freeing it immediately would be a use-after-free. The whole
difficulty of lock-free programming is mostly this question of when it is safe to free. There are three
standard answers:

- **Epoch-based reclamation** [Fraser04]. There is a global epoch counter. A thread entering a critical
  region records the current epoch; freed nodes are placed on a retire list tagged with the epoch; a
  node is actually freed only once every thread has advanced past its epoch, which proves no thread can
  still hold it.
- **Hazard pointers** [Michael04]. Before using a shared pointer a thread publishes it in a per-thread
  hazard slot; a node is freed only if no thread has published it. More precise than epochs, with more
  per-access overhead.
- **Read-copy-update (RCU)** [McKenney]. Readers proceed with no synchronization at all; a writer makes
  a new version and waits for a grace period (until all pre-existing readers have finished) before
  freeing the old one. Dominant in the Linux kernel.

The point for this project is that the arena (chapter 01, section 6) already defers all freeing to a
single bulk `destroy`, which is reclamation-friendly: a lock-free structure over an arena does not free
individual nodes anyway, so the reclamation question reduces to "when is it safe to bulk-free the whole
arena", which is a much easier question than per-node freeing.

### 2.4 Coroutines

A C++20 **coroutine** is a function that can suspend and later resume, keeping its locals across the
suspension in a compiler-allocated frame. They let asynchronous code read like straight-line code:
"submit the read, suspend, resume when the completion arrives, use the result". For the io_uring loop
(chapter 04) a coroutine task type lets each request be written sequentially while the loop multiplexes
many of them. The cost is the frame allocation per coroutine and the care needed around lifetimes (the
frame must outlive the suspension). They are an ergonomic tool, not a concurrency model by themselves;
they pair with the single-threaded loop or with thread-per-core.

## 3. Design space

| Model | Scaling | Complexity | Correctness risk | Fit with current code | Reclamation needed |
|-------|---------|------------|------------------|-----------------------|--------------------|
| Single loop + async I/O | One core | Lowest | Lowest | Runs as-is | No |
| Thread-per-core | All cores | Medium | Low (no shared structures) | Runs per shard as-is | No |
| Shared-memory, locks | All cores | Medium-high | Medium | Needs locks added | Lock-guarded |
| Shared-memory, lock-free | All cores | Highest | Highest | Needs rewrite | Yes (EBR/HP/RCU) |

The recommended path is **single loop with async I/O first** (it is the README's design and ships the
most value soonest), then **thread-per-core sharding** to use all cores, because it reuses the
single-threaded code unchanged and avoids concurrent data structures entirely. Shared-memory lock-free
structures are a last resort, justified only if sharding's load imbalance proves unfixable.

## 4. Notable researchers and key papers

- Hans Boehm, Sarita Adve — the foundations of the C++ concurrency memory model [BoehmAdve08].
- Anthony Williams — the standard practitioner reference on C++ concurrency [Williams].
- Keir Fraser — epoch-based reclamation [Fraser04].
- Maged Michael — hazard pointers [Michael04], and the Michael-Scott lock-free queue [MichaelScott96].
- Paul McKenney — read-copy-update (RCU) [McKenney].
- Maurice Herlihy, Nir Shavit — the theory of concurrent objects and progress conditions [HerlihyShavit].
- Avi Kivity and colleagues — Seastar, the thread-per-core shared-nothing framework [Kivity].

## 5. Concrete design for this codebase

**Primary path: single loop now, sharding next.** Build the asynchronous single-threaded loop from
chapter 04 first, using coroutines for request handling. Then introduce a sharding layer: hash or
range-partition the keyspace into N shards, run one engine instance per shard pinned to one core, and
route each request to its shard. Each shard is the existing single-threaded engine, arenas and all,
with no change to the data structures. Cross-shard scans (chapter 11) fan out to the shards and merge
their results; cross-shard transactions are deferred to chapter 15.

**The few genuinely shared things.** The segment reference counts (chapters 05 and 11) and the snapshot
registry (chapter 11) are the only state touched by more than one activity, and only when background
compaction or garbage collection runs concurrently with reads. Make these `std::atomic` from the start
with documented ordering (section 6), so the single-threaded path pays nothing and the move to a
background thread does not require a rewrite.

**If shared-memory is ever needed.** The skiplist has a well-known lock-free form, and because it lives
in an arena, the reclamation problem reduces to bulk-freeing the arena at flush time under an epoch
guard rather than per-node freeing. This is the cleanest place to apply epoch-based reclamation if
sharding proves insufficient.

**Write the model down.** Document the chosen concurrency model in the code and in the architecture
walkthrough (chapter 01, section 9, and `TASKS.md` section G), including which state is shared and what
orders its accesses, so the assumptions are explicit rather than implied by the absence of locks.

## 6. C++, memory, and concurrency mechanics

The reference-count pattern is the concrete case to get right, and it is the same one `std::shared_ptr`
uses. Incrementing the count while you already hold a reference can be `relaxed`, because the existing
reference already establishes the needed ordering. The final decrement that may free the object must be
`acq_rel` (or a `release` decrement followed by an `acquire` fence before the free), so that all uses
of the object on all threads happen-before the free. Getting this wrong is a use-after-free that
appears only under load, which is why it is written here explicitly rather than left to intuition.

The arenas stay per-shard and are never shared, which is what keeps the single-threaded code valid
under thread-per-core; the `rebind_arena` discipline (chapter 01, section 6) continues to apply within
a shard. Per-core structures should be padded to a cache line to avoid **false sharing**, where two
cores contend on the same cache line even though they touch different variables; the arena already
aligns its blocks to 64 bytes (`kBlockAlignment`), and per-core engine state should be aligned the same
way.

Coroutines need a task type integrated with the io_uring completion handling from chapter 04; the
coroutine frame for an in-flight request must live until its I/O completes, which is the same lifetime
rule as the registered I/O buffers in chapter 04, and the scratch arena's move hazard means nothing a
suspended coroutine holds may live in the scratch arena across a suspension.

## 7. Risks, alternatives, and interactions

Thread-per-core's risk is load imbalance: a hot key range overloads its core while others idle; the
mitigations are good partitioning (chapter 16) and, in the worst case, splitting hot shards.
Shared-memory lock-free code's risk is that its bugs are rare, load-dependent, and nearly impossible to
reproduce by hand, which is the strongest argument for adopting the deterministic testing of chapter 19
before writing any such code. Coroutine frame allocation can be a cost on the hottest path and may need
a custom allocator.

The fallback is the single-threaded asynchronous loop: it is correct by construction (no shared data-
path state) and, because storage engines are often disk-bound, one core driving io_uring can saturate a
device, in which case multicore data-path concurrency buys nothing and should not be built.

Interactions: this chapter decides whether chapters 05, 10, and 11's reference counts are plain or
atomic; it underpins all of Part IV (a distributed node is concurrent); and it is the chapter whose
bugs most require chapter 19's methods.

## 8. Experiment plan

**Hypothesis.** A single core driving asynchronous I/O saturates the target device for the intended
workload; if it does not, thread-per-core sharding scales throughput nearly linearly with cores until
load imbalance or cross-shard work dominates.

**Setup.** Engine-level driver (chapter 03). First measure single-core throughput against device
bandwidth. Then, if the core is the limit, measure sharded throughput from 1 to N cores under uniform
and Zipfian key distributions (the latter creates hot shards).

**Baseline.** Single-threaded synchronous (today), then single-threaded asynchronous.

**Variants.** Thread-per-core at increasing core counts; uniform versus skewed keys.

**Metrics.** Throughput versus cores; per-core utilization (to expose imbalance); and, for any lock-
free structure built, a stress test plus the chapter 19 deterministic schedule explorer to find races.

**Success criteria.** The experiment establishes whether the device or the core is the limit, and if
the core, whether sharding scales acceptably under realistic skew.

**Killer result.** If one core already saturates the device, no multicore data-path concurrency is
built, and the effort goes to async I/O and durability (chapter 04) instead.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section G (document the concurrency model) and section M (execution
model).

1. Make the segment reference counts and snapshot registry `std::atomic` with documented ordering, even
   while single-threaded.
2. Build the single-threaded asynchronous loop with coroutines over io_uring (chapter 04).
3. Measure single-core throughput against device bandwidth (section 8) before building anything
   multicore.
4. If needed, add a thread-per-core sharding layer running the existing engine per shard, with a
   routing and cross-shard-merge layer.
5. Document the concurrency model in the code and in chapter 01.
