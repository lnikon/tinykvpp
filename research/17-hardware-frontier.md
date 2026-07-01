# 17 — Hardware frontier

Depends on: chapter 01 (the file abstractions, the seam where backends plug in), chapter 04 (io_uring
and O_DIRECT), chapter 13 (NUMA pairs with thread-per-core), chapter 14 and chapter 16 (RDMA serves the
network). Follows the nine-section depth template.

## 1. Problem statement

frankie uses synchronous POSIX file I/O (`src/core/fs.hpp`), relies on the operating system page cache,
leaves `O_DIRECT` unused, and uses SIMD only to compare keys (`src/core/simd.hpp`). Modern storage and
interconnect hardware offers capabilities that this design ignores, and some of them are an unusually
good fit for an LSM-tree because its writes are append-only and its files are immutable. This chapter
surveys those capabilities, says which fit this design and why, and identifies the one seam,
`src/core/fs.hpp`, where most of them plug in without disturbing the rest of the engine.

## 2. Background and theory

**io_uring**, covered for durability in chapter 04, is the foundation: a Linux interface where the
application submits I/O on one ring and reaps completions on another, with optional features such as
registered (pre-pinned) buffers and files, a polled completion mode, and a kernel-side submission
thread (SQPOLL) that together remove most per-operation system-call overhead and keep many operations
in flight. It applies to sockets as well as files, so it unifies the storage and network I/O paths.

**Zoned storage (ZNS SSDs)** [Bjorling21ZNS] is the strongest fit. A normal SSD hides a flash
translation layer that remaps writes and runs its own garbage collection, which causes write
amplification and unpredictable latency the application cannot control. A ZNS SSD instead exposes its
storage as zones that must be written sequentially and erased as a whole, and it does no hidden garbage
collection. This matches an LSM-tree almost exactly: an SSTable is written once, sequentially, and never
modified, so an SSTable maps onto a zone, and compaction's reclamation of old SSTables maps onto erasing
whole zones. The application's append-only file (`src/core/fs.hpp:append_only_file`) is already shaped
like a zone append. The payoff is control over garbage collection, lower and more predictable write
amplification, and better tail latency. ZenFS is the RocksDB backend that demonstrates this.

**Persistent memory and CXL.** Persistent memory is byte-addressable non-volatile memory: the CPU reads
and writes it with ordinary load and store instructions, and the data survives power loss, with
persistence made durable by cache-flush instructions. LSM designs have used it for the memtable or the
upper levels or the log, removing block I/O from the write path (NoveLSM [Kannan18NoveLSM], MatrixKV).
The specific product that popularized it (Intel Optane) has been discontinued, but the live direction is
**CXL**, a cache-coherent interconnect that lets a host attach, expand, pool, and share memory across
machines, which reopens the same byte-addressable-far-memory design space.

**RDMA and kernel bypass.** Remote direct memory access lets one machine read or write another
machine's memory without involving the remote CPU, at far lower latency than a TCP round trip. It is a
natural transport for the replication and routing traffic of chapters 14 and 16. DPDK is the related
technology for user-space packet processing.

**Computational (near-data) storage** pushes computation, such as filtering or part of a compaction,
into the storage device, so that less data has to move across the bus to the CPU. SmartSSDs are the
commercial form.

**GPU offload** uses the massive parallelism of a GPU for bulk operations that suit it: sorting,
merging during compaction, compression, and building filters.

**NUMA awareness.** On a multi-socket machine, memory attached to a socket is faster for that socket's
cores than memory attached to another socket. Pinning a thread and its memory to one socket avoids the
penalty, which pairs directly with the thread-per-core design of chapter 13.

**SIMD beyond comparison.** The project already uses SIMD for key comparison; the same instruction
classes accelerate scanning, decompression (chapter 09), filter probing (chapter 06), and hashing.

## 3. Design space

| Technology | What it helps | Maturity / portability | Fit with this design | Complexity |
|------------|---------------|------------------------|----------------------|------------|
| io_uring | All I/O throughput/latency | High (recent kernels) | Excellent | Medium |
| ZNS SSD | Write amp, tail latency | Medium (specific drives) | Excellent (append-only) | High |
| PMEM / CXL | Write-path latency, far memory | Low/flux | Good for memtable/WAL | High |
| RDMA / DPDK | Replication/network latency | Medium (specific NICs) | Good for Part IV | High |
| Computational storage | Data movement | Low | Niche (offload compaction) | High |
| GPU | Bulk sort/compress/filter | Medium | Niche (batch jobs) | High |
| NUMA-aware placement | Multicore memory latency | High | Good (with sharding) | Low |
| Extended SIMD | Scan/decompress/filter/hash | High | Good | Low |

The recommended order: io_uring first (portable, high value, already needed by chapter 04); then a ZNS
backend (the highest-fit, most distinctive bet for an LSM); NUMA-aware placement alongside chapter 13's
sharding; extended SIMD as cheap wins; and RDMA, PMEM/CXL, computational storage, and GPU as conditional
bets gated on hardware availability and measured need.

## 4. Notable researchers and key papers

- Jens Axboe — io_uring [Axboe].
- Matias Bjorling and colleagues — Zoned Namespaces (ZNS) and ZenFS [Bjorling21ZNS].
- Sudarsun Kannan and colleagues — NoveLSM, persistent-memory LSM [Kannan18NoveLSM].
- The CXL Consortium specifications — Compute Express Link.
- DPDK and RDMA verbs ecosystems for kernel-bypass networking.

## 5. Concrete design for this codebase

The key architectural point is that `src/core/fs.hpp` is the seam. The two interfaces
`random_access_file` and `append_only_file` already abstract the storage device, so new backends slot in
behind them without touching the storage data structures.

**io_uring backend** (shared with chapter 04): an asynchronous implementation behind both interfaces,
with registered buffers, serving the WAL, flush, compaction, and (for Part IV) the network.

**ZNS backend**: an `append_only_file` implementation over a zoned block device (via the Linux zoned
block APIs), where each SSTable fills a zone and compaction resets whole zones. Because the SSTable
writer (`src/storage/sstable_writer.cpp`) already writes sequentially and never rewrites, little above
the file layer changes; the segments manager (chapter 05) gains a zone-to-segment mapping and reclaims
by zone.

**PMEM/CXL**: place the WAL or the memtable in byte-addressable persistent memory, so the write path
uses loads and stores plus cache flushes instead of block writes; this is a variant of the memtable
arena (chapter 01) backed by persistent memory.

**RDMA**: a transport implementation for the chapter 14 and chapter 16 message layer.

**NUMA**: allocate each shard's arenas from its socket's local memory and pin the shard's thread to that
socket (chapter 13).

**SIMD**: extend `src/core/simd.hpp` with vectorized routines for decompression assistance, filter
probing, and hashing, using the same compile-time target-attribute approach already in the file.

## 6. C++, memory, and concurrency mechanics

io_uring is used via `liburing`; ZNS via the zoned block device interface (for example `libzbd`); PMEM
via a persistent-memory library (the PMDK family) with explicit flush and fence for durability; NUMA via
`libnuma` plus thread pinning. O_DIRECT and ZNS both impose alignment requirements (chapter 04, section
6): buffers and offsets aligned to the device block size, which the 64-byte arena alignment does not
satisfy, so a 4096-aligned arena variant is needed. SIMD continues to use the project's existing
target-attribute pattern (`__attribute__((target(...)))` as in `src/core/simd.hpp`) with runtime CPU
feature detection. None of these change the immutable read side's freedom from concurrency, except RDMA
and io_uring completions, which are handled on the loop or thread that owns them (chapter 13).

## 7. Risks, alternatives, and interactions

The dominant risk is hardware availability and portability: ZNS, PMEM, RDMA, computational storage, and
GPUs each require specific hardware, so each must sit behind the `fs.hpp` (or network) seam with a
portable fallback. ZNS's sequential-write-only constraint is a non-issue for an LSM (which never updates
in place) but would be fatal for an in-place design, which is part of why the LSM choice pays off here.
The PMEM ecosystem is in flux after Optane's discontinuation, so PMEM work should target the CXL
direction rather than a specific discontinued product.

The portable, high-value baseline is io_uring on ordinary NVMe; everything else is a conditional bet
justified by a measurement showing the baseline is the limit.

Interactions: io_uring is chapter 04; the ZNS backend changes how chapter 05 reclaims space; RDMA serves
chapters 14 and 16; NUMA pairs with chapter 13; SIMD accelerates chapters 06 and 09.

## 8. Experiment plan

**Hypothesis.** io_uring on NVMe improves throughput and tail latency over synchronous I/O; a ZNS
backend reduces write amplification and tail-latency variance versus a conventional SSD for the same
workload; the other technologies help only where a measurement shows the baseline is the limit.

**Setup.** Engine-level driver (chapter 03) on the relevant hardware, comparing each backend against the
synchronous-POSIX baseline, in the uncached regime so device behavior is visible, with device-level I/O
measured by `blktrace`/`iostat` (chapter 03).

**Baseline.** Synchronous POSIX I/O on NVMe (today).

**Variants.** io_uring on NVMe; ZNS backend on a zoned drive; PMEM-backed WAL where hardware exists;
NUMA-aware versus NUMA-oblivious placement on a multi-socket machine.

**Metrics.** Throughput and tail latency; measured device write amplification (application bytes versus
device bytes); and tail-latency variance for the ZNS comparison.

**Success criteria.** io_uring improves throughput and tail latency on NVMe; ZNS reduces device write
amplification and tail-latency variance; NUMA-aware placement reduces multicore memory-stall time.

**Killer result.** If ordinary NVMe with io_uring already saturates the workload's needs and its tail
latency is acceptable, the specialized hardware backends are not worth their complexity and portability
cost.

## 9. Implementation checklist

Part V (hardware frontier).

1. Add the io_uring backend behind `fs.hpp` (shared with chapter 04), with registered buffers and an
   O_DIRECT-aligned arena variant.
2. Add a ZNS `append_only_file` backend mapping SSTables to zones, and a zone-aware reclamation in the
   segments manager (chapter 05).
3. Make shard arenas NUMA-local and pin shard threads (chapter 13).
4. Extend `src/core/simd.hpp` for filter probing, hashing, and decompression assistance.
5. Add RDMA transport (chapters 14, 16) and evaluate PMEM/CXL for the WAL or memtable where hardware
   exists. Record results here.
