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

### 2.1 What solver.c computes

TODO: in your own words —
- What is the input (14-digit string) and what is the output?
- How is a cube represented (`p[7]`, `o[7]`) and why only 7 cubies?
- Which invariants must a valid state satisfy (see `valid()`)?
- What does `build_table()` do, step by step?

### 2.2 Where the cost lies

| Allocation | Storage | Bytes |
|---|---|---|
| One move toward solved per state | heap | 3,674,160 |
| BFS queue of 32-bit ranks | heap | 14,696,640 |
| Factored quarter-turn transitions | automatic | 34,614 |
| **Peak** | | **18,405,414** |

TODO: explain where each number comes from (e.g. 14,696,640 = 3,674,160 x 4 bytes),
and the 66,134,880 transition updates (3,674,160 states x 9 edges x 2 tables).

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
[`measure.ps1`](../ripes-test/measure.ps1). Each program ran 5-10 times; raw data in
[`results_rv32iss.csv`](../ripes-test/results_rv32iss.csv).

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

TODO (your own words):
- Why use the slope between experiment runs instead of (experiment − control) / N? (the 1 MiB row)
- Estimate from the VSRTL container declaration what one guest byte should cost, and compare with 80.
- What the occasional jumps in sampled private bytes (338 → 402 MiB at 4 MiB, 1339 → 1596 MiB at 16 MiB) suggest.

**Projection.** 80.27 x 18,405,414 ≈ 1.48 x 10^9 bytes ≈ **1.38 GiB** of host memory
for the baseline's peak, plus about 27 MiB for Ripes itself.

TODO: compare with this machine's 15.8 GiB and state your conclusion.

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

**RV32_5S** (3 runs each, raw data in [`results_log.csv`](../ripes-test/results_log.csv)):

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

TODO (your own words):
- Why the same instruction count runs ~7x slower on RV32_ISS when every store hits a new address,
  and why that difference disappears on RV32_5S.
- How much slower RV32_5S is than RV32_ISS, and why (instruction-level vs circuit-level simulation).
- Why the RV32_5S control grows with run length (hint: the GUI can step backwards).
- What the ~2047 MiB ceiling at `mem_16M` might be.
- Compare with the assignment's 1.09 M/s (RV32_ISS) and 20.5 k/s (RV32_5S) and explain the difference.

### 2.5 Why report.md §7 does not survive the move to Ripes

TODO: combine 2.3 and 2.4 — what keeping the full table would cost on Ripes in memory and in time,
and therefore why the design must change.

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
