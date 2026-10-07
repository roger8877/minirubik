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
TODO

## 4. Stage 3: Optimizing in C
TODO

## 5. Stage 4: RV32I Assembly
TODO

## 6. Correctness Gates
TODO

## 7. LED Matrix Visualization
TODO

## 8. Pipeline Walkthrough
TODO

## AI Usage Disclosure
TODO: e.g. "Claude (Anthropic) was used to explain Ripes usage and RISC-V basics, to help write the
measurement harness `measure.ps1`, and to translate/polish the English of this note from my drafts.
All measurements were run by me, and the design decisions and analysis are my own."
