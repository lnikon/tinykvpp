# 03 — Benchmark and measurement methodology

This chapter defines how every experiment in this program is run and reported. It is referenced by
the "Experiment plan" section of every Part II through VI chapter, so that those chapters can say
"measure write amplification under workload A from chapter 03" instead of redefining the setup each
time.

The chapter has two halves. The first half is the theory of measuring a storage engine correctly:
what to measure, how to drive load without lying to yourself, and how to control the cache. The
second half is the concrete harness to build in this repository, reusing what already exists.

## 1. Why benchmark before optimizing

The cost model in chapter 02 tells you which operations *could* be expensive, but not which ones
*are* expensive for a given workload on given hardware. An index lookup that is logarithmic in
theory may be invisible in practice because the data is cached and the cost is dominated by the disk
read that follows it. Optimizing code that is not on the critical path produces complexity with no
payoff, which is exactly the failure mode `TASKS.md` section I warns against with "Benchmark first.
Do not optimize until it is [the bottleneck]."

So the rule for this program is: every proposed optimization states, up front, the measurement that
would show it is needed, and the measurement that would show it is not worth keeping. Those
measurements are defined here.

## 2. What to measure

Four families of numbers describe a storage engine. An experiment should report whichever of them
its hypothesis is about, and should not report a single average where a distribution is meant.

**The three amplifications** (defined in chapter 02): write amplification, read amplification, and
space amplification. These are the structural costs and are usually what compaction, filter, and
key-value-separation experiments are about.

**Throughput**: operations per second, and bytes per second, at a sustained, steady state. Steady
state matters: the first few seconds of a run are warmup (empty caches, empty levels) and must be
excluded, and the run must be long enough that background compaction has reached its steady rate,
otherwise the number flatters the engine.

**Latency, as a distribution**: report the 50th, 99th, 99.9th, and maximum percentiles, not the
mean. The mean hides the tail, and the tail is what users feel. The standard tool for recording
latency distributions cheaply and correctly is a high-dynamic-range histogram such as HdrHistogram,
introduced by Gil Tene.

**Resource use**: CPU cycles per operation, memory footprint, and, where the device matters, actual
bytes transferred to and from the device (which is not the same as bytes the application wrote,
because of the page cache and the device's own behavior).

## 3. The coordinated-omission trap

The most common way a latency benchmark lies is **coordinated omission**, a problem named and
explained by Gil Tene. It happens when the load generator waits for one request to finish before
sending the next. If the engine stalls for one second, a closed-loop generator simply sends fewer
requests during that second and records only the few long ones, instead of the thousands of requests
that a real, rate-driven client would have tried to send and which would all have been delayed.
The recorded tail latency is then far better than the truth.

There are two correct responses, and an experiment must use one of them.

The first is **open-loop load**: send requests at a fixed target rate from a clock, independent of
when previous requests complete, and measure each request's latency from its intended send time, not
from when the generator got around to sending it. Under a stall, the queue of intended-but-unsent
requests grows, and their latencies grow with it, which is the truth.

The second is **closed-loop with correction**: keep the wait-for-completion generator, but when a
request takes longer than the intended inter-arrival gap, backfill the latencies of the requests
that should have been sent during the stall. HdrHistogram has built-in support for this correction.

Throughput-only experiments (for example "write amplification under a bulk load") can use a simple
closed loop, because they do not report latency. Any experiment that reports a latency percentile
must address coordinated omission and say which method it used.

## 4. Workload models

A benchmark result is only meaningful relative to a workload. This program uses three sources of
workload, from synthetic to real.

**YCSB**, the Yahoo! Cloud Serving Benchmark [Cooper10YCSB], defines a standard set of mixes that
cover most key-value patterns: workload A is 50% reads and 50% updates, B is 95% reads and 5%
updates, C is read-only, D reads the most recently inserted keys, E is short range scans, and F is
read-modify-write. Reporting against these mixes makes results comparable to the wider literature.

**Key and value distributions** must be stated separately from the mix. The key access distribution
is usually one of: uniform (every key equally likely, which stresses the disk and defeats caching),
Zipfian (a few hot keys dominate, which is realistic and rewards caching), or latest (recently
inserted keys are hottest, which stresses level 0). The value-size distribution matters for
key-value separation (chapter 10) and compression (chapter 09); real values are often small with a
heavy tail of large ones, so a fixed value size can mislead.

**Real traces** are the final check. Two well-documented public characterizations are the Facebook
study of RocksDB workloads by Cao and colleagues [Cao20RocksDB] and the Twitter cache-workload study
by Yang and colleagues [Yang20Twitter]. Replaying or imitating these is the closest a synthetic
benchmark gets to reality, and they are the source of the "small values, Zipfian, with bursts"
intuition that the synthetic knobs above are trying to reproduce.

## 5. Controlling the cache: cached versus uncached

Chapter 02, section 8, established that a result is uninterpretable unless it states whether the data
was in the operating system page cache. The harness must therefore be able to run an experiment in
each regime deliberately.

To measure **uncached** (disk-bound) behavior, use one of: a dataset much larger than the machine's
memory so that most reads miss the cache naturally; dropping the page cache between phases; or
opening files with O_DIRECT (`src/core/fs.hpp:open_flag::direct`, currently unused) so reads bypass
the cache entirely. The third is the most controlled and is the one chapter 04 and chapter 17 build
toward anyway.

To measure **cached** (CPU-bound) behavior, use a dataset that fits in memory and warm it with a full
read pass before measuring, so the disk never appears and the experiment isolates in-memory work such
as index-structure speed (chapter 07) or comparator cost.

Many experiments should report both, because the right structure can differ between the two regimes.

## 6. Statistical hygiene

Three rules keep the numbers honest. Run each configuration several times and report the spread, not
one number, because a single run can be perturbed by other activity on the machine. Separate warmup
from steady state explicitly and exclude warmup. Pin the experiment to fixed CPU frequencies where
possible (disable frequency scaling) so that runs are comparable, because otherwise the processor's
own power management adds noise larger than the effect being measured.

## 7. The concrete harness to build in this repository

The project already depends on the Google Benchmark library and uses it for micro-benchmarks:
`test/skiplist_benchmark.cpp` and `test/memtable_benchmark.cpp` build a key pool, insert into the
structure, and report items processed per second. That is the right tool for the in-memory,
cached-regime micro-experiments (chapters 07 and 08). It is not enough for engine-level experiments,
which need amplification numbers and disk behavior.

The missing piece, and the first concrete deliverable this chapter calls for, is an **engine-level
workload driver**. Its shape:

- It constructs an `engine` (`src/engine/engine.cpp:engine::create`) on a real directory.
- It generates a request stream from a chosen YCSB mix, key distribution, and value-size
  distribution (section 4), with a fixed seed so runs are reproducible.
- It drives the engine open-loop for latency experiments (section 3) and closed-loop for
  throughput-only experiments.
- It reports the four families of numbers from section 2.

Computing the amplifications requires instrumenting the I/O layer, which is cheap because all device
I/O already funnels through two types. **Write amplification**: wrap or count inside
`src/core/fs.hpp:random_access_file::write` and `append_only_file::append` to total the bytes written
to disk, and divide by the bytes the workload asked to store. **Read amplification**: count calls to
`random_access_file::read` per user `get`. **Space amplification**: sum the sizes of the SSTable
files on disk and divide by the live bytes the workload believes it stored (which the driver knows,
because it generated the data). To separate application bytes from true device bytes, use
`blktrace`/`iostat` at the device level for the experiments where that distinction matters (chapter
17), but the application-level counts above are enough for comparing compaction policies.

Supporting tools, all standard on the target platform: `perf` for CPU profiling and cache-miss
counts, `eBPF`-based tools for off-CPU and syscall analysis, `blktrace` for device-level I/O, and an
HdrHistogram implementation for latency (section 2). The fault-injection seam over `fs.hpp` that
chapter 19 builds also belongs to this harness, because deterministic, reproducible workloads are a
prerequisite for both benchmarking and testing.

## 8. The output contract

Every experiment in this program reports, at minimum: the workload (mix, key distribution, value
sizes), the cache regime (cached or uncached, and how it was enforced), the load method (open or
closed loop), the hardware and the dataset size, the metric distributions (not just means), and the
seed. An experiment that omits these cannot be reproduced or compared, and an unreproducible result
is treated as no result.

## 9. Notable people and references

- Gil Tene — coordinated omission and HdrHistogram.
- Brian Cooper and colleagues — YCSB [Cooper10YCSB].
- Zhichao Cao and colleagues — characterization of RocksDB workloads at Facebook [Cao20RocksDB].
- Juncheng Yang and colleagues — characterization of Twitter's cache workloads [Yang20Twitter].

With the foundation in place — the code (chapter 01), the theory (chapter 02), and the measurement
method (this chapter) — the Part II chapters can now each state a precise problem, design a solution,
and define the experiment that decides whether it works.
