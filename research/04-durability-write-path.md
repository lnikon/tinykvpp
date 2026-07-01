# 04 — Durability and the write path

Depends on: chapter 01 (the write path and the file abstractions), chapter 02 (the cost model),
chapter 03 (how to measure). This chapter follows the nine-section depth template.

## 1. Problem statement

Every write in frankie pays for a full, synchronous flush to stable storage, one per operation. The
write path is `src/engine/engine.cpp:engine::put`, which calls `wal_.append(...)`, which is
`src/engine/wal.cpp:wal_writer::append`, which calls `file_.append_fsync(encoded_entry)` on the
underlying `src/core/fs.hpp:append_only_file`. The `append_fsync` call writes the bytes and then
issues an `fdatasync`, blocking the calling thread until the device confirms the data is durable.

That is the strongest durability a single write can have, and the slowest. The thread does nothing
but wait for the device on every `put` and every `del`. There is no batching: ten writes pay for ten
device round trips even if they arrive together. The I/O itself is also fully synchronous and
blocking throughout `src/core/fs.hpp`; there is no asynchronous submission, and the `O_DIRECT` flag
(`open_flag::direct`) is defined but never used. The project's stated intent in `README.md` is an
asynchronous, io_uring-based main loop, but the code is the opposite of that today.

The result is that write throughput is bounded by device flush latency, not by device bandwidth, and
the CPU is idle while it waits. This chapter is about closing that gap without giving up the
durability the user actually needs.

## 2. Background and theory

To reason about this, you must know exactly what a write goes through and where it can be lost.

When the application writes bytes to a file, they first sit in a buffer the application controls. An
ordinary `write` system call copies them into the operating system's **page cache**, in memory. At
that point the data survives an application crash but not a power loss, because it is still only in
volatile memory. An `fsync` (flush data and metadata) or `fdatasync` (flush data and only the
metadata needed to read it back) system call forces the page cache contents down to the device.
Even then, the device may hold them in its own volatile write cache, so true durability also requires
the device to honor a cache-flush command, which `fdatasync` issues. Only after that are the bytes on
the physical media.

The cost of a flush is one device round trip. On a spinning disk this is on the order of milliseconds.
On an NVMe SSD it is on the order of tens of microseconds. Either way it is enormous compared to the
cost of copying a few hundred bytes into the page cache. So a design that flushes once per write is
paying the most expensive cost in the stack on every operation.

The classic fix is **group commit**, an idea from the file-system and database literature
[Hagmann87]. Instead of flushing after each record, the engine collects all the records that arrive
in a short window into one buffer, writes them with a single `write`, and issues a single
`fdatasync` that makes all of them durable at once. If `k` writes share one flush, the per-write
flush cost drops by a factor of `k`. The price is latency: a write must wait for the batch to close
before it is acknowledged. This is a direct read/update tradeoff in the RUM sense (chapter 02): you
spend a little latency to buy a lot of throughput.

A second, orthogonal fix is **asynchronous I/O**. With synchronous blocking calls, the thread that
issues a flush can do nothing else until it returns. With asynchronous I/O, the thread submits the
operation and continues, learning of completion later. The modern Linux interface for this is
**io_uring** [Axboe], in which the application places submission entries on a ring shared with the
kernel and reaps completion entries from another ring, avoiding a system call per operation and
allowing many operations to be in flight at once. io_uring can also chain operations, so a write and
its following flush can be submitted as a linked pair.

A third axis is **how durable to be at all**. The spectrum runs from "fdatasync every write" (lose
nothing) through "fdatasync every few milliseconds" (lose at most that window on power loss) to "rely
on the operating system to flush eventually" (lose whatever was in the page cache). The right point
is a policy decision the engine should expose, not hard-code, because different users accept
different data-loss windows.

The correctness backbone underneath all of this is **write-ahead logging**, formalized by the ARIES
recovery method [Mohan92]: the log record for a change must be durable before the change is
considered committed, so that recovery can always replay forward from the log. frankie already
follows the write-before-apply ordering (chapter 01, section 3); the question here is only how
efficiently the log is made durable, not whether it is.

## 3. Design space

| Approach | Throughput | Per-write latency | Data-loss window | Complexity |
|----------|------------|-------------------|------------------|------------|
| fsync per write (today) | Low | One device flush | None | Lowest |
| Group commit (sync) | High | One flush per batch | None | Low |
| Periodic fsync | Highest | None (async ack) | Last interval | Low |
| io_uring batched write+flush | High | One flush per batch | None | Medium |
| O_DIRECT + own writeback | High, predictable | Tunable | Tunable | High |

The two cheap, high-value moves are group commit (keeps full durability, large throughput gain, low
complexity) and a configurable periodic-fsync mode (highest throughput, bounded loss). The io_uring
and O_DIRECT moves are larger investments that also serve chapters 05 and 17.

## 4. Notable researchers and key papers

- Robert Hagmann — group commit in the Cedar file system [Hagmann87].
- C. Mohan and colleagues — ARIES write-ahead logging and recovery [Mohan92].
- Jens Axboe — io_uring, the Linux asynchronous I/O interface [Axboe].
- Diego Didona and colleagues — a careful study of modern Linux storage APIs including io_uring and
  their performance characteristics [Didona22].

## 5. Concrete design for this codebase

The design has three layers, each independently shippable.

**Layer one: group commit on the existing synchronous path.** Change `wal_writer` so that `append`
no longer flushes. Instead it appends the encoded record to an in-memory commit buffer and returns a
commit ticket (a monotonically increasing position). A new method `wal_writer::commit()` writes the
whole buffer with one `append` and issues one `fdatasync`, and returns the durable position. The
engine decides when to call `commit()`: in the simplest single-threaded form, it calls `commit()`
once per externally driven batch of writes, or after each write when the caller asks for synchronous
durability. The buffer is an arena allocation (chapter 01, section 6) so there is no per-write heap
traffic. This alone removes the per-write flush without any new threads or kernel features.

Proposed signatures, integrating with the existing `core::status` model:

```
// wal.hpp
std::expected<commit_ticket, core::status> append(const wal_entry &entry) noexcept; // buffers only
std::expected<durable_position, core::status> commit() noexcept;                     // one write + one fdatasync
```

**Layer two: a durability policy in config.** Add a `durability` knob to `src/core/config.hpp` with
three modes: `sync` (commit after every write, today's behavior), `group` (commit when the buffer
reaches a size or a small time bound), and `interval` (commit on a timer, accepting a bounded loss
window). The engine consults it to decide when to call `commit()`. This makes the data-loss window an
explicit, documented choice rather than an accident.

**Layer three: io_uring as the I/O backend.** Introduce an async file backend behind the same
`append_only_file` and `random_access_file` interfaces, so callers do not change. Internally it uses
`liburing` to submit a write and a linked flush as one operation, and to keep several SSTable-flush
and compaction writes (chapter 05) in flight at once. Completion is delivered through the engine's
main loop. This is where the README's design is finally realized, and it is the layer that also
benefits flush and compaction, not just the WAL.

`O_DIRECT` (`open_flag::direct`) becomes useful at layer three, for the large sequential SSTable
writes where bypassing the page cache avoids polluting it with data that will not be read soon. Note
the constraint it imposes (section 6).

## 6. C++, memory, and concurrency mechanics

The commit buffer must outlive the asynchronous operation that writes it. With synchronous group
commit this is trivial (the buffer is alive across the blocking `commit()` call). With io_uring it is
not: the kernel may complete the write after the C++ call returns, so the bytes must remain valid and
unmoved until the completion is reaped. This rules out the scratch arena (chapter 01, section 6),
whose `realloc` can move memory; the commit buffer must be a stable block arena allocation or a
registered buffer. io_uring's **registered buffers** feature lets the application pin a set of
buffers with the kernel once and reuse them, avoiding per-operation setup; the commit buffer is a
natural candidate.

`O_DIRECT` requires that the buffer address, the file offset, and the length all be aligned to the
device's logical block size, typically 512 or 4096 bytes. The arena aligns blocks to 64 bytes
(`kBlockAlignment`), which is not enough; an O_DIRECT path needs a separate allocation aligned to
4096 (via `posix_memalign` or an arena variant with that alignment). This is a concrete, testable
requirement, not a vague caution.

If layer three runs the actual I/O on a separate thread or via kernel completions while the main
thread continues, then the commit buffer and the completion state are shared across threads, and
their handoff needs the correct `std::memory_order` (chapter 13 explains the model). The simplest
safe design keeps submission and completion on the same thread as the main loop and uses io_uring
purely to overlap I/O, which avoids shared-memory synchronization entirely; this is the recommended
first form and is why chapter 13's single-threaded-loop option pairs naturally with this chapter.

Coroutines (C++20, chapter 13) are the clean way to express "submit the flush, suspend, resume when
durable" so that the write path still reads like straight-line code. They are optional; an explicit
state machine works too, at the cost of more manual bookkeeping.

## 7. Risks, alternatives, and interactions

The main risk of group commit is added latency for a write that is alone in its window; a small
maximum batch-close time bounds this. The main risk of interval mode is the data-loss window, which
is why it is opt-in and documented. The main risk of io_uring is complexity and a dependency on a
recent kernel; the mitigation is that layers one and two deliver most of the throughput gain without
it, so io_uring can be deferred until its other beneficiaries (chapter 05 compaction, chapter 17
hardware) make it worthwhile.

The simplest fallback, if io_uring does not pay off on the target hardware, is to keep layers one and
two: synchronous group commit plus a durability policy already removes the per-write flush, which is
the dominant cost.

This chapter interacts with chapter 05 (compaction also wants asynchronous, batched writes), chapter
12 (the log's checksum and torn-write handling are part of durability), and chapter 13 (the execution
model determines whether the I/O backend is single-threaded or shared).

## 8. Experiment plan

**Hypothesis.** Group commit increases sustained write throughput by a large factor (target: at
least 5x on NVMe) with no loss of durability, at the cost of a bounded increase in single-write p99
latency.

**Setup.** Use the engine-level driver from chapter 03. Workload: a write-heavy load (YCSB load
phase and YCSB-A) with small values and a uniform key distribution, run to steady state. Hardware and
dataset size reported per chapter 03's output contract.

**Baseline.** Today's per-write `append_fsync`.

**Variants.** Synchronous group commit with batch sizes of 1 (equals baseline), 8, 64, 256; interval
mode at 1 ms and 10 ms; io_uring batched, if layer three exists.

**Metrics.** Sustained writes per second; write latency distribution (p50/p99/p999, open-loop per
section 3 to avoid coordinated omission); and, for interval mode, the measured worst-case data-loss
window via crash injection (chapter 19).

**Success criteria.** Group commit reaches the target throughput multiple while keeping p99 within a
stated bound; durability is confirmed unchanged by the crash-injection test for `sync` and `group`
modes.

**Killer result.** If throughput is already bandwidth-bound rather than flush-bound on the target
device (so batching does not help), or if the latency cost of batching exceeds the throughput benefit
for the intended workload, then only the io_uring overlap (not batching) is worth keeping.

## 9. Implementation checklist

Cross-referenced to `TASKS.md` section M (Async I/O and execution model).

1. Split `wal_writer::append` into buffering plus `wal_writer::commit()`; move the `fdatasync` into
   `commit()`. Keep the commit buffer in a stable block arena, not the scratch arena.
2. Add a `durability` mode to `src/core/config.hpp` and consult it from `engine::put`/`engine::del`.
3. Add crash-injection support to the test harness (shared with chapter 19) to verify the data-loss
   window of each mode.
4. Introduce an io_uring-backed implementation behind `append_only_file`/`random_access_file`, with
   registered buffers, and an O_DIRECT-aligned buffer path for large SSTable writes.
5. Run the experiment in section 8 and record the results in this chapter.
