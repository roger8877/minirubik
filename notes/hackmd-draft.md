# Assignment 1: minirubik on RV32I

> **Fork:** https://github.com/roger8877/minirubik, forked from sysprog21/minirubik at commit `231796c`
> **Ripes:** `v2.2.6-106-g5b8a616` (continuous prerelease, 2026-08-18), Windows x86_64 build
> **Host:** Intel Core i5-14400F, 15.8 GiB RAM, Windows 11
> **Conventions:** "code size" = bytes of linked `.text` with `RENDER=0`; "retired instructions" = Ripes `--iret` on `RV32_ISS` with the same input. "MiB" = 2^20 bytes.

<!--
寫作規則（貼上 HackMD 前刪掉這段）：
- TODO 的地方要用你自己的話寫（中文寫給我也可以，我幫你翻成英文），不要讓 AI 代寫分析。
- 每完成一節就貼上 HackMD 一次，留下多個 revision。
-->

## 1. The State Space

TODO (later): group order 3,674,160 = 7! x 3^6, the 9 HTM generators, Cayley graph,
the orientation-sum invariant (mod 3), and why BFS proves the diameter is 11.

## 2. Stage 1: Characterizing the Baseline

### 2.1 What `solver.c` computes

**Input and output.** The program takes a 14-digit string. The first 7 digits give the
permutation `p[7]`: which of the 7 movable corner cubies sits in each position. The last 7
digits give the orientation `o[7]`: how each of those cubies is twisted (0, 1 or 2). The output
is a shortest sequence of moves, in the half-turn metric, that returns the cube to solved.

**Why only 7 cubies.** A 2x2x2 cube has 8 corners, but the solver only turns the R, B and D
faces. None of these three faces touches the front-upper-left corner, so that corner never
moves and serves as a fixed reference. Turning the other faces would only be the same state
seen from a different viewpoint (a whole-cube rotation), so fixing one corner removes those
duplicates.

**Invariants of a valid state** (checked by `valid()`):
- the 7 positions hold 7 distinct cubies, so `p` is a permutation of 0..6;
- every orientation is 0, 1 or 2;
- the orientation sum is divisible by 3. This is a modulo-3 invariant, not a parity: a quarter
  turn adds twists that sum to 0 mod 3, so the 7th orientation is determined by the other 6.

This gives 7! = 5,040 permutations and 3^6 = 729 orientations, 3,674,160 states in total.

**How it solves.** `build_table()` runs a breadth-first search from the solved state over all
3,674,160 states and records, for every state, one move that brings it one step closer to
solved. A query then just follows these moves; since no state is more than 11 moves away,
it takes at most 11 lookups.

### 2.2 Where the cost lies

| Allocation | Storage | Bytes |
|---|---|---|
| One move toward solved per state | heap | 3,674,160 |
| BFS queue of 32-bit ranks | heap | 14,696,640 |
| Factored quarter-turn transitions | automatic | 34,614 |
| **Peak** | | **18,405,414** |

- One byte per state for the move toward solved: 3,674,160 x 1 = 3,674,160 bytes.
- The BFS queue holds every state once as a 32-bit rank: 3,674,160 x 4 = 14,696,640 bytes,
  79.8% of the peak. It exists only because BFS enumerates the whole space.
- Transition tables for the 3 quarter turns, stored as 16-bit entries:
  3 x 5,040 x 2 + 3 x 729 x 2 = 30,240 + 4,374 = 34,614 bytes.

The time cost: BFS expands 3,674,160 states with 9 moves each, 33,067,440 edges, and every
edge updates both the permutation and the orientation table, 66,134,880 updates. At about
15 instructions per update, a direct translation needs on the order of 10^9 instructions.

### 2.3 Measurement: host bytes per guest byte

**Method.** Each test program stores 32-bit words with `sw` over N bytes starting at
guest address `0x20000000` (outside `.text`/`.data`, so the loader touches none of it).
The loop computes the address as `base + (offset & mask)`:

- experiment (`mem_*.s`): `mask = -1`, so every store hits a new address and N distinct guest bytes are written;
- control (`ctrl_*.s`): `mask = 4095`, so the stores keep rewriting the same 4 KiB.

Both versions execute the same instructions (the control has one extra instruction,
because `li t4, 4095` expands to `lui` + `addi`), so any difference in host memory
comes from the number of distinct guest bytes written, not from run length.
Host memory is `Process.PeakWorkingSet64`, the peak tracked by Windows, read by
[`measure.ps1`](https://github.com/roger8877/minirubik/blob/main/ripes-test/measure.ps1). Each program ran 5-10 times; raw data in
[`results_rv32iss.csv`](https://github.com/roger8877/minirubik/blob/main/ripes-test/results_rv32iss.csv).

**Static check.** Retired instructions match the count predicted from the source:
`4 + 5 x (N/4) + 2`, e.g. 5,242,886 for N = 4 MiB.

**Results** (peak working set, MiB; spread across runs ≤ 0.7 MiB):

| N (guest bytes) | experiment | control | experiment − control |
|---|---|---|---|
| 1 MiB | 107.1 | 19.1 | 88.0 |
| 4 MiB | 347.9 | 27.2 | 320.7 |
| 16 MiB | 1311.1 | 27.1 | 1284.0 |

Slope between experiment runs:
(347.9 − 107.1) / 3 = **80.27** and (1311.1 − 347.9) / 12 = **80.27** host bytes per guest byte.
The intercept, 347.9 − 4 x 80.27 ≈ 26.8 MiB, matches the control's footprint.

The 1 MiB row gives 88 rather than 80 because the control itself is not constant
(19.1 MiB at 1 MiB, 27.1 MiB at 4 and 16 MiB). Taking the slope between experiment runs
cancels Ripes' own footprint without depending on the control, so 80.27 is the figure used.

**Why so many bytes.** The RISC-V program sees ordinary memory, but that memory only exists
inside Ripes. VSRTL keeps it in an `unordered_map` with one entry per guest byte, so every
byte written creates a separate hash-map node holding the address, the value and the links
that chain nodes together, plus the bucket array and allocator overhead. That bookkeeping
costs about 80 host bytes to store 1 byte of guest data.

Sampled private bytes occasionally jumped by 64 MiB (at 4 MiB) and 256 MiB (at 16 MiB).
This fits a hash-map rehash: when the map grows it allocates a larger bucket array and keeps
the old one until the move finishes, so both exist briefly. The 50 ms sampling catches that
moment only in some runs; the OS-tracked peak working set did not show it.

**Projection.** 80.27 x 18,405,414 ≈ 1.48 x 10^9 bytes ≈ **1.38 GiB** of host memory
for the baseline's peak, plus about 27 MiB for Ripes itself. This machine has 15.8 GiB,
so on this machine memory alone would not stop the baseline.

### 2.4 Measurement: retired instructions per second

Rates from the same runs (iret / median `--exectime`):

| Program | iret | median exectime (ms) | instructions/s |
|---|---|---|---|
| `ctrl_1M` | 1,310,727 | 66 | 19.9 M |
| `ctrl_4M` | 5,242,887 | 246 | 21.3 M |
| `ctrl_16M` | 20,971,527 | 949 | 22.1 M |
| `mem_1M` | 1,310,726 | 291 | 4.5 M |
| `mem_4M` | 5,242,886 | 1,450 | 3.6 M |
| `mem_16M` | 20,971,526 | 6,598 | 3.2 M |

**RV32_5S** (3 runs each, raw data in [`results_log.csv`](https://github.com/roger8877/minirubik/blob/main/ripes-test/results_log.csv)):

| Program | iret | median exectime (ms) | instructions/s | peak WS (MiB) |
|---|---|---|---|---|
| `ctrl_1M` | 1,310,727 | 4,745 | 276 k | 107.6 |
| `mem_1M` | 1,310,726 | 4,845 | 271 k | 187.8 |
| `ctrl_4M` | 5,242,887 | 21,343 | 246 k | 348.8 |
| `mem_4M` | 5,242,886 | 20,283 | 258 k | 669.6 |
| `ctrl_16M` | 20,971,527 | 76,542 | 274 k | 1308.0 |
| `mem_16M` | 20,971,526 | 83,430 | 251 k | 2047.5 |

One `ctrl_4M` run took 27.3 s against 19.0 s and 21.3 s for the others; the median is used.

Observations from the RV32_5S runs:
- On RV32_5S even the control grows with run length (107.6 → 348.8 → 1308.0 MiB),
  about 64 host bytes per retired instruction, while on RV32_ISS it stays near 27 MiB.
- Subtracting the RV32_5S control still gives the RV32_ISS slope:
  (187.8 − 107.6) / 1 = 80.2 and (669.6 − 348.8) / 4 = 80.2 host bytes per guest byte.
- `mem_16M` stopped at about 2047 MiB in all three runs, below the ~2592 MiB the trend predicts,
  although iret shows the program ran to completion. This point is excluded from the slope.

**Simulation rate depends on what the program does.** On RV32_ISS the same instruction count
runs about 7x slower when every store hits a new address (3.2 M/s against 22.1 M/s at 16 MiB),
because each new guest byte means inserting a node into the hash map, and occasionally a rehash.
Rewriting the same 4 KiB only updates existing entries.

**RV32_5S is about 80x slower than RV32_ISS** (about 0.27 M/s against 22 M/s for the control).
Pipeline stalls and flushes explain only a small part: in a test loop, 312 instructions took
520 cycles, about 1.7x. The main reason is how the simulator works. RV32_ISS executes each
instruction directly and only updates the result. RV32_5S simulates the whole circuit every
cycle: every stage, register, multiplexer and hazard unit. On RV32_5S the cost of inserting
into the hash map is small next to that, so the control and the experiment run at the same speed.

**Why the RV32_5S control grows.** On RV32_5S memory grows with run length even when only 4 KiB
is written. A likely cause is that the pipeline models keep a history of past cycles so the GUI
can step backwards; RV32_ISS does not need it. I did not confirm this in the source.
The ~2047 MiB ceiling of `mem_16M` on RV32_5S is not explained by these measurements.

**Comparison with the handout.** My rates are higher than the assignment's 1.09 M/s (RV32_ISS)
and 20.5 k/s (RV32_5S), about 3x and 13x respectively. These depend on the host CPU, the Ripes
build and the kind of loop measured, so I report my own numbers and use them below.

### 2.5 Why `report.md` §7 does not survive the move to Ripes

**Time.** Building the table takes about 10^9 instructions:

| Model | rate | time for 10^9 instructions |
|---|---|---|
| RV32_ISS, my machine (store-heavy loop) | ~3.2 M/s | ~5 minutes |
| RV32_5S, my machine | ~0.26 M/s | ~1 hour |
| RV32_5S, handout's figure | 20.5 k/s | ~13.5 hours |

**Memory.** About 1.38 GiB of host memory on this machine (section 2.3).

On a fast host the baseline is not strictly impossible, but it fails the target's real limits:

1. **The table does not fit.** The static data budget is 128 KiB. The full table is 3.6 MB,
   and even at 4 bits per state it is 1,794 KiB, 14 times the budget. A complete precomputed
   distance table is also not allowed, because it would move the work off the simulated processor.
2. **Building it on the target is too slow.** About 10^9 instructions, while the pass
   condition allows at most 5 x 10^7 retired instructions per query (worst distance-11 state),
   20 times less. And the table would have to be rebuilt for every query.
3. **Verification moves to the host.** [`report.md`](https://github.com/roger8877/minirubik/blob/main/report.md) §7 keeps the full table because it can verify
   every answer. That makes sense on a normal computer, but on Ripes the table can neither be
   shipped nor built in time. Instead, the full table is used on the host to check the solver
   (gates H1-H4), and the target only runs the search.

So the program must find shortest solutions **without** a full table: a search that uses only
small tables. That is the starting point of Stage 2.

## 3. Stage 2: Representation and Algorithm

Stage 1 showed that the full table cannot be shipped or built on the target. The program
therefore has to find a shortest solution by searching from the given state, using only
small tables. Code: [`ida/`](https://github.com/roger8877/minirubik/tree/main/ida).

### 3.1 Search: iterative deepening A* (IDA*)

The search is a depth-first search with a move limit, called `bound`. It first tries to solve
the cube within `bound` moves; if that fails, it raises `bound` by one and searches again.

```c
unsigned bound = heuristic(p, o);
while (!dfs(p, o, 0, bound, NO_FACE, path))
    ++bound;
```

**Why the answer is optimal.** When the search succeeds with `bound = k`, every smaller bound
has already been searched completely and failed, so no solution shorter than k exists.

**Why it starts at h instead of 0.** The heuristic never overestimates (3.2), so no solution can
be shorter than h(start); the bounds below it would always fail.

**Why it terminates.** Every `dfs` call is limited to depth `bound`, so each iteration is finite.
`bound` grows by one per iteration, and no state is more than 11 moves from solved, so the
search succeeds at `bound` ≤ 11 at the latest.

**Memory.** Depth-first search only keeps the current path, at most 11 moves, so there is no
queue. `MAX_DEPTH = 11` is the HTM diameter, which bounds the path array (and later the stack).

Inside the search, each call does:

```c
unsigned h = heuristic(p, o);       /* two table lookups */
if (g + h > bound) return 0;        /* cannot finish within bound: prune */
if (h == 0) return 1;               /* solved */
```

`g` is the number of moves made so far, so `g + h` is a lower bound on the length of any
solution through this state. If that already exceeds `bound`, the whole subtree is skipped.

**Move pruning.** Two consecutive moves of the same face either cancel or combine into one move
(R then R2 equals R'), so they never appear in a shortest solution. After the first move only
6 of the 9 moves are tried.

**A small trace.** For the state reached by one R turn (`25314672313211`), h = 1, so `bound`
starts at 1. The root is node 1. Its children R and R2 lead to states with h = 1 at g = 1,
so g + h = 2 > 1 and both are pruned (nodes 2 and 3). The child R' reaches solved with h = 0
(node 4). The program prints `R'` and reports 4 nodes.

### 3.2 Heuristic and admissibility

The heuristic is

h(state) = max(`perm_dist[p]`, `orient_dist[o]`)

where `perm_dist` is the exact number of moves needed to put all 7 cubies in their correct
positions while ignoring orientation, and `orient_dist` is the exact number needed to fix all
orientations while ignoring positions. Both are computed by BFS on the host.

**Admissibility argument.** Report.md §4 shows that the permutation and the orientation change
independently: a move maps the permutation to a new permutation, whatever the orientation is,
and the same for the orientation. So any sequence of moves that solves the whole cube also
solves the permutation on its own. Its length is therefore at least the shortest way to solve
the permutation alone, which is `perm_dist[p]`. The same holds for `orient_dist[o]`. Since
neither value can exceed the true distance, their maximum cannot either. This is the
pattern-database argument of Culberson and Schaeffer, applied to two projections of the state.

Gate H1 checks this directly on all 3,674,160 states (section 6).

### 3.3 Representation: ranks and transition tables

A state is two numbers: the permutation rank p (0..5039) and the orientation rank o (0..728),
using the same ranking as `solver.c`. A move is two table lookups:

```c
new_p = perm_move[p][move];
new_o = orient_move[o][move];
```

I considered two options:

| | Option 1: arrays `p[7]`, `o[7]` | Option 2: ranks + transition tables |
|---|---|---|
| Cost of one move | loop over 7 cubies, up to 3 quarter turns, ~200 instructions | 2 lookups |
| Cost of h | rank the arrays first, which needs multiplication | 2 lookups |
| Static data | small | 109,611 bytes |

I chose option 2: it uses the memory budget to make every node cheap, it still fits under
128 KiB, and it removes all multiplication from the search (ranking happens once, on input).

**Memory budget**

| Table | Size | Bytes |
|---|---|---|
| `perm_move` | 5,040 x 9 x 2 bytes | 90,720 |
| `orient_move` | 729 x 9 x 2 bytes | 13,122 |
| `perm_dist` | 5,040 x 1 byte | 5,040 |
| `orient_dist` | 729 x 1 byte | 729 |
| **Total** | | **109,611 (107.0 KiB)** |

Move entries need 2 bytes because ranks go up to 5,039; a byte holds at most 255.

**How the tables are built** ([`ida/gen.c`](https://github.com/roger8877/minirubik/blob/main/ida/gen.c)).
For every rank, the generator unranks it to arrays, applies each of the 9 moves with the
`source`/`twist` tables from `solver.c`, and ranks the result. Example: from solved (rank 0),
R gives the permutation `[1,4,2,0,3,5,6]` (rank 1104) and orientation `[1,2,0,2,1,0]` (rank 426):

```text
perm_move[0]   = {1104, 3294, 2190, 9, 16, 18, 198, 566, 368}
orient_move[0] = { 426,    0,  426, 16, 0, 16,   0,   0,   0}
                    R     R2    R'  B  B2  B'    D   D2   D'
```

The orientation row shows two facts from section 1: D moves never change orientation, and R2
from solved restores it because the two twists cancel. The distance tables come from a BFS over
each abstraction starting at rank 0:

| Table | max | distribution (distance: count) |
|---|---|---|
| `perm_dist` | 7 | 0:1, 1:9, 2:54, 3:297, 4:1233, 5:2157, 6:1244, 7:45 |
| `orient_dist` | 6 | 0:1, 1:2, 2:12, 3:64, 4:274, 5:336, 6:40 |

So h is at most 7 while true distances reach 11; this gap is what the search has to pay for.

**Not packed.** Distances fit in 4 bits, so two entries could share a byte, saving about 2.9 KB.
But each packed lookup costs about 5 more instructions, and every node does two lookups, so
packing would add about 10 instructions to every node in exchange for space I do not need.
Since no table is packed, gate H4 does not apply.

### 3.4 Measured search cost

Running the search on all 3,674,160 states on the host (gate H3) gives the number of `dfs`
calls ("nodes") per query:

| distance | states | mean nodes | max nodes |
|---|---|---|---|
| 7 | 227,536 | 561.7 | 3,945 |
| 8 | 870,072 | 2,860.7 | 14,756 |
| 9 | 1,887,748 | 13,901.1 | 55,585 |
| 10 | 623,800 | 48,135.8 | 250,324 |
| 11 | 2,644 | 206,624.8 | 639,798 |

- The reference vector `21345671111111` needs 233,966 nodes.
- The hardest distance-11 state, `54721631111111`, needs **639,798** nodes, 2.7 times as many,
  so a single sample is not enough to judge the worst case.
- The budget is 5 x 10^7 instructions, so the assembly may spend at most about
  **78 instructions per node**. This is the target for stages 3 and 4.

The ten hardest distance-11 states are listed in
[`ida/h3_result.txt`](https://github.com/roger8877/minirubik/blob/main/ida/h3_result.txt);
they are the test inputs for the worst case on Ripes.

### 3.5 Alternative considered

A larger pattern database (for example orientation combined with the positions of a few cubies)
would prune more and reduce the node count. It does not fit next to the transition tables
(107 KiB + tables of tens of KiB > 128 KiB), so it would require giving up the cheap
transition tables and paying more instructions per node. With 639,798 nodes measured and
about 78 instructions per node available, the current design should fit; if the assembly
measurement shows otherwise, this is the first trade-off to revisit.

## 4. Stage 3: Optimizing in C

Stage 2 fixed the algorithm and measured its cost in nodes. Stage 3 keeps the search exactly
the same (same heuristic, same move order, same pruning) and reduces the instructions per node,
because the hardest state allows about 78. The result,
[`ida/search2.h`](https://github.com/roger8877/minirubik/blob/main/ida/search2.h), is the
version the assembly follows.

### 4.1 Changes and the operation counts behind them

**A. Recursion becomes a loop over an explicit stack.** Each recursive call saves the return
address and registers on the stack and restores them on return. Version 2 keeps one small
frame per level: the state (P, O), the next move to try, the face to skip, and the remaining
moves. 11 frames of 7 bytes replace 11 call frames, so memory goes down, not up, and the
assembly needs no `jal`/`ret` per node.

**B. A child is checked before it is pushed.** Version 1 enters every child and only then finds
it must be pruned. Version 2 computes the child's h in the parent's loop, which version 1 did
too, just one call later, and skips the child if it is pruned. The check is not extra work;
what disappears is the push and pop for every pruned child. Measured over all states, only
**16.7%** of nodes are pushed (table in 4.2): each expanded node generates about 6 children
and about 5 of them are pruned.

**C. Each distance moves into its transition row, and the pointers are pre-scaled.**
Each row now has 10 halfwords: the 9 next states and the distance in entry 9. Looking up the
next state used to need `p * 9` (a multiply, so shifts and adds on RV32I) and the distance
needed a second table. Now the table entries already hold the scaled position of the next row:

| Table | Entry holds | Address of the next row | Max entry |
|---|---|---|---|
| `perm_tab` | rank x 10 | base + entry x 2 (one shift) | 5039 x 10 = 50,390 |
| `orient_tab` | rank x 20 | base + entry (no shift) | 728 x 20 = 14,560 |

`perm_tab` cannot store rank x 20 directly, since 5039 x 20 = 100,780 does not fit in a
16-bit entry, and 32-bit entries would double the table past 128 KiB. `orient_tab` can,
because its ranks are small. Total size grows from 109,611 to 115,380 bytes, still under
131,072.

**D. Each frame stores the moves still allowed (`rem`).** Pruning used to test `g + h > bound`
for every child. Now `rem = bound - g - 1` is computed once when a frame is pushed, and each
child is tested with `h > rem`: one subtraction per push replaces one addition per child,
and there are about 6 children per push.

**E. One loop over moves 0..8.** Version 1 used a face loop and a turn loop and computed
`face * 3 + turn`. Version 2 runs a single counter and, when it reaches the first move of the
previous face (`skip`), jumps ahead by 3. This removes the second loop counter and the index
arithmetic, a few instructions per node.

At the root no move has been made, so `skip` is set to `NO_SKIP = 9`, a value no move number
(0..8) can equal.

### 4.2 Correctness of the restructuring

Version 2 passes H3 on all 3,674,160 states (234.8 s). On every distance-11 state it also
visits exactly the same nodes as version 1, which shows the changes altered how the search
runs, not what it searches.

| distance | mean nodes | mean pushed | pushed / nodes |
|---|---|---|---|
| 8 | 2,860.7 | 477.9 | 0.167 |
| 9 | 13,901.1 | 2,317.9 | 0.167 |
| 10 | 48,135.8 | 8,023.9 | 0.167 |
| 11 | 206,624.8 | 34,438.9 | 0.167 |

### 4.3 Measured on Ripes: gcc reference builds

Both versions were compiled freestanding with
`riscv64-unknown-elf-gcc -O2 -march=rv32i -mabi=ilp32` (no libc, no libgcc; input parsing and
ranking use only adds and shifts, see
[`ida/ida_rv.c`](https://github.com/roger8877/minirubik/blob/main/ida/ida_rv.c)) and run on
RV32_ISS. The disassembly contains no `mul`, `div` or `rem`.

| Build | `21345671111111` iret | `54721631111111` (hardest) iret | `.text` bytes |
|---|---|---|---|
| version 1 | 13,706,318 | 37,477,930 | 1,068 |
| version 2 | 10,534,439 | 28,804,947 | 980 |
| change | −23.1% | −23.1% | −8.2% |

Version 2 spends about 45 instructions per node on the hardest state
(28,804,947 / 639,798), below the 78 available. These gcc figures are the reference the
hand-written assembly has to beat.

## 5. Stage 4: RV32I Assembly
TODO

## 6. Correctness Gates

All host gates are run by [`ida/verify.c`](https://github.com/roger8877/minirubik/blob/main/ida/verify.c)
against an exact BFS over all 3,674,160 states, built from the same transition tables.
Output: [`ida/h3_result.txt`](https://github.com/roger8877/minirubik/blob/main/ida/h3_result.txt).

| Gate | What is checked | Result |
|---|---|---|
| H1 | h ≤ exact distance for every state | PASS. Mean h 5.144, mean distance 8.756; h is exact for 17,108 states |
| H2 | `perm_dist` and `orient_dist` fully populated, solved entry 0, no other zero; every column of `perm_move` and `orient_move` is a permutation of the ranks | PASS. Max 7 and 6 |
| H3 | the search returns a path of exactly the exact distance, and replaying it reaches solved, for every state | PASS. 252 s wall-clock (WSL, i5-14400F) |
| H4 | packed accessor | not applicable: no table is packed (3.3) |

T5–T7 (on Ripes): TODO after stage 4.

## 7. LED Matrix Visualization
TODO

## 8. Pipeline Walkthrough
TODO

## Status at the Phase 1 Deadline

Completed: stage 1 (both measurements), stage 2 (IDA* design, admissibility, gates H1-H3),
stage 3 (version 2 search, measured on Ripes with gcc reference builds).

Not yet completed: the hand-written RV32I assembly (stage 4), the Ripes gates T5-T7 for my own
assembly, the LED matrix renderer and the pipeline walkthrough. The gcc builds in section 4.3
already run on Ripes and stay under the instruction budget, but they are the reference, not my
assembly. I will continue this work after the deadline and document it in later revisions.

## AI Usage Disclosure

**My own work.** I wrote and ran every Ripes measurement program in `ripes-test/` and collected
all the data in this note on my machine. I made the design decisions after comparing the
options against these numbers: IDA* with max(permutation PDB, orientation PDB) as the
heuristic, pruning repeated faces, the rank-based transition tables (option 2 in 3.3, chosen to
spend memory on cheaper nodes while staying under 128 KiB), and the optimizations A-E in 4.1.
I checked my understanding of each step by working through examples by hand (the R move in
section 1, the pipeline cycle counts, the search traces) and by questioning the alternatives.

**AI assistance.** I used Claude (Anthropic) as a tutor and coding assistant:

- explanations of Ripes, RISC-V basics, IDA*, pattern databases and the original `solver.c`;
- the measurement harness `ripes-test/measure.ps1`;
- the C implementation in `ida/` (`cube.h`, `gen.c`, `search.h`, `search2.h`, `ida.c`,
  `verify.c`, `ida_rv.c`, `build_rv.sh`), written by Claude from my design decisions above;
- translating and polishing this note into English from my Chinese explanations.
