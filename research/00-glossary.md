# 00 — Glossary

Plain definitions of every term used in this research program. Terms are listed alphabetically.
When a definition uses another defined term, that term is written in the same words as its own
entry so it can be found. Chapter numbers point to where the term is used in depth.

### Amplification (write, read, space)

Three numbers that measure the overhead of a storage engine. **Write amplification** is the number
of bytes actually written to disk divided by the number of bytes the user asked to store. It is
greater than one because data is rewritten during compaction. **Read amplification** is the number
of disk reads needed to answer one user read. **Space amplification** is the number of bytes on
disk divided by the number of live (current) bytes the user data actually needs; it is greater than
one because of stale versions and deleted-but-not-yet-removed entries. These three trade off
against each other; see the RUM conjecture and chapter 02.

### Anti-entropy

A background process in a replicated system that compares the data held by two replicas and copies
over whatever is missing, so that replicas converge to the same state. Often implemented with a
Merkle tree to find the differing ranges cheaply. See chapter 14.

### ANN (approximate nearest neighbor)

A search that, given a query vector, returns vectors that are close to it under some distance
(for example Euclidean or cosine), without guaranteeing the exact closest result. Used for
similarity search over embeddings. HNSW is the most common index for it. See chapter 18.

### Arena (bump allocator)

A memory allocator that owns one or more large blocks of memory and serves allocation requests by
returning the next free bytes and advancing a single offset (a "bump"). It cannot free individual
allocations; it frees everything at once when destroyed or reset. It is fast and has no
per-allocation bookkeeping. This project uses one in `src/core/arena.cpp`. See chapter 01.

### ART (adaptive radix tree)

An in-memory ordered index for string keys. It is a trie (a tree keyed by successive pieces of the
key) whose node types adapt to how many children they have, which keeps it compact and
cache-friendly. A candidate replacement for the skiplist memtable. See chapter 08.

### Bloom filter

A small probabilistic data structure that answers the question "might this key be in this set?"
It can say "definitely not" or "maybe". It never produces a false negative but can produce a false
positive. It is used to skip on-disk files that definitely do not contain a key. See chapter 06.

### Block (data block)

The unit in which an SSTable stores key-value entries and the unit in which it is read from disk.
This project targets a block size of 4096 bytes (`src/storage/sstable_writer.hpp:sstable_writer_config`).
Reading one key reads its whole block. See chapters 01 and 09.

### Block cache

An in-memory cache of recently read data blocks, so that a repeated read of a hot block does not go
to disk. This project does not have one yet; it relies on the operating system page cache. The
research on which replacement policy to use is in chapter 19.

### Block index

A small structure inside an SSTable that maps a key to the block that would contain it, so a read
does not scan every block. This project's index is **sparse**: one entry per block, holding that
block's smallest key. A **dense** index would hold one entry per key. See chapters 01 and 07.

### Cache-oblivious

An algorithm or layout that is efficient across all levels of the memory hierarchy without being
told the size of any cache. Contrast with cache-aware designs that are tuned to a specific block
size. See chapter 02.

### CAP theorem / PACELC

CAP states that when a network partition (a communication failure splitting the nodes into groups)
occurs, a distributed system must choose between staying consistent and staying available. PACELC
extends it: Else (when there is no partition) the system still trades Latency against Consistency.
See chapter 15.

### Causal consistency

A consistency model that guarantees operations which are causally related (one could have
influenced the other) are seen in the same order by everyone, while unrelated operations may be
seen in different orders. Weaker than linearizability, cheaper to provide. See chapter 15.

### Checksum (CRC32, CRC32C, xxHash)

A short number computed from a block of bytes, stored next to those bytes, and recomputed on read
to detect corruption. **CRC32** is a classic cyclic redundancy check; this project has a
software-table implementation in `src/core/crc32.hpp`. **CRC32C** uses a different polynomial that
modern CPUs compute with a single instruction. **xxHash** (and its variant xxh3) is a fast
non-cryptographic hash often used as a checksum. See chapter 12.

### Compaction

The background process that merges several SSTables into fewer, larger ones, dropping overwritten
and deleted entries along the way. It is what keeps read and space amplification bounded as more
data is written. Its policy (how files are chosen and merged) is the central design choice of an
LSM-tree. See chapter 05.

### Consensus

The problem of getting a group of nodes to agree on a single value (or a single ordered log of
values) even when some nodes fail or messages are delayed. Raft and Paxos are consensus algorithms.
See chapter 14.

### Consistent hashing

A way to assign keys to nodes so that adding or removing a node moves only a small fraction of the
keys, instead of remapping everything. Used for partitioning data across a cluster. See chapter 16.

### Coroutine

A function that can suspend itself and be resumed later, keeping its local state across the
suspension. C++20 provides coroutines as a language feature. They are a natural way to write
asynchronous I/O code that reads like straight-line code. See chapter 13.

### CRDT (conflict-free replicated data type)

A data type designed so that replicas can be updated independently and then merged automatically
without conflicts, always converging to the same value. Used for highly available systems that
accept writes during partitions. See chapter 15.

### Database cracking

An adaptive indexing technique where the index is built incrementally as a side effect of answering
queries, instead of all at once up front. Each query physically reorganizes (cracks) the data into
the pieces it touched. See chapter 18.

### Deterministic simulation testing

A testing method where the whole system is run on a single thread against simulated time, disk, and
network, with all sources of randomness driven by one seed. Because everything is deterministic, any
failure can be replayed exactly by reusing the seed. Pioneered for FoundationDB. See chapter 19.

### Dense index / sparse index

See **Block index**. A dense index has one entry per key; a sparse index has one entry per block.

### Epoch-based reclamation (EBR)

A technique for safely freeing memory in a lock-free data structure. Threads announce which "epoch"
(time period) they are working in; memory is only freed once no thread could still be looking at it.
An alternative to hazard pointers and RCU. See chapter 13.

### External-memory model (I/O model)

A cost model that counts the number of block transfers between a small fast memory and a large slow
disk, instead of counting individual operations. It is the right model for storage engines because
disk transfers dominate the cost. Introduced by Aggarwal and Vitter. See chapter 02.

### Eytzinger layout

A way to store a sorted array so that a binary search visits memory in cache-friendly order. The
elements are placed as if in a breadth-first walk of the search tree. Searching it is still
logarithmic but branch-free and faster in practice. See chapter 07.

### Fan-out / size ratio (T)

In a leveled LSM-tree, the factor by which each level is larger than the one above it. A larger size
ratio means fewer levels (better reads) but more rewriting per level (worse writes). See chapter 02.

### Fence pointer

A stored key that marks the boundary of a block or file, letting a read decide which block or file
to look in without reading the data itself. The block index is made of fence pointers. See
chapter 02.

### Filter (probabilistic)

A small structure that quickly rules out keys that are not present, accepting a bounded rate of
false positives in exchange for being much smaller than the data. Bloom, cuckoo, XOR, and ribbon
filters are examples. See chapter 06.

### fsync / fdatasync

System calls that force data already written to a file to actually reach stable storage.
**fsync** flushes the file's data and metadata; **fdatasync** flushes the data and only the metadata
needed to read it back, so it can be slightly faster. Without one of these, a write may sit in a
volatile cache and be lost on power failure. See chapter 04.

### Gossip

A communication pattern where each node periodically exchanges state with a few random peers, so
information spreads through the cluster like rumor. Used for membership and failure detection. See
chapter 16.

### Group commit

Batching several log writes so that one fsync makes all of them durable at once, instead of one
fsync per write. It trades a little latency for much higher throughput. See chapter 04.

### Hazard pointer

A technique for safe memory reclamation in lock-free structures. Before using a shared pointer, a
thread publishes it as "hazardous"; memory is only freed if no thread has published it. An
alternative to epoch-based reclamation. See chapter 13.

### Hinted handoff

When a replica that should receive a write is temporarily down, another node stores the write and a
hint to deliver it later, so the write is not lost. See chapter 14.

### HNSW (hierarchical navigable small world)

A graph-based index for approximate nearest neighbor search. It builds layers of a navigable graph
so that a search can hop quickly toward the query vector. See chapter 18.

### Hybrid logical clock (HLC)

A clock that combines physical wall-clock time with a logical counter, so that timestamps both
roughly match real time and always respect causality. See chapter 15.

### Interpolation search

A search over a sorted array that guesses the position of the target by assuming the keys are evenly
spread, then refines. On uniform data it is faster than binary search; on skewed data it can be slow.
See chapter 07.

### io_uring

A modern Linux interface for asynchronous I/O. The program places I/O requests on a submission ring
and collects results from a completion ring, avoiding a system call per operation. See chapters 04
and 17.

### Key-value separation

Storing large values in a separate log and keeping only the keys (and pointers to the values) in
the LSM-tree, so that compaction rewrites keys but not values. The WiscKey design. See chapter 10.

### Leveling / tiering / lazy leveling

Three compaction policies. **Leveling** keeps one sorted run per level and merges eagerly; it
favors reads. **Tiering** keeps several runs per level and merges lazily; it favors writes. **Lazy
leveling** tiers the small levels and levels the largest one, aiming for a balance. See chapter 05.

### Linearizability

The strongest single-object consistency model. Every operation appears to take effect at a single
instant between its call and its return, and all clients see one consistent order. See chapter 15.

### LSM-tree (log-structured merge-tree)

The family of data structure this project belongs to. Writes go to an in-memory table and a log;
when the table fills it is written to disk as a sorted file; background compaction merges those
files. It turns random writes into sequential writes. Introduced by O'Neil and colleagues. See
chapter 02.

### Manifest

A small, crash-safe record of which SSTables currently make up the database and at which levels. It
lets the engine install the result of a compaction atomically and recover the correct set of files
after a crash. See chapters 05 and 12.

### Memtable

The in-memory table that absorbs writes before they are flushed to disk as an SSTable. In this
project it is a skiplist backed by an arena (`src/storage/memtable.cpp`). See chapters 01 and 08.

### Merging iterator

An iterator that reads from several sorted sources at once (the memtable and several SSTables) and
yields their entries in one merged sorted order, picking the newest version of each key. Needed for
range scans. See chapter 11.

### Merkle tree

A tree of hashes where each leaf hashes a data range and each parent hashes its children. Comparing
two Merkle trees finds differing ranges quickly. Used in anti-entropy. See chapter 14.

### Memory model / memory_order

The rules that say what one thread is guaranteed to observe of another thread's memory writes. In
C++, atomic operations take a `std::memory_order` argument that selects how strongly ordered they
are, trading guarantees against speed. Getting this right is required for correct lock-free code.
See chapter 13.

### Minimal perfect hash function (MPHF)

A hash function built for a fixed, known set of keys that maps each key to a distinct slot with no
gaps and no collisions. Possible only because the keys are known in advance. Useful for immutable
files. See chapters 06 and 07.

### MVCC (multi-version concurrency control)

Keeping multiple versions of each key, tagged with a version number, so that a reader can see a
consistent snapshot of the data at a point in time without blocking writers. See chapter 11.

### O_DIRECT

A flag that opens a file so that reads and writes bypass the operating system page cache and go
straight to the device. It gives the application control over caching at the cost of doing the
caching itself. The flag is defined but unused in `src/core/fs.hpp`. See chapters 04 and 17.

### Page cache

The operating system's in-memory cache of file contents. Ordinary reads and writes go through it.
A database can rely on it or bypass it with O_DIRECT and manage its own block cache. See chapter 19.

### Predecessor query

Given a target key, find the largest stored key that is less than or equal to it. This is the query
a sparse block index must answer, and it is why an order-destroying hash table cannot serve as that
index. See chapter 07.

### Quorum

A subset of replicas large enough that any two such subsets overlap. Requiring reads and writes to
reach a quorum guarantees a read sees the latest write. See chapter 14.

### Raft / Paxos

Two consensus algorithms. **Paxos** (Lamport) is the original and is famously hard to follow.
**Raft** (Ongaro and Ousterhout) was designed to be understandable and is built around an elected
leader and a replicated log. See chapter 14.

### RCU (read-copy-update)

A synchronization technique where readers proceed without locks and writers make a new copy, with
old copies freed only after all readers that could see them have finished. Common in the Linux
kernel. See chapter 13.

### Read repair

When a quorum read notices that some replicas returned stale data, it writes the latest value back
to them as a side effect. See chapter 14.

### Restart point

In a prefix-compressed block, a position where the full key is stored instead of only its
difference from the previous key. Restart points let a search jump into the middle of a block. See
chapter 09.

### RUM conjecture

The claim that any access method must trade off Read overhead, Update overhead, and Memory (space)
overhead, and that improving one tends to worsen another. The framing tool for the whole design
space. By Athanassoulis and colleagues. See chapter 02.

### Sequence number

A number assigned to every write in increasing order, used to tell which version of a key is newer
and to define snapshots. In this project it is `engine::sequence_` and is stored in every internal
key. See chapters 01 and 11.

### Shared-nothing / thread-per-core

A design where each CPU core owns a slice of the data and its own structures, and cores communicate
by messages rather than shared memory and locks. The Seastar and ScyllaDB model. See chapter 13.

### Skiplist

An ordered, linked structure with multiple levels of forward pointers that allow logarithmic search
and insertion without rebalancing. This project's memtable is a skiplist
(`src/storage/skiplist.hpp`). See chapters 01 and 08.

### Snapshot / snapshot isolation

A consistent view of the database as of a chosen point in time (a chosen sequence number), so that
a long read or scan does not see writes that happened after it started. See chapter 11.

### SSTable (sorted string table)

An immutable on-disk file holding key-value entries sorted by key, plus an index and a footer. The
unit that the memtable is flushed to and that compaction merges. See chapter 01.

### Tombstone

A marker that records a deletion. Because data is never overwritten in place, a delete is stored as
a tombstone that shadows older values until compaction removes both. This project stores a
`tombstone` flag in every internal key. See chapters 01 and 05.

### Torn write

A write that was only partially persisted because a crash happened in the middle of it, leaving a
record that is neither the old nor the new value. Checksums and careful ordering detect and recover
from it. See chapter 12.

### Varint (variable-length integer)

An encoding that stores small integers in fewer bytes by using seven bits of each byte for data and
the eighth as a continuation flag. This project uses it for lengths
(`src/core/serialization/buffer_writer.hpp:write_varint`). See chapter 01.

### WAL (write-ahead log)

An append-only file that records every write before it is applied, so that after a crash the writes
not yet flushed to an SSTable can be replayed. In this project it is `src/engine/wal.cpp`. See
chapters 01 and 04.

### Write stall

A pause in accepting writes because the engine cannot flush or compact fast enough to keep up,
forcing clients to wait. Compaction scheduling aims to avoid it. See chapter 05.

### Zipfian distribution

A skewed access pattern where a few keys are very popular and most are rare, common in real
workloads. Benchmarks use it to test caching and hot-key behavior. See chapter 03.

### ZNS / zoned storage

A type of SSD that exposes its storage as zones that must be written sequentially and erased as a
whole. It removes the device's hidden garbage collection, which suits append-only designs like an
LSM-tree. See chapter 17.
