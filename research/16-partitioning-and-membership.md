# 16 — Partitioning and membership

Depends on: chapter 11 (scans, which range partitioning supports), chapter 13 (the same sharding idea,
within one machine), chapter 14 (each shard is a replication group), chapter 17 (gossip rides the async
network layer), chapter 19 (testing). Follows the nine-section depth template. Assumes no prior
distributed-systems knowledge.

## 1. Problem statement

A single node, or a single replication group, can hold only as much data and serve only as much load as
one machine allows. To grow beyond that, the keyspace must be split across many machines, and the
cluster must know which machines are alive and which one holds each piece of the data. frankie has
neither. The closest existing idea is the thread-per-core sharding of chapter 13, which splits work
across cores within one machine; this chapter splits data across machines, using the same partitioning
idea one level up.

## 2. Background and theory

### 2.1 How to split the keyspace

There are two basic schemes, with a refinement.

**Hash partitioning** assigns a key to a shard by hashing the key. It spreads load evenly and avoids
hotspots, but it destroys order, so a range scan (chapter 11) must touch every shard, and changing the
number of shards remaps almost every key.

**Range partitioning** assigns contiguous key ranges to shards. It keeps order, so a range scan touches
only the shards covering the range, but sequential keys (timestamps, monotonic ids) concentrate on one
shard and create a hotspot.

**Consistent hashing** [Karger97] is the refinement that makes hash partitioning practical to resize.
Keys and nodes are mapped onto a ring, and a key belongs to the next node clockwise. Adding or removing
a node moves only about one Nth of the keys, instead of remapping everything. Assigning each node many
**virtual nodes** on the ring evens out the distribution. **Rendezvous (highest-random-weight)
hashing** [ThalerRavishankar98] achieves the same minimal-movement property by having each key choose
the node that scores highest under a hash of the key and the node.

The approach most production systems use decouples the two mappings: fix a large number of shards, map
keys to shards by hash or range, and then map shards to nodes through a placement layer. Rebalancing
becomes "move whole shards between nodes", which is far simpler than remapping keys. CockroachDB and
TiKV use range partitioning with dynamic splitting of ranges as they grow or get hot.

### 2.2 Combining partitioning with replication

Each shard is its own replication group, typically its own Raft group (chapter 14). A cluster of many
shards is therefore many Raft groups, an arrangement usually called **multi-Raft**. A node hosts
replicas of many shards, and each shard's leader may be on a different node, spreading the leader load.

### 2.3 Membership and failure detection

The cluster must agree on who is in it and notice when a node fails. Doing this with every node pinging
every other node scales badly. **SWIM** [Das02SWIM] is the standard scalable protocol: each node
periodically pings a random peer, and if it does not respond, asks a few other nodes to ping it on its
behalf before declaring it suspect, and disseminates membership changes by piggybacking them on these
messages (a form of **gossip**, where information spreads epidemically through random pairwise
exchanges). A refinement is the **phi-accrual failure detector** [Hayashibara04], which outputs a
continuous suspicion level rather than a binary up-or-down verdict, so the rest of the system can choose
how much suspicion warrants action, adapting to network conditions instead of a fixed timeout.

## 3. Design space

| Decision | Options | Recommended |
|----------|---------|-------------|
| Partition scheme | hash, range, consistent-hash | range, with dynamic split/merge |
| Shard-to-node mapping | direct hashing, placement coordinator | placement layer over many shards |
| Replication per shard | single group, multi-Raft | multi-Raft (one Raft group per shard) |
| Membership/failure | all-to-all heartbeats, SWIM gossip | SWIM gossip |
| Failure decision | fixed timeout, phi-accrual | phi-accrual |

Range partitioning with dynamic splitting is recommended because it preserves the range scans of
chapter 11 and adapts to hotspots by splitting hot ranges; hash partitioning is the simpler fallback if
cross-shard scans are never needed.

## 4. Notable researchers and key papers

- David Karger and colleagues — consistent hashing [Karger97].
- David Thaler, Chinya Ravishankar — rendezvous (highest-random-weight) hashing [ThalerRavishankar98].
- Abhinandan Das, Indranil Gupta, Ashish Motivala — SWIM membership and failure detection [Das02SWIM].
- Naohiro Hayashibara and colleagues — the phi-accrual failure detector [Hayashibara04].
- Practical multi-Raft, range-partitioned systems: CockroachDB and TiKV.

## 5. Concrete design for this codebase

**Shards as range partitions.** Split the keyspace into ranges, each range a shard, each shard a Raft
group (chapter 14). Ranges split when they grow past a size bound or get hot, and merge when they shrink
(the CockroachDB model). A shard split is, concretely, splitting an LSM-tree at a key: the SSTables are
divided at the split key (or marked so each half reads only its part), and two Raft groups take over the
two halves.

**A routing layer.** A request is routed to the shard owning its key by a range lookup in a routing
table that maps key ranges to shards and shards to their current Raft leaders. The routing table is
cached on every node and refreshed on misses. Cross-shard scans (chapter 11) fan out to the shards
covering the range and merge their results with the same merging iterator used inside a node.

**Placement.** A placement layer maps shards to nodes and rebalances by moving shard replicas between
nodes, aiming for even data and leader distribution. It can be a small Raft-replicated coordinator or a
consistent-hashing assignment.

**Membership.** Run SWIM gossip for membership and a phi-accrual detector for failure suspicion; a
suspected node's shards trigger Raft re-elections (for shards it led) and re-replication (to restore the
replication factor).

This reuses the within-machine sharding of chapter 13: the same routing-and-merge structure that sends a
request to the right core sends it to the right node, one level up.

## 6. C++, memory, and concurrency mechanics

The routing table is a range map (an ordered structure keyed by range start) consulted on every
request; it is read-mostly and updated on splits, merges, and leader changes, so it fits a read-
optimized concurrent structure or a copy-on-write snapshot read under chapter 13's discipline. Gossip
and SWIM messages ride the same asynchronous network layer as Raft (chapters 14 and 17). A shard split
must be crash-consistent (chapter 12): the manifest records the split atomically so a crash during a
split recovers to either the pre-split or post-split shape, never a mixture. The membership and routing
state is shared cluster-wide and is the distributed analog of chapter 13's shared state.

## 7. Risks, alternatives, and interactions

The main risk of range partitioning is hot ranges from sequential keys; mitigations are load-based
splitting and, for keyspaces known to be sequential, hashing a prefix of the key to spread inserts.
Resharding (split, merge, move) must be correct under concurrent reads, writes, and failures, which is a
significant testing burden (chapter 19). Routing-table staleness causes misdirected requests, handled by
detecting the miss and refreshing.

The fallback is fixed hash partitioning with consistent hashing: simple, even, and adequate when
cross-shard range scans are not required.

Interactions: each shard is a chapter 14 Raft group; cross-shard scans and transactions use chapters 11
and 15; splits rely on chapter 12's atomic manifest; gossip uses chapter 17's network; and the whole
thing is the cluster-level form of chapter 13's sharding.

## 8. Experiment plan

**Hypothesis.** Range-partitioned multi-Raft scales storage and throughput roughly linearly with nodes
under a balanced workload, rebalances without downtime, and detects and recovers from node failures
within a bounded time, while hot ranges are handled by load-based splitting.

**Setup.** A multi-node cluster (chapters 14, 13) on the engine-level driver (chapter 03), with the
fault injector (chapter 19) for node failures and partitions. Workloads: uniform keys, sequential keys
(to create a hotspot), and a scan-heavy mix.

**Baseline.** A single shard / single node.

**Variants.** Increasing node counts; range versus hash partitioning; with and without load-based
splitting on the sequential-key workload.

**Metrics.** Throughput and storage capacity versus node count; rebalancing duration and its impact on
foreground latency; failure-detection time and false-positive rate (phi-accrual tuning); and hot-range
behavior with and without splitting.

**Success criteria.** Near-linear scaling under balanced load; rebalancing without unavailability;
bounded, low-false-positive failure detection; hotspots relieved by splitting.

**Killer result.** If the dataset and load fit comfortably on one machine (with replication for fault
tolerance, chapter 14), partitioning adds complexity for no benefit and is not built.

## 9. Implementation checklist

Part IV (the distributed system).

1. Implement range partitioning with a per-shard Raft group (multi-Raft) and a routing layer with a
   cached range map.
2. Implement shard split and merge as crash-consistent LSM operations recorded atomically in the
   manifest (chapter 12).
3. Implement a placement layer that rebalances shards across nodes.
4. Implement SWIM gossip membership and a phi-accrual failure detector driving re-election and
   re-replication.
5. Run the scaling, rebalancing, and failure experiments (chapter 19); record results here.
