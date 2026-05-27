# Huff n Puff High Rise Slot Project Summary

## Repo Roles

- `core/`: authoritative reference and debug/testing harnesses.
- `efficient_core/`: optimized RTP-sweep implementation and optimization documentation.
- `validation/`: validation tooling for equivalence checks.
- `outputs/`: generated logs, summaries, and benchmark outputs.

## Critical Invariants

- Do not modify `core/core.cpp` unless explicitly instructed. It is the reference logic.
- Keep generated outputs under `outputs/`.
- Keep optimized implementations under `efficient_core/`.
- Keep validation scripts/tools under `validation/`.
- Preserve gameplay order:
  1. Generate 6x5 pay window.
  2. Build the initial active pay window and apply EXPHAT active-row adjustment only if EXPHAT is active.
  3. Calculate ways win.
  4. Build active pay window.
  5. Evaluate/apply GIRDER if hats are active.
  6. Recount hats and determine free-game trigger.
- HAT/EXPHAT/HORHAT/VERTHAT count for bonus state but are not paying ways symbols.
- WILD substitutes for paying symbols but is not seeded as its own paying symbol.
- Ways wins are calculated before GIRDER modifies the active window.
- Fixed seeds should produce deterministic results in single-thread mode.

## Completed Task 1

Artifacts:

- `core/core_test.cpp`: deterministic debug/testing clone of the core logic.
- `core/core_flow.md`: detailed reference-flow documentation for `core/core.cpp`.
- `core/edge_cases.md`: documented gameplay and implementation edge cases.
- `outputs/test.txt`: generated debug log.

`core/core_test.cpp` currently:

- Uses deterministic seed `123456789`.
- Runs 25 general randomized spins.
- Logs pay windows, active layouts, RNG draws, detected wins, payout calculations, hat counts, bonus triggers, final summary, hit table, and win table.
- Includes clearly separated forced feature tests:
  - GIRDER via injected HAT.
  - EXPHAT active-row expansion via injected EXPHAT.
  - HAT on first active reel / non-paying ways seed behavior.

Useful command:

```bash
g++ -std=c++17 -Wall -Wextra core/core_test.cpp -o /tmp/core_test
/tmp/core_test
```

## Task 2 Current State

Artifacts:

- `efficient_core/efficient_core.cpp`: optimized simulator.
- `efficient_core/optimization_config.md`: optimization assumptions, decisions, threading, RNG policy, validation, and commands.
- `validation/validate_efficient_core.py`: deterministic validation script.
- `outputs/efficient_summary.txt`: 25-spin optimized validation output.
- `outputs/efficient_benchmark.txt`: 1,000,000-spin development benchmark output.
- `outputs/efficient_thread_smoke.txt`: small multithread smoke output.

Optimization choices:

- Replaced hot-loop `vector<vector<Symbol>>` with fixed-size `std::array` windows.
- Removed per-reel `sliceReels()` temporary allocations.
- Avoided active-window copying except for mutable GIRDER feature logic.
- Replaced position-vector symbol scans with direct count/boolean scans.
- Stored reel strips and paytable as compile-time constants.
- Kept RNG distribution APIs conservative to preserve probability behavior.
- Scans only the active window for EXPHAT expansion, matching current `core/core.cpp`.
- Accumulates symbol win totals for reporting with `WinTable[symbol][left2right] += symbol_win`.
- Kept debug logging out of the optimized hot path.
- Implemented simple optional threading with thread-local aggregates and joined worker threads.

Threading assumptions:

- Single-thread mode is the deterministic validation mode.
- Multithread mode is deterministic for the same seed, spin count, and thread count.
- Multithread mode is not expected to be spin-by-spin identical to single-thread mode because RNG streams are partitioned by worker.
- `--threads N` is an explicit cap; workers are joined before exit; no detached threads are used.

## Validation Strategy

Primary deterministic validation:

```bash
g++ -std=c++17 -Wall -Wextra core/core_test.cpp -o /tmp/core_test
/tmp/core_test
g++ -O3 -std=c++17 -Wall -Wextra efficient_core/efficient_core.cpp -o /tmp/efficient_core
python3 validation/validate_efficient_core.py
```

Expected validation result:

```text
validation passed: efficient_core summary and feature markers match outputs/test.txt
```

Manual optimized run:

```bash
/tmp/efficient_core --spins 25 --seed 123456789 --single-thread --feature-tests --output outputs/efficient_summary.txt
```

Development benchmark only, not a large production sweep:

```bash
/tmp/efficient_core --benchmark --seed 123456789 --threads 1 --output outputs/efficient_benchmark.txt
```

Thread smoke test:

```bash
/tmp/efficient_core --spins 1000 --seed 123456789 --threads 2 --output outputs/efficient_thread_smoke.txt
```

## Task 3 Current State

Artifacts:

- `game.cpp`: playable and simulation-ready runner using `efficient_core/efficient_core.cpp` as backend.
- `validation/validate_game.py`: checks 25-spin fixed-seed game reports against `outputs/test.txt` and validates CSV headers.
- `outputs/rtp_report.txt`: generated RTP report.
- `outputs/symbol_distribution.csv`: generated symbol distribution report matching `gameRule/example_symbol_dist.csv` columns.

Compile:

```bash
g++ -O3 -std=c++17 -Wall -Wextra game.cpp -o /tmp/game
```

Play spin by spin:

```bash
/tmp/game --interactive
/tmp/game --interactive --seed 123456789
/tmp/game --interactive --seed 123456789 --show-stats
```

When `--seed` is omitted, `game.cpp` generates a random startup seed for playable use and prints the chosen seed in the session/report.

Interactive commands:

- Press `Enter`, `s`, or type `spin` to spin.
- Type `stats` to print running RTP, total wager, total payout, and hit frequency.
- Type `q`, `quit`, or `exit` to leave.

Run simulations:

```bash
/tmp/game --spins 1000000 --seed 123456789 --single-thread
/tmp/game --spins 1000000 --seed 123456789 --threads 4
/tmp/game --spins 1000000 --threads 4
/tmp/game --spins 1000000 --threads 4 --rtp-output outputs/rtp_report.txt --symbol-output outputs/symbol_distribution.csv
```

Reports:

- `outputs/rtp_report.txt` includes spins, seed, threads, wager, payout, net, RTP, variance, hit frequency, free-game count, and throughput.
- `outputs/symbol_distribution.csv` uses columns `Symbol,Occurrence,Win,Mode,Hits,RTP` plus metric rows.
- Current engine reports base-game paid spins and free-game triggers; no separate free-spin loop exists yet.

Game validation:

```bash
python3 validation/validate_game.py
```

Expected result:

```text
validation passed: game.cpp reports match outputs/test.txt and CSV schema
```

## Core New Task 4/2/3 Current State

Artifacts:

- `core_new.cpp`: standalone simulator built from `Game_Rules/*.json` and `Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx`.
- `efficient_core/efficient_core_new.cpp`: optimized/thread-capable implementation of `core_new.cpp` behavior.
- `game_new.cpp`: interactive and simulation runner using `efficient_core_new.cpp` as backend.

Important assumptions:

- Final reel strips are not present in the workbook; `core_new.cpp` and `efficient_core_new.cpp` use documented placeholder 100-stop strips from the workbook skeleton counts.
- Final symbol paytable is not present in the workbook; base ways wins are instrumented but return zero.
- Free-spin hazard expansion reuses Base Table D because no separate free-spin hazard table is supplied.
- Full RTP/math validation is blocked until final reel strips and paytable are supplied.

Compile:

```bash
g++ -std=c++17 -Wall -Wextra core_new.cpp -o /tmp/core_new
g++ -O3 -std=c++17 -Wall -Wextra efficient_core/efficient_core_new.cpp -o /tmp/efficient_core_new
g++ -O3 -std=c++17 -Wall -Wextra game_new.cpp -o /tmp/game_new
```

Smoke validation performed:

```bash
/tmp/core_new --spins 1000 --seed 123456789 --output /tmp/RTP_summary_core_new.md
/tmp/efficient_core_new --spins 1000 --seed 123456789 --single-thread --output /tmp/RTP_summary_new.md --symbol-output /tmp/symbol_distribution_new.csv
/tmp/game_new --spins 1000 --seed 123456789 --single-thread --rtp-output /tmp/rtp_report_new.md --symbol-output /tmp/symbol_win_distribution_new.csv
/tmp/efficient_core_new --spins 1000 --seed 123456789 --threads 2 --output /tmp/RTP_summary_new_threads.md --symbol-output /tmp/symbol_distribution_new_threads.csv
```

Fixed-seed single-thread result for all three new executables:

- Spins: `1000`
- Total bet: `20000`
- Total win: `14795`
- Total RTP: `73.9750%`
- Normal FS triggers: `4`
- Super Saw FS triggers: `1`
- Girder triggers: `2`
- Mystery Stack triggers: `132`

## Task 5 Current State

Artifacts:

- `core_new2.cpp`: Task 5 corrected alternative based on `core_new.cpp`.
- `Claude_Review_Verification_Report.md`: issue-by-issue verification against Excel, `Game_Rules/`, and code.
- `Claude_Review_Changes_And_Purpose.md`: traceability for each code change in `core_new2.cpp`.
- `Claude_Review_Issue_Clarifications.md`: confirmed, no-change, and ambiguous issue buckets.
- `outputs/RTP_summary_core_new2_smoke.md`: 1,000-spin smoke output.

Verified changes in `core_new2.cpp`:

- Saw paths skip `BRICK_FRAME` cells.
- Trigger-window saw symbols execute before spin 1.
- Base-game `HAZARD_HARD_HAT` placement is restricted to reels 1, 3, and 5.
- Normal FS `HAZARD_HARD_HAT`, `HORIZONTAL_SAW_HARD_HAT`, and `VERTICAL_SAW_HARD_HAT` placement is restricted to reels 1, 3, and 5.
- RTP output labels clarify overflow/jackpot are subcomponents included in feature totals.
- Base reelset usage label clarifies R1 is the 3-row base spin and R2-R4 are not re-spun after Hazard expansion.

Ambiguous / unresolved:

- Normal FS WILD reel restriction conflicts: Excel `Reelsets_12` gives WILDs on reels 2-5, while `Game_Rules/Free_Spins_3.json` says reels 2-3 only.
- Girder Super Saw throw for exactly 6 non-Super-Saw HHs lacks Excel probability and add/replace mechanics.
- Super Saw FS overflow table is not separately supplied; current code uses FS-D.
- Final reel strips and paytable remain unavailable, so full RTP validation is still blocked.

Validation performed:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic core_new2.cpp -o /tmp/core_new2_check
/tmp/core_new2_check --spins 1000 --seed 123 --output outputs/RTP_summary_core_new2_smoke.md
```

Recommended next step:

- Review the Task 5 markdown reports and resolve the listed source conflicts before promoting `core_new2.cpp` over `core_new.cpp`.

Recommended next step:

- Supply final reel strips and base paytable from the math model, then replace placeholder strip/paytable TODOs and rerun equivalence checks.

## Notes For Fresh Sessions

- Start by reading `AGENTS.md`, `core/core.cpp`, `core/core_flow.md`, `core/core_test.cpp`, `core_new.cpp`, `efficient_core/optimization_config.md`, `efficient_core/efficient_core_new.cpp`, `game.cpp`, `game_new.cpp`, and this file.
- If core behavior changes, update `core/core_test.cpp` first, regenerate `outputs/test.txt`, then update/validate `efficient_core/efficient_core.cpp`.
- Use `outputs/test.txt` as the observable behavior reference for deterministic tests.
- Do not run extremely large simulations automatically. Use 1,000,000 spins only for development benchmarking.
