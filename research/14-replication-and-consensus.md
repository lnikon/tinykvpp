# 14 — Replication and consensus

Depends on: chapter 01 (the WAL, which becomes the replicated log), chapter 04 (durability), chapter
11 (snapshots, for log compaction), chapter 13 (a replicated node is concurrent), chapter 19 (how this
is tested). Follows the nine-section depth template. This is the first chapter of Part IV, the
distributed system, which `README.md` names as the project's end goal. It assumes no prior
distributed-systems knowledge.

## 1. Problem statement

frankie is a single node. Everything so far survives a process crash (via the WAL, chapter 01) but
nothing survives the loss of the machine: if the disk dies, the data is gone, and while the machine is
down the data is unavailable. A distributed key-value store keeps copies of the data on several
machines so that it survives machine loss and stays available when some machines are down. There is no
replication in the code today. This chapter is about how to add it correctly, which is harder than it
looks because the copies must agree even when machines and network links fail.

## 2. Background and theory

### 2.1 Why and how to replicate

Replication keeps the data on N machines (replicas). The hard part is keeping the replicas consistent
while writes are happening and while failures occur. There are two main families.

**Leader-based, consensus replication.** One replica is the leader. It orders all writes into a log and
tells the followers to append the same entries in the same order. A write is committed once a majority
of replicas have stored it. Because any two majorities overlap, a committed write is never lost as long
as a majority survives, and all replicas apply the same log in the same order, so they reach the same
state. This gives strong consistency (linearizability, chapter 15). Raft, Multi-Paxos, and Viewstamped
Replication are in this family.

**Leaderless, quorum replication.** There is no leader. A client writes to W replicas and reads from R
replicas. If R + W is greater than N, every read overlaps every write, so a read can find the latest
value. This is the Amazon Dynamo design [DeCandia07Dynamo]. It is highly available (any replica can
take a write) but gives only eventual consistency by default, and it needs background mechanisms to
repair divergence: **read repair** (a read that sees stale replicas writes the latest value back),
**hinted handoff** (a write meant for a down replica is parked elsewhere and delivered later), and
**anti-entropy** (replicas periodically compare and reconcile, using a Merkle tree to find differing
ranges cheaply).

### 2.2 The consensus problem

Consensus is getting the replicas to agree on one value, or one ordered log of values, despite
failures. A correct consensus algorithm must be **safe** (the replicas never disagree on a committed
entry) and **live** (they eventually decide). A fundamental result, the FLP impossibility [FLP85],
proves that no deterministic algorithm can guarantee both in a fully asynchronous network where even
one process can fail, because a slow process is indistinguishable from a failed one. Real systems
escape this by using timeouts (assuming the network is usually timely) for liveness while never
compromising safety; they may pause progress during a bad period but never produce a wrong answer.

The standard frame is the **replicated state machine**: if every replica starts in the same state and
applies the same sequence of deterministic commands, they end in the same state. So replication reduces
to agreeing on the order of commands, which is exactly a replicated log. This is the key connection for
this project: frankie already has a log of commands, the WAL (chapter 01), and an apply function,
`memtable::put`. Making it distributed means replacing the local WAL with a replicated log and applying
committed entries through the existing path.

**Raft** [Ongaro14Raft] is the leader-based algorithm designed to be understandable. It has three
pieces: leader election (a follower that hears nothing from a leader for a timeout starts an election
and becomes leader if a majority votes for it), log replication (the leader appends client commands and
replicates them, committing once a majority acknowledges), and safety rules that ensure a new leader
has all committed entries. **Multi-Paxos** [Lamport98] is the older, more general, and famously harder
to follow algorithm in the same family. **Viewstamped Replication** [OkiLiskov88] predates both and is
very close to Raft in structure. **EPaxos** [Moraru13EPaxos] is leaderless consensus that commits
non-conflicting commands in one round trip, removing the single-leader bottleneck at the cost of more
complexity.

### 2.3 Log compaction

A replicated log grows forever, so it must be truncated. The replica periodically takes a snapshot of
its state (which an LSM-tree already produces as SSTables, chapter 11) and discards log entries older
than the snapshot. A replica that has fallen too far behind is caught up by shipping it a snapshot
rather than replaying the whole log.

## 3. Design space

| Approach | Consistency | Leader bottleneck | Common-case round trips | Partition behavior | Complexity |
|----------|-------------|-------------------|-------------------------|--------------------|------------|
| Raft | Strong (linearizable) | Yes | 1 (to a majority) | Minority side stalls | Medium |
| Multi-Paxos | Strong | Yes | 1 | Minority stalls | High |
| Viewstamped Replication | Strong | Yes | 1 | Minority stalls | Medium |
| EPaxos | Strong | No | 1 if no conflict | Quorum-based | High |
| Dynamo (leaderless) | Eventual (tunable) | No | 1 to R/W | Stays available, may diverge | Medium |

The recommended starting point is **Raft**, because it is the most understandable, gives strong
consistency, and maps directly onto the existing WAL and apply path. Leaderless Dynamo-style
replication is the alternative when availability under partition matters more than strong consistency,
and is the subject of chapter 15's consistency discussion.

## 4. Notable researchers and key papers

- Leslie Lamport — Paxos [Lamport98].
- Diego Ongaro, John Ousterhout — Raft, designed for understandability [Ongaro14Raft].
- Brian Oki, Barbara Liskov — Viewstamped Replication [OkiLiskov88]; Liskov and Cowling's revisit.
- Iulian Moraru, David Andersen, Michael Kaminsky — Egalitarian Paxos (EPaxos) [Moraru13EPaxos].
- Giuseppe DeCandia and colleagues — Dynamo, leaderless quorum replication [DeCandia07Dynamo].
- Michael Fischer, Nancy Lynch, Michael Paterson — the FLP impossibility result [FLP85].

## 5. Concrete design for this codebase

The design is "replicate the WAL with Raft and apply through the existing path".

**The log.** The Raft log entry is the existing `wal_entry` (`src/engine/wal.cpp`), extended with the
Raft term number. The on-disk Raft log reuses the WAL file infrastructure (`append_only_file`, chapter
04), so the durability work of chapter 04 (group commit, fsync ordering) applies directly to the
replicated log.

**The state machine.** Applying a committed entry is the existing `memtable::put`. The engine's apply
path becomes "apply committed Raft entries in order", and a write is acknowledged to the client only
after it is committed (a majority has it) and applied.

**The roles.** The leader accepts writes, appends them, replicates them, and applies on commit.
Followers append what the leader sends and apply on commit; they can serve follower reads (chapter 15)
but not strongly consistent reads without going through the leader or using a read-index protocol.

**Snapshots and log compaction.** Reuse the LSM snapshot (chapter 11) and SSTables as the Raft
snapshot, and truncate the Raft log past the snapshot point. A lagging follower is caught up by shipping
SSTables.

**Membership.** Cluster membership changes (adding or removing a replica) use Raft's joint-consensus
mechanism so the cluster never has two disjoint majorities during a change.

Start with a one-node Raft group (degenerate, but exercises the code path), then a three-node group.

## 6. C++, memory, and concurrency mechanics

Replication needs networking, which is the same asynchronous I/O surface as chapter 04 and chapter 17:
io_uring handles sockets as well as files, so the network layer and the storage layer share the loop.
Log entries are serialized with the existing `endian_integer`/`buffer_writer` helpers (chapter 01).

A replicated node is concurrent by nature, so chapter 13's discipline applies: the Raft log, the commit
index, and the role state are shared between the network-handling and apply activities, and their
accesses must be ordered (the simplest correct form keeps Raft on the single-threaded loop, so the
sharing is between coroutines on one thread, not across cores).

There is one correctness requirement specific to this project that is easy to miss. The replicated
state machine must be **deterministic**: applying the same log on two replicas must produce the same
logical key-value state. Two things in the current code are non-deterministic and must be handled. The
internal-key timestamp is taken from the local wall clock
(`src/storage/memtable.cpp:memtable::put` calls `core::wall_clock_ms()`), which differs across
replicas; for replication, the timestamp that affects the stored value must be assigned by the leader
and carried in the log entry, not generated locally on each replica. By contrast, the skiplist's
random node heights (`src/storage/skiplist.hpp:random_height`, seeded from `std::random_device`) are
fine to leave non-deterministic, because they affect only the in-memory structure, not the logical
value; two replicas may have differently shaped skiplists yet identical key-value contents. Drawing
this line correctly (logical state must be deterministic, physical representation need not be) is the
key design point.

## 7. Risks, alternatives, and interactions

Consensus is notoriously easy to get subtly wrong, with bugs that appear only under specific failure
interleavings; this is the strongest reason to pair this chapter with chapter 19 (a TLA+ specification
of the protocol and Jepsen testing of the implementation). The leader is a throughput bottleneck and a
failure point whose detection and re-election add latency; EPaxos or sharding (one Raft group per
shard, chapter 13) mitigate it. The determinism requirement above is a real trap.

The fallback is to start with leader-based Raft, single Raft group, and defer both leaderless
replication and multi-group sharding until the single-group version is correct and tested.

Interactions: the log is chapter 01's WAL with chapter 04's durability; snapshots are chapter 11;
concurrency follows chapter 13; consistency guarantees and reads are chapter 15; partitioning into
multiple Raft groups is chapter 16; and correctness rests on chapter 19.

## 8. Experiment plan

**Hypothesis.** A Raft-replicated frankie keeps committed writes consistent and durable across replica
failures and network partitions, at a write-latency cost of one round trip to a majority, and recovers
leadership within a bounded time after a leader failure.

**Setup.** A three-node and a five-node cluster on the engine-level driver (chapter 03), with a fault
injector (chapter 19) that kills replicas, partitions the network, and delays messages. Correctness is
checked with a linearizability checker (Jepsen-style, chapter 19).

**Baseline.** The single-node engine (no fault tolerance) for the latency and throughput reference.

**Variants.** Three versus five replicas; leader failure during writes; minority and majority
partitions.

**Metrics.** Linearizability (must hold for committed operations under all injected failures); write
latency and throughput versus replication factor; leader failover time; and behavior on the minority
side of a partition (it must stall, not serve stale strong reads).

**Success criteria.** No linearizability violation under any injected failure; failover within the
bound; the expected one-round-trip write latency.

**Killer result.** If the use case is satisfied by single-node durability (the data is not
business-critical, or is reconstructable), then replication's cost in latency and complexity is not
warranted.

## 9. Implementation checklist

This is Part IV (the distributed system), beyond the current single-node `TASKS.md`; it maps to
chapters 14 through 16 of this program.

1. Define the Raft log entry as the extended `wal_entry`; persist it on the WAL infrastructure.
2. Implement leader election, log replication, and the safety rules; apply committed entries through
   `memtable::put`.
3. Make the applied state deterministic: assign value-affecting timestamps at the leader and carry them
   in the log; leave skiplist heights local.
4. Reuse LSM snapshots and SSTables for Raft snapshotting and follower catch-up; implement joint-
   consensus membership changes.
5. Write a TLA+ specification and run Jepsen-style tests (chapter 19); record results here.
