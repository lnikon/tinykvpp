# 21 — The design continuum

This is a synthesis chapter. It does not follow the nine-section template and proposes no single feature.
It offers a way of thinking that unifies several earlier chapters, and it sketches the long-horizon
direction that thinking points toward. Read chapters 02, 05, 07, and 08 first, because this chapter
generalizes them.

## 1. The observation

Several earlier chapters made the same move. Chapter 02 showed that leveling and tiering are not two
different data structures but two settings of one parameter, the number of sorted runs per level.
Chapter 05 then added lazy leveling as an intermediate setting. Chapter 07 made the block index a
swappable strategy chosen by a configuration knob. Chapter 08 did the same for the memtable. Chapter 06
made the filter's bits per key a per-level parameter. In every case, what looked like a choice between
distinct designs turned out to be a choice of coordinates in a continuous space of designs.

This is not a coincidence. It is the central insight of a line of research by Stratos Idreos and
colleagues: the structures that storage engines are built from are points in a continuous design space,
and the boundaries we draw between named structures (B-tree, LSM-tree, hash table) are conventions, not
walls. Their "Data Calculator" [Idreos18DataCalculator] formalizes the space and can compute the
performance of a structure from its coordinates, and their work on design continuums and self-designing
key-value stores [Idreos19Continuum] shows how to move continuously between LSM-like and B-tree-like
designs by varying a small set of parameters.

## 2. What the parameters are

For this engine, the design coordinates are exactly the configuration knobs the earlier chapters
introduced, plus a few more they implied:

- The compaction policy and its size ratio (chapter 05): how many runs per level, and how much larger
  each level is. This is the leveling-tiering axis.
- The filter bits per level (chapter 06): how much memory to spend ruling out absent keys, and where.
- The block index strategy (chapter 07): sorted, Eytzinger, interpolation, or learned.
- The memtable structure (chapter 08): skiplist, radix tree, or other.
- The block size and compression (chapter 09): how much to read per access, and how hard to compress.
- The inline-versus-separated value threshold (chapter 10): where large values go.

Each knob is a coordinate. A particular setting of all of them is one point, one concrete engine design.
The RUM conjecture of chapter 02 says these coordinates cannot all be pushed toward "cheap" at once; they
trade off. So the design space is a surface of tradeoffs, and choosing an engine is choosing a point on
that surface.

## 3. Why this reframes the whole program

Once you see the knobs as coordinates, the research program changes shape. The earlier chapters each
asked "which setting of this one knob is best?" and answered "it depends on the workload, so measure
(chapter 03)." The design-continuum view asks the next question: rather than fixing each knob by hand,
can the engine choose its own coordinates from the workload it actually sees?

This is the idea of a **self-designing** store. It has three levels of ambition, in increasing order:

1. **Per-segment choice.** Each SSTable already records the structures it was written with (chapter 07
   persists the index kind; chapter 06 the filter schedule; chapter 09 the codec). So different segments
   can already use different coordinates, chosen when they are written. This is achievable today and is
   the natural consequence of making each knob a recorded per-segment property.
2. **Workload-driven tuning.** Measure the live workload (read/write ratio, key skew, value sizes, scan
   frequency) and pick the coordinates that the cost model (chapter 02) predicts are best for it,
   re-tuning as the workload shifts. This is the direction of robust-tuning work such as Endure (referred
   to in the broader-horizons note of the original `TASKS.md`), which chooses coordinates that are good
   not just for the expected workload but across the uncertainty in it.
3. **Synthesis from the continuum.** Given a workload and a hardware profile, compute the coordinates
   directly from a model of the space, as the Data Calculator does, rather than searching by experiment.
   This is the long-horizon research goal and the most speculative.

## 4. What to do with this now

The practical takeaway is small and immediate, even though the vision is large. When implementing the
earlier chapters, make every design choice a recorded parameter rather than a hard-coded constant:
persist the compaction policy and size ratio, the filter schedule, the index kind, the memtable
structure, the block size and codec, and the value-separation threshold, each with the segment or in the
manifest. This costs almost nothing during implementation (the chapters already call for most of it) and
it is the precondition for all three levels of self-design above. An engine whose every structural choice
is a stored coordinate can later be tuned, per segment, per workload, or eventually automatically; an
engine whose choices are baked into the code cannot.

So the design-continuum chapter does not add a feature. It adds a discipline: treat the knobs as
coordinates, record them, and keep the door open to choosing them automatically later. That discipline is
the unifying thread behind chapters 05, 06, 07, 08, 09, and 10, and it is the bridge from this program of
hand-chosen optimizations to the longer-term goal of a store that designs itself for its workload.

## 5. Notable researchers and key papers

- Stratos Idreos and colleagues — the Data Calculator, which computes the performance of a data structure
  from its design coordinates [Idreos18DataCalculator].
- Stratos Idreos and colleagues — design continuums and the path toward self-designing key-value stores
  [Idreos19Continuum].
