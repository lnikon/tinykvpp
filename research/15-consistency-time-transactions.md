# 15 — Consistency, time, and transactions

Depends on: chapter 11 (MVCC and snapshots, the basis for transactions), chapter 14 (replication, which
this chapter defines the guarantees of), chapter 16 (partitioning, for cross-shard transactions),
chapter 19 (consistency checking). Follows the nine-section depth template. It assumes no prior
distributed-systems knowledge.

## 1. Problem statement

Once data is replicated (chapter 14), the engine must define what a client is guaranteed to see, how
operations on different nodes are ordered in time, and whether several keys can be changed together
atomically. frankie has none of this defined. It has a local per-node sequence counter
(`engine::sequence_`) and a local wall-clock timestamp in each internal key
(`src/storage/memtable.cpp` calls `core::wall_clock_ms()`), both of which are meaningless across nodes,
and it has no notion of a transaction spanning more than one key. This chapter defines the consistency
guarantees, the clocks that make cross-node ordering possible, and the transaction protocols.

## 2. Background and theory

### 2.1 Consistency models

A consistency model is a contract about what reads can observe given the writes that happened. From
strongest to weakest:

- **Linearizability** [HerlihyWing90]. Every operation appears to take effect instantaneously at some
  point between its call and its return, and all clients agree on one order consistent with real time.
  This is what a single, non-replicated register would do; it is the strongest single-object guarantee
  and the easiest for application programmers to reason about.
- **Sequential consistency** [Lamport79]. All clients see operations in one order consistent with each
  client's own program order, but not necessarily consistent with real time.
- **Causal consistency**. Operations that are causally related (one could have influenced the other)
  are seen in the same order by everyone; unrelated operations may be seen in different orders. It is
  the strongest model achievable while staying available during a network partition.
- **Eventual consistency**. Replicas converge to the same value if writes stop, with no ordering
  guarantee in the meantime.

### 2.2 The CAP and PACELC constraints

The **CAP theorem** (conjectured by Brewer, proved by Gilbert and Lynch [GilbertLynch02]) states that
when a network partition splits the replicas, the system must choose between **consistency** (refuse to
serve possibly-stale data) and **availability** (serve anyway). A consensus system like Raft (chapter
14) chooses consistency: the minority side of a partition stops serving strong reads and writes.
**PACELC** [Abadi12] extends this with the common case: Else, when there is no partition, the system
still trades **latency** against **consistency**, because stronger consistency needs more coordination
and so more latency. These are not avoidable; they are the shape of the design space, the distributed
analog of the RUM conjecture (chapter 02).

For systems that must stay available during partitions, **conflict-free replicated data types**
(CRDTs) [Shapiro11] let replicas accept writes independently and merge automatically, always
converging, which provides causal-plus consistency without coordination.

### 2.3 Clocks and ordering

To order operations across nodes you need a notion of time, and physical clocks on different machines
drift apart, so they cannot be trusted directly. The classic tools:

- **Lamport logical clocks** [Lamport78]. A counter that increments on each event and is advanced to
  exceed any timestamp seen in a message. It respects causality (if A causes B then A's timestamp is
  less than B's) but says nothing about real time.
- **Vector clocks** [Fidge88]. A vector of counters, one per node, that can detect whether two events
  are causally ordered or concurrent, at the cost of size proportional to the number of nodes.
- **Hybrid logical clocks (HLC)** [Kulkarni14HLC]. A clock that combines a physical timestamp with a
  logical counter so that its values both track real time closely and always respect causality. It is
  the practical choice for ordering across nodes because it is bounded in size and meaningful as
  approximate wall time.
- **TrueTime** [Corbett12Spanner]. Google's Spanner uses real clocks with bounded, explicitly known
  uncertainty (from GPS and atomic clocks) and waits out that uncertainty to provide external
  consistency (linearizability across the whole database). It needs special hardware to keep the
  uncertainty small.

### 2.4 Transactions

A transaction groups several operations so they take effect atomically (all or nothing) and in
isolation from other transactions. Within one node, multi-version concurrency control (chapter 11)
already provides snapshot isolation: a transaction reads from a snapshot and its writes become visible
together. Across nodes the problem is harder:

- **Two-phase commit (2PC)** coordinates an atomic commit across several shards: a coordinator asks
  every participant to prepare, and commits only if all vote yes. Its weakness is blocking: if the
  coordinator fails after prepare, participants are stuck holding locks until it recovers.
- **Percolator** [Peng10Percolator] builds snapshot-isolation transactions on top of a key-value store
  using a global timestamp oracle and a 2PC scheme whose locks and intents are stored as ordinary
  key-value records. It is how distributed transactions were added to BigTable.
- **Calvin** [Thomson12Calvin] takes a different route: agree on a global order of transactions first
  (by consensus), then execute them deterministically, which removes the need for 2PC at commit time.
- **Spanner** [Corbett12Spanner] layers 2PC over per-shard Paxos groups and uses TrueTime to make the
  result externally consistent.

## 3. Design space

| Dimension | Options | Recommended starting point |
|-----------|---------|----------------------------|
| Read consistency | Linearizable (via leader), bounded-staleness/follower, eventual | Tunable; default linearizable |
| Cross-node clock | Lamport, vector, HLC, TrueTime | HLC |
| Single-key transactions | (free from Raft) | Linearizable single-key |
| Intra-shard multi-key | MVCC snapshot isolation (chapter 11) | Snapshot-isolation transactions |
| Cross-shard transactions | 2PC, Percolator, Calvin | Percolator-style over per-shard Raft |

## 4. Notable researchers and key papers

- Maurice Herlihy, Jeannette Wing — linearizability [HerlihyWing90].
- Leslie Lamport — logical clocks and sequential consistency [Lamport78], [Lamport79].
- Eric Brewer; Seth Gilbert, Nancy Lynch — the CAP theorem and its proof [GilbertLynch02].
- Daniel Abadi — PACELC [Abadi12].
- Marc Shapiro and colleagues — conflict-free replicated data types [Shapiro11].
- Colin Fidge, Friedemann Mattern — vector clocks [Fidge88].
- Sandeep Kulkarni and colleagues — hybrid logical clocks [Kulkarni14HLC].
- James Corbett and colleagues — Spanner and TrueTime [Corbett12Spanner].
- Alexander Thomson and colleagues — Calvin, deterministic transactions [Thomson12Calvin].
- Daniel Peng, Frank Dabek — Percolator [Peng10Percolator].

## 5. Concrete design for this codebase

**Tunable read consistency.** Expose a read-consistency level per request: linearizable reads go
through the leader (or use a Raft read-index to confirm leadership without a log write), while
bounded-staleness or follower reads are served locally by a replica and labeled with how stale they
may be. This is a direct PACELC knob (chapter 2's RUM applied to consistency).

**Adopt HLC for cross-node ordering.** Replace the local wall-clock timestamp in the internal key with
a hybrid logical clock value assigned at the leader, which also satisfies the determinism requirement
from chapter 14 (the value-affecting timestamp must come from the log, not each replica's local clock).
The existing per-node `sequence_` is a Lamport-style counter; the HLC generalizes it across nodes while
remaining close to wall time.

**Transactions, in three layers.** First, single-key linearizable operations come for free from Raft
(chapter 14). Second, intra-shard multi-key transactions reuse the MVCC and snapshot machinery of
chapter 11: a transaction reads from a snapshot HLC and commits its writes atomically through the
shard's Raft log. Third, cross-shard transactions use a Percolator-style scheme over the per-shard Raft
groups (chapter 16): a timestamp oracle (the HLC) assigns start and commit timestamps, and locks and
intents are stored as special key-value records, with 2PC across the shards. Calvin is the alternative
to evaluate if 2PC's blocking proves troublesome.

## 6. C++, memory, and concurrency mechanics

An HLC value packs a physical component and a logical counter, for example a 64-bit physical
millisecond time and a 16-bit logical counter, into the timestamp field that already exists in the
internal key (`src/storage/memtable.hpp:internal_key::timestamp`), which is currently filled from
`core::wall_clock_ms()`. The HLC update rules (advance the logical counter when the physical clock has
not moved, and on receiving a message take the maximum) are a few lines but must be applied on every
send and receive.

Percolator's locks and intents are stored as ordinary records in the store, so they reuse the existing
value path; a transaction's reads at a snapshot HLC reuse chapter 11's snapshot reads. The timestamp
oracle is a shared service (the leader's HLC), so its access is subject to chapter 13's concurrency
discipline. Snapshot reads pin versions (chapter 11), and the cross-shard 2PC coordinator should itself
be Raft-backed (as in Spanner) so it does not block on a single coordinator's failure.

## 7. Risks, alternatives, and interactions

The central risks are 2PC's blocking on coordinator failure (mitigated by a Raft-backed coordinator)
and clock-related anomalies (an HLC bounds causal ordering but not real-time skew, so externally
consistent reads in the Spanner sense need TrueTime-style bounded clocks or must accept slightly weaker
guarantees). Transactions add significant complexity and must be checked with a consistency checker
(chapter 19).

The fallback is conservative and still useful: linearizable single-key operations (free from chapter
14) plus intra-shard snapshot-isolation transactions (free from chapters 11 and 14), deferring
cross-shard transactions until they are actually needed.

Interactions: this chapter defines the guarantees of chapter 14, builds transactions on chapter 11,
spans the shards of chapter 16, and is verified by chapter 19.

## 8. Experiment plan

**Hypothesis.** The engine provides the advertised consistency level under failures, and snapshot-
isolation transactions are correct under contention, with cross-shard transactions adding a bounded
latency cost.

**Setup.** A multi-node, multi-shard cluster (chapters 14, 16) with the fault injector and a
consistency checker (chapter 19) that verifies linearizability for strong reads and snapshot isolation
for transactions, under partitions, crashes, and clock skew.

**Baseline.** Single-key linearizable operations.

**Variants.** Linearizable versus follower reads; intra-shard versus cross-shard transactions; injected
clock skew of varying magnitude.

**Metrics.** Consistency-checker verdicts (must pass for the claimed level); read and transaction
latency by consistency level; and the abort rate of transactions under contention.

**Success criteria.** The checker finds no violation of the advertised level under any injected fault,
including clock skew within the assumed bound; latencies match the PACELC expectation (stronger means
slower).

**Killer result.** If the application only needs single-key operations, the entire transaction and
clock apparatus is unnecessary and only chapter 14's single-key linearizability is built.

## 9. Implementation checklist

Part IV (the distributed system).

1. Add a per-request read-consistency level (linearizable via leader/read-index; follower with
   staleness label).
2. Replace the local wall-clock timestamp with a leader-assigned HLC carried in the log (also
   satisfies chapter 14's determinism rule).
3. Build intra-shard snapshot-isolation transactions on chapters 11 and 14.
4. Build cross-shard transactions Percolator-style over per-shard Raft groups with a Raft-backed
   coordinator; evaluate Calvin as an alternative.
5. Verify every level with the consistency checker (chapter 19); record results here.
