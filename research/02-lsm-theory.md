# 02 — The theory of log-structured merge-trees

This chapter explains, from first principles, why an LSM-tree is built the way it is and what its
costs are. It assumes no prior database knowledge. It defines a cost model and then uses that model
to derive the tradeoffs that every later chapter argues within. Chapter 01 showed the code; this
chapter shows the theory that the code is an instance of.

## 1. The problem: writes are expensive in the wrong shape

A key-value store must persist data on a storage device: a spinning disk or a solid-state drive
(SSD). Both devices are far faster at **sequential** access (reading or writing a long run of
adjacent bytes) than at **random** access (touching scattered locations). On a spinning disk this is
because the head must physically move. On an SSD it is because the device reads and writes in large
internal pages and must erase whole blocks before rewriting them, so small scattered writes cause
hidden extra work inside the drive.

A naive store that keeps data sorted on disk and updates it in place must do a random write for
every update, because the place to write is wherever the key already lives. That is the slow shape.
The LSM-tree exists to avoid it.

## 2. The core idea

The LSM-tree turns random writes into sequential writes by never updating data in place. It does
three things.

First, it absorbs every write into a small sorted structure in memory, called the **memtable**. A
write to memory is cheap and involves no disk seek.

Second, to survive a crash, it also appends each write to a **write-ahead log** (WAL), an
append-only file. Appending is sequential, so it is fast, and the log lets the engine rebuild the
memtable after a crash by replaying it.

Third, when the memtable fills up, the engine writes its entire sorted contents out to disk in one
sequential pass as an immutable file called a **sorted string table** (SSTable). The memtable is
then cleared and the log is truncated.

Over time this produces many SSTables. A read may therefore have to look in several of them, and the
same key may appear in several of them with different versions. To keep the number of files and the
amount of stale data bounded, a background process called **compaction** periodically reads several
SSTables, merges them in sorted order, keeps only the newest version of each key, drops deleted
keys, and writes the result back out as fewer, larger SSTables.

Every component in chapter 01 maps onto this: the memtable is `src/storage/memtable.cpp`, the WAL is
`src/engine/wal.cpp`, the SSTable writer is `src/storage/sstable_writer.cpp`, and compaction is the
piece that does not exist yet (chapter 05).

## 3. The cost model: counting block transfers

To reason about cost precisely, this program uses the **external-memory model**, also called the
**I/O model**, introduced by Aggarwal and Vitter [AggarwalVitter88]. The model ignores the cost of
work done in memory and counts only transfers of blocks between a small fast memory and a large slow
disk. It has three parameters:

- `N` — the number of data items (keys) in the store.
- `B` — the number of items that fit in one block, the unit of disk transfer.
- `M` — the number of items that fit in memory at once.

The model is the right one for a storage engine because, when data does not fit in memory, the
number of block transfers dominates the running time. Two classic results from this model frame
everything:

- Searching a sorted, block-structured index (a B-tree) costs `O(log_B N)` transfers.
- Sorting `N` items costs `O((N/B) · log_{M/B}(N/B))` transfers, the "sorting bound".

The LSM-tree is essentially a way to amortize that sorting bound across many small writes instead of
paying it per write.

## 4. The three amplifications

LSM-tree cost is usually expressed as three ratios. Each is defined plainly here and used for the
rest of the program.

**Write amplification** is the total bytes written to disk divided by the bytes the user actually
stored. It is greater than one because compaction rewrites data. If a key is rewritten ten times on
its way down through the levels, the write amplification contributed by it is ten.

**Read amplification** is the number of blocks read from disk to answer one user read. It is greater
than one because a key may have to be looked for in several SSTables.

**Space amplification** is the bytes occupied on disk divided by the bytes of live, current data. It
is greater than one because of stale versions and tombstones that have not yet been compacted away.

These three cannot all be made small at once. That is the content of the next section.

## 5. Levels, the size ratio, and the leveling/tiering choice

SSTables are organized into **levels**, numbered from 0. Level 0 holds the freshly flushed tables.
Each deeper level is larger than the one above it by a fixed factor `T`, called the **size ratio**
or **fan-out**. Because each level is `T` times larger, the number of levels needed to hold `N`
items when memory holds `M` is about:

```
L ≈ log_T(N / M)
```

There are two main ways to organize the runs within a level, and the choice is the single most
important tradeoff in LSM design.

**Leveling** keeps exactly one sorted run per level. When a level would exceed its size, it is
merged into the level below. Because a level fills and merges down repeatedly, each item is rewritten
on the order of `T` times per level, so:

- Write amplification is on the order of `T · L`.
- A point read examines one run per level, so read amplification is on the order of `L` (and far
  less with filters; see section 7).
- Space amplification is low, near one, because each level has a single run with little duplication.

**Tiering** allows up to `T` runs per level. A level is merged only when it has accumulated `T`
runs, and the merged result moves to the next level. Because merging happens `T` times less often:

- Write amplification is on the order of `L`.
- A point read may have to examine up to `T` runs per level, so read amplification is on the order
  of `T · L`.
- Space amplification is higher, on the order of `T`, because many runs hold stale versions.

So leveling favors reads and space at the cost of writes; tiering favors writes at the cost of reads
and space. This is the lever chapter 05 designs around.

A middle option, **lazy leveling** [Dayan18Dostoevsky], uses tiering on all the small levels (where
most of the write cost is) and leveling only on the largest level (where most of the data and read
cost is), capturing much of the benefit of both.

The following table summarizes the asymptotic costs. `s` denotes the false-positive rate of a
per-level filter (section 7).

| Policy        | Write amp | Point read (absent key) | Point read (present) | Space amp |
|---------------|-----------|-------------------------|----------------------|-----------|
| Leveling      | O(T·L)    | O(L·s)                  | O(1) amortized       | ~1        |
| Tiering       | O(L)      | O(T·L·s)                | O(1) amortized       | O(T)      |
| Lazy leveling | O(L)      | O(L·s) on last level    | O(1) amortized       | ~1 to O(T)|

## 6. The RUM conjecture

The tradeoff above is one case of a general principle, the **RUM conjecture**, stated by
Athanassoulis and colleagues [Athanassoulis16RUM]. RUM stands for Read overhead, Update overhead,
and Memory overhead. The conjecture is that any access method must trade these three against one
another, and that pushing one down tends to push another up. Leveling versus tiering is exactly a
move along the Read-versus-Update edge of this triangle, with Memory (space) coming along for the
ride.

The value of the RUM framing is that it tells you there is no single best structure, only structures
that are best for a chosen point in the read/update/memory space. This is why chapter 03 insists on
measuring the actual workload, and why chapter 21 proposes treating the structure itself as a
parameter to be chosen per workload rather than fixed in advance.

## 7. Filters change the read story

A point read's read amplification of `O(L)` or `O(T·L)` would be painful, but it is largely
eliminated for absent keys by a **filter**: a small in-memory structure, one per SSTable, that can
say "this key is definitely not in this table" most of the time. The classic filter is the Bloom
filter (chapter 06). With a per-table filter of false-positive rate `s`, a point lookup for a key
that is not present only touches a table's data block with probability `s`, so the expected disk
reads for an absent key fall from `O(L)` to `O(L · s)`, and `s` can be made small.

This is why the cost table above multiplies the read terms by `s`. It is also why chapter 06 matters
so much: the filter is what makes LSM point reads competitive with in-place structures, and the
Monkey result [Dayan17Monkey] shows that how you size the filters across levels changes the constant
significantly.

## 8. Why "cached versus uncached" must be controlled when measuring

The I/O model counts disk transfers, but a real system has the operating system **page cache** in
front of the disk (chapter 01, section 6 referenced it; the glossary defines it). If the data being
read is already in the page cache, a "disk read" costs nothing and the I/O count is irrelevant; the
cost is then dominated by in-memory work such as key comparisons and index lookups.

This has a direct consequence for measurement, taken up in chapter 03: an experiment that wants to
measure disk-bound behavior must ensure the data is not already cached (for example by using a
dataset much larger than memory, or by dropping caches, or by using O_DIRECT to bypass the cache),
and an experiment that wants to measure in-memory work such as index-structure speed must ensure the
data is cached so the disk does not dominate. An experiment that does not state which regime it is in
is not interpretable. This is the reason `TASKS.md` section I and chapter 07 specify "measured with
the subsequent block read both cached and uncached".

## 9. The deeper memory-hierarchy point: cache-aware versus cache-oblivious

The same block-transfer argument that applies between memory and disk also applies between the CPU
caches and main memory, just with smaller blocks. A layout chosen to be efficient for one specific
block size is **cache-aware**; the Eytzinger layout in chapter 07 is an example. A layout that is
efficient across all block sizes at once, without being told any of them, is **cache-oblivious**, a
concept due to Frigo, Leiserson, Prokop, and Ramachandran [Frigo99]. This distinction reappears in
chapter 07 (index layouts) and chapter 08 (memtable structures), where the in-memory access pattern,
not the disk, is the cost that matters.

## 10. Notable researchers and key papers

- Patrick O'Neil, Edward Cheng, Dieter Gawlick, Elizabeth O'Neil — the original LSM-tree
  [ONeil96].
- Alok Aggarwal, Jeffrey Scott Vitter — the external-memory (I/O) cost model [AggarwalVitter88].
- Matteo Frigo, Charles Leiserson, Harald Prokop, Sridhar Ramachandran — cache-oblivious
  algorithms [Frigo99].
- Manos Athanassoulis, Stratos Idreos, and colleagues — the RUM conjecture [Athanassoulis16RUM].
- Niv Dayan, Manos Athanassoulis, Stratos Idreos — Monkey [Dayan17Monkey] and Dostoevsky
  [Dayan18Dostoevsky], which sharpen the filter-sizing and leveling/tiering analysis used above.

## 11. How this maps to frankie

frankie today implements the write side of this theory: a memtable (`src/storage/memtable.cpp`), a
write-ahead log (`src/engine/wal.cpp`), and an SSTable writer (`src/storage/sstable_writer.cpp`). It
does not yet implement the parts that bound read and space amplification over time: there are no
levels, no compaction, and no filters, and the read path does not yet consult SSTables at all
(chapter 01, sections 4 and 5). In the terms of this chapter, frankie has a write path with
unbounded read and space amplification, because nothing yet merges the flushed tables or skips them
on read.

The chapters that follow address exactly these gaps, in roughly this order of leverage: chapter 04
makes the write path fast and durable, chapter 05 adds compaction and levels to bound amplification,
and chapter 06 adds filters to bound point-read cost. Chapter 03, next, defines how to measure
whether any of it works.
