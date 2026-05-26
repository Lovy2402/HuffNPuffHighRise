# Efficient Core Optimization Config

## Implementation

- Source: `efficient_core/efficient_core.cpp`
- Output: `outputs/efficient_summary.txt` by default
- Reference logic: `core/core.cpp`
- Debug validation reference: `outputs/test.txt`

## Optimization Assumptions

- The optimized implementation preserves gameplay order from `core/core.cpp`.
- `core/core.cpp` remains read-only and authoritative.
- Non-paying first-reel symbols are not seeded as paying ways symbols. This matches the debuggable behavior logged in `outputs/test.txt`.
- Debug logging is not part of the optimized hot path.
- The optimized hot path uses fixed-size arrays for the 6x5 window and active windows to avoid per-spin heap allocation.

## Optimization Changes Explained

This section describes what changed from the reference style and why those changes help performance.

### Fixed-size arrays instead of nested vectors

Reference-style pattern:

```cpp
vector<vector<Symbol>> pay_window(no_of_rows, vector<Symbol>(no_of_reels));
```

Optimized pattern:

```cpp
using Window = array<array<Symbol, NO_OF_REELS>, NO_OF_ROWS>;
Window window{};
```

Why this helps:

- The window size is always `6 x 5`, so dynamic allocation is unnecessary.
- `std::array` stores the data inline, which is more cache-friendly.
- It avoids repeated heap allocations across millions of spins.
- The compiler can optimize loops better when dimensions are compile-time constants.

General lesson: when data has a small fixed shape, prefer fixed-size arrays over heap-backed containers.

### No `sliceReels()` allocation in ways calculation

Reference-style pattern:

```cpp
vector<Symbol> sliced_reel = sliceReels(pay_window, idx, start_idx);
```

Optimized pattern:

```cpp
for (int row = start_row; row < NO_OF_ROWS; row++) {
    Symbol current = window[row][reel];
}
```

Why this helps:

- The reference creates a new vector every time it checks a reel.
- Ways calculation runs for several symbols and several reels every spin.
- Reading directly from the original window avoids temporary containers.

General lesson: in hot loops, avoid building temporary collections when direct indexed traversal is simple.

### Active window copying reduced to feature logic only

Reference behavior creates an active pay window after ways calculation. The optimized code avoids that copy for pure counting and payout paths where possible, but still uses a small fixed-size `ActiveWindow` for GIRDER because GIRDER mutates active symbols.

Why this helps:

- Counting hats can be done directly over active rows in the original window.
- GIRDER needs a mutable active-region view, so a fixed-size buffer is still appropriate there.
- This keeps mutation local and avoids heap allocation.

General lesson: copy data only when the algorithm truly needs an independent mutable version.

### Direct hat and EXPHAT scans

Reference-style code returns structs/vectors of positions for symbol lookup. The optimized simulation only needs to know whether EXPHAT exists in the active window and how many hats are active.

Optimized functions:

```cpp
bool hasExplosiveHat(const Window& window, int start_row);
int countHatWindow(const Window& window, int start_row);
int countHatActive(const ActiveWindow& window, int rows);
```

Why this helps:

- Avoids storing position vectors when only a count or boolean is needed.
- Reduces memory writes and object construction.
- Keeps logic explicit and easy to audit.

General lesson: return only the information the caller needs in performance-sensitive code.

### Paytable and reels stored as compile-time constants

The optimized implementation stores reel strips, paytable values, symbol counts, and dimensions as `constexpr` values.

Why this helps:

- Constants are known at compile time.
- The compiler can inline and simplify more aggressively.
- It avoids accidental runtime mutation of reel strips or pay values.

General lesson: make game math tables immutable when they are not supposed to change at runtime.

### Distribution construction kept simple but isolated

The optimized code still uses `std::uniform_int_distribution` and `std::uniform_real_distribution` to preserve RNG behavior with `std::mt19937`. The distribution calls are isolated inside `getRandom()` and `getUniform()`.

Why this is conservative:

- Replacing distributions with modulo arithmetic could bias results or break equivalence.
- Keeping distribution APIs preserves correctness while still allowing the rest of the loop to be optimized.

General lesson: do not optimize random sampling by changing probability semantics unless you have a statistical validation plan.

### Aggregation uses thread-local state

Each worker thread owns an `Aggregate` object:

```cpp
vector<Aggregate> local_results(threads);
```

Workers write only to their own slot, then the main thread merges results after all workers join.

Why this helps:

- No locks in the hot spin loop.
- No shared counters that cause contention.
- Worker threads terminate cleanly and are joined before process exit.

General lesson: for simulations, prefer independent worker state and final reduction over shared mutable counters.

### Symbol win totals are accumulated for reports

The aggregate tables now keep cumulative distribution data:

```cpp
HitTable[symbol][left2right]++;
WinTable[symbol][left2right] += symbol_win;
```

Why this matters:

- Hit counts and win totals are report data, not gameplay state.
- Cumulative win totals allow `game.cpp` to calculate symbol-level RTP.
- The change does not alter RNG order, pay-window generation, ways calculation, GIRDER, EXPHAT, or final spin payouts.

General lesson: separate game result logic from reporting aggregation, and make aggregation semantics explicit.

### Debug output removed from the hot path

The debug implementation logs every intermediate state. The optimized implementation writes only summaries by default and feature-test output only when `--feature-tests` is requested.

Why this helps:

- File I/O is much slower than the game math.
- Logging every spin would dominate runtime and hide actual simulation performance.

General lesson: keep observability builds and performance builds separate, but validate them against each other.

## Behavior-Preservation Notes

- The optimized code preserves the gameplay order from `core/core.cpp`.
- EXPHAT expansion is evaluated only when EXPHAT appears in the current active window.
- Ways wins are calculated before GIRDER modifies the active window.
- HAT, EXPHAT, HORHAT, and VERTHAT count for bonus state but are not paying ways symbols.
- WILD substitutes for paying symbols but is not seeded as its own paying symbol.
- Single-thread mode with fixed seed is the validation mode.
- Multi-thread mode is deterministic only for the same seed, spin count, and thread count.

## Benchmark Settings

- Default run: `25` spins for deterministic validation.
- Development benchmark: `1,000,000` spins via `--benchmark`.
- Do not run 1 billion spins automatically.

Example benchmark command:

```bash
g++ -O3 -std=c++17 efficient_core/efficient_core.cpp -o /tmp/efficient_core
/tmp/efficient_core --benchmark --seed 123456789 --threads 1 --output outputs/efficient_benchmark.txt
```

## Thread Limits

- Threading is optional and simple.
- Default thread count is approximately 10% of hardware threads, with a minimum of 1.
- `--threads N` sets an explicit cap and is never exceeded.
- `--single-thread` forces one thread for deterministic validation against `outputs/test.txt`.
- Worker threads are joined before program exit. No detached threads are used.

## RNG Policy

- Single-thread mode uses `std::mt19937` seeded with `--seed`.
- Single-thread validation with seed `123456789` is intended to match the debug test sequence.
- Multi-thread mode derives one independent deterministic seed per worker from the base seed and worker index.
- Multi-thread results are deterministic for the same seed, spin count, and thread count, but are not expected to be spin-by-spin identical to single-thread mode because RNG streams are partitioned.
- `game.cpp` uses a random startup seed when `--seed` is omitted, so playable sessions do not repeat by default.

## Validation Strategy

- Compile the debug harness and regenerate `outputs/test.txt`.
- Compile the optimized implementation.
- Run optimized single-thread deterministic validation with 25 spins and feature tests enabled.
- Compare summary and feature-state markers against `outputs/test.txt`.

Commands:

```bash
g++ -std=c++17 -Wall -Wextra core/core_test.cpp -o /tmp/core_test
/tmp/core_test
g++ -O3 -std=c++17 efficient_core/efficient_core.cpp -o /tmp/efficient_core
/tmp/efficient_core --spins 25 --seed 123456789 --single-thread --feature-tests --output outputs/efficient_summary.txt
python3 validation/validate_efficient_core.py
```

## CLI Options

- `--spins N`: number of spins to simulate.
- `--seed S`: base RNG seed.
- `--threads T`: number of worker threads.
- `--single-thread`: force one worker.
- `--benchmark`: use 1,000,000 development spins.
- `--feature-tests`: append forced GIRDER, EXPHAT, and HAT feature checks to the output.
- `--output PATH`: write summary output to the selected path under `outputs/`.

## Game Runner

Task 3 adds `game.cpp`, which reuses this implementation by compiling the optimized core as a library-style backend. It provides:

- Interactive spin-by-spin play.
- Simulation runs with the same threading policy.
- RTP report output.
- Symbol distribution CSV output using cumulative hit and win totals.

Example commands:

```bash
g++ -O3 -std=c++17 -Wall -Wextra game.cpp -o /tmp/game
/tmp/game --interactive --seed 123456789 --show-stats
/tmp/game --spins 1000000 --seed 123456789 --threads 4 --rtp-output outputs/rtp_report.txt --symbol-output outputs/symbol_distribution.csv
python3 validation/validate_game.py
```

## Core New Task 2/3 Additions

New files:

- `efficient_core/efficient_core_new.cpp`: optimized backend for `core_new.cpp` behavior.
- `game_new.cpp`: user-facing runner using `efficient_core/efficient_core_new.cpp` as its backend.

`efficient_core_new.cpp` keeps the Task 4 source assumptions from `core_new.cpp`:

- Game flow and feature behavior come from `Game_Rules/*.json`.
- Weights/prizes/reelset skeleton come from `Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx`.
- Final reel strips are still unavailable; placeholder 100-stop strips use the workbook skeleton counts.
- Final symbol paytable is still unavailable; base ways wins remain instrumented and pay zero.

Optimization notes for `core_new.cpp` behavior:

- Moved RNG into an `Engine` object so each worker owns independent deterministic state.
- Replaced dynamically sized reel strips with fixed `array<Symbol, 100>` strips matching the workbook skeleton length.
- Kept 6x5 windows as fixed `std::array` values.
- Added thread-local `Statistics` aggregation and final merge with all worker threads joined.
- Default thread count is conservative via `defaultThreadCount()`, roughly 8-10% of hardware threads.
- `--threads N` is an explicit cap; `--single-thread` is the deterministic equivalence mode.

Compile and smoke-test commands:

```bash
g++ -O3 -std=c++17 -Wall -Wextra efficient_core/efficient_core_new.cpp -o /tmp/efficient_core_new
/tmp/efficient_core_new --spins 1000 --seed 123456789 --single-thread --output /tmp/RTP_summary_new.md --symbol-output /tmp/symbol_distribution_new.csv
/tmp/efficient_core_new --spins 1000 --seed 123456789 --threads 2 --output /tmp/RTP_summary_new_threads.md --symbol-output /tmp/symbol_distribution_new_threads.csv

g++ -O3 -std=c++17 -Wall -Wextra game_new.cpp -o /tmp/game_new
/tmp/game_new --spins 1000 --seed 123456789 --single-thread --rtp-output /tmp/rtp_report_new.md --symbol-output /tmp/symbol_win_distribution_new.csv
```

Single-thread validation status:

- `core_new.cpp`, `efficient_core_new.cpp`, and `game_new.cpp` produce the same 1,000-spin fixed-seed totals for seed `123456789`.
- Full math validation is not claimed because the workbook does not include final reel strips or a base paytable.
