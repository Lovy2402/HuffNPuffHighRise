# `core.cpp` Flow Documentation

This document explains the current behavior of `core/core.cpp` as the authoritative reference implementation. It is descriptive only; `core/core.cpp` should remain read-only.

## High-Level Execution Flow

The executable starts in `main()` and runs:

```text
main()
  -> simulateAll(10)
       -> simulateOneSpin() repeated N times
            -> generatePayWindow()
            -> getActivePayWindow()
            -> explosiveHAT()
            -> waysWinCalculation()
            -> getActivePayWindow()
            -> allHatCount()
            -> optional girderTrigger()
            -> allHatCount()
            -> return round win and free-game trigger flag
       -> aggregate RTP, variance, and free-game frequency
```

The game uses a 5-reel by 6-row pay window. A spin starts with `active_rows = 3`, meaning only rows `3..5` are initially active. Some feature logic can lower `active_rows`, which expands the active area upward.

## Static Game Data

### Symbols

`Symbol` defines the full symbol set:

```text
HV1, HV2, HV3, HV4,
LV1, LV2, LV3, LV4,
WILD,
HAT, EXPHAT, HORHAT, VERTHAT,
BLANK
```

The paying symbol array is:

```text
HV1, HV2, HV3, HV4, LV1, LV2, LV3, LV4
```

Important behavior:

- `WILD` substitutes during ways evaluation.
- `WILD` is not evaluated as its own paying seed symbol.
- `HAT`, `EXPHAT`, `HORHAT`, and `VERTHAT` are feature symbols, not paying ways symbols.
- `BLANK` exists in the enum but is not present in the base reel strips.

### Paytable

The paytable is indexed by symbol enum value and left-to-right match length. It has columns for match lengths `0..5`.

Only `3`, `4`, and `5` reel matches pay. Match lengths `0`, `1`, and `2` pay zero.

The paytable includes a ninth row of zeros for special symbols, but regular win evaluation only seeds from the eight paying symbols.

### Reel Strips

The base-game reel strips are `BG1_reel1` through `BG1_reel5`. `BG1_Reels` stores pointers to those strips, and `BG1_Reelsize` stores each strip length.

Only reel 1 includes `EXPHAT` in the current strips. Reels 2-5 include `WILD`. Several reels include `HAT`.

## Randomness

The global RNG is:

```cpp
std::mt19937 rng(
    std::chrono::high_resolution_clock::now()
        .time_since_epoch()
        .count()
);
```

This means normal `core.cpp` runs are not deterministic. Each process starts from a time-derived seed.

Random helper functions:

- `getRandom(a, b)` returns an inclusive integer in `[a, b]`.
- `getUniform()` returns a floating-point value from `[0.0, 1.0)`.
- `SRSWOR(allowedPositions, number)` shuffles a copy of allowed positions and returns the first `number` positions, capped by available size.

## Pay Window Generation

`generatePayWindow(Symbol* ReelSet[], vector<int> ReelSize)` builds a `6 x 5` matrix:

```text
pay_window[row][reel]
```

For each reel:

1. Pick a random starting index from `0` to `ReelSize[reel] - 1`.
2. Fill all 6 rows by reading sequential strip symbols.
3. Wrap around the strip with modulo arithmetic.

Pseudo-flow:

```text
for each reel:
  start_idx = random strip stop
  for row 0..5:
    strip_idx = (start_idx + row) % reel_size
    pay_window[row][reel] = reel_strip[strip_idx]
```

The output is the full generated window. Active rows are handled later.

## Active Window Handling

`getActivePayWindow(pay_window, active_rows)` returns rows from `active_rows` through row `5`.

Examples:

```text
active_rows = 3 -> active rows are 3, 4, 5 -> 3 active rows
active_rows = 2 -> active rows are 2, 3, 4, 5 -> 4 active rows
active_rows = 1 -> active rows are 1, 2, 3, 4, 5 -> 5 active rows
active_rows = 0 -> active rows are 0, 1, 2, 3, 4, 5 -> 6 active rows
```

The variable name `active_rows` acts like a starting row index, not a count of active rows.

## Ways Win Calculation

`waysWinCalculation(pay_window, start_idx)` evaluates paying ways over rows `start_idx..5`.

The function uses the full `pay_window`, but all reel slices begin at `start_idx`, so inactive rows above `start_idx` are ignored.

### Step 1: Determine Seed Symbols From Reel 1

The function slices reel 0 from `start_idx` to row 5.

It counts occurrences of symbols in that slice using:

```cpp
vector<int> symbol_count(no_of_symbols, 0);
```

Since `no_of_symbols` is 8, this only safely represents symbols `HV1..LV4`. The code then builds `uniqueSymbols` by scanning the fixed paying symbol array. As a result:

- Only paying symbols visible on the first active reel can seed a ways evaluation.
- `WILD` on reel 1 does not seed a ways win by itself.
- HAT-like symbols do not seed ways wins.

### Step 2: Count Consecutive Reel Occurrences

For each seed symbol:

1. Start with `left2right = 0` and `ways = 1`.
2. For each reel from left to right:
   - Slice the active rows for that reel.
   - Count occurrences of the seed symbol or `WILD`.
   - If occurrence count is positive:
     - Multiply `ways` by occurrence count.
     - Increment `left2right`.
   - Otherwise stop evaluation for that seed symbol.

This implements adjacent left-to-right ways.

### Step 3: Calculate Payout

For each seed symbol:

```text
symbol_win = PayTable[symbol][left2right] * ways
round_win += symbol_win
```

Then the global tracking tables are updated:

```cpp
HitTable[symbol][left2right]++;
WinTable[symbol][left2right] = symbol_win;
```

Important note: `WinTable` is assigned, not accumulated. That means it stores the most recent symbol win for that symbol and match length, not the cumulative total. This may be intentional for the reference implementation and should be preserved when matching `core.cpp` exactly.

## EXPHAT Feature Flow

`explosiveHAT(pay_window, active_rows)` checks whether `EXPHAT` is present in the active pay window passed into the function.

Current call sequence:

```text
active_rows = 3
pay_window = generatePayWindow(...)
active_pay_window = getActivePayWindow(pay_window, active_rows)
active_rows = explosiveHAT(active_pay_window, active_rows)
```

Important behavior:

- EXPHAT is searched only inside the initial active window, not the whole generated 6x5 window.
- If no active EXPHAT exists, `active_rows` is unchanged.
- If at least one active EXPHAT exists, a random uniform value is drawn.

The EXPHAT probability table is:

```text
0.75, 0.20, 0.05
```

`cumProbability()` maps the random value to an index:

```text
p < 0.75 -> index 0
p < 0.95 -> index 1
otherwise -> index 2
```

The returned active start row is:

```text
active_rows - (index + 1)
```

With the initial value `active_rows = 3`, this yields:

```text
index 0 -> active_rows = 2 -> 4 active rows
index 1 -> active_rows = 1 -> 5 active rows
index 2 -> active_rows = 0 -> 6 active rows
```

Ways wins are calculated after this active-row adjustment.

## HAT Counting

`allHatCount(pay_window)` counts all feature-hat symbols in the provided window:

```text
HAT, EXPHAT, HORHAT, VERTHAT
```

The function does not inspect inactive rows unless the caller passes the full window. In `simulateOneSpin()`, it is called on `active_pay_window`, so the count applies only to the active area.

## GIRDER Feature Flow

After ways wins are calculated, `simulateOneSpin()` rebuilds the active window with the possibly updated `active_rows`, then counts hats:

```text
active_pay_window = getActivePayWindow(pay_window, active_rows)
hat_counts = allHatCount(active_pay_window)
```

If at least one hat-like symbol is active:

1. Draw a uniform random value.
2. If the value is below `girder_threshold`, trigger GIRDER.

The GIRDER threshold is:

```text
0.1
```

So the feature has a 10% trigger chance when at least one active hat-like symbol exists.

### GIRDER Mutation

`girderTrigger(active_pay_window)` modifies the active window only.

It:

1. Counts existing hat-like symbols.
2. Builds an allowed-position list of all cells that are not already hat-like symbols.
3. Calculates:

```text
required_hats = max(0, 6 - existing_hat_count)
```

4. Selects `required_hats` positions using `SRSWOR`.
5. Replaces selected symbols with `HAT`.

This attempts to bring the active window to at least 6 total hat-like symbols. If the active window already has 6 or more hats, no new positions are selected.

Important behavior:

- GIRDER does not mutate the original full `pay_window` in `simulateOneSpin()`.
- GIRDER happens after ways payout calculation.
- Therefore GIRDER-injected HATs do not affect the current spin's ways payout.
- GIRDER can affect the free-game trigger check because hat count is recalculated after GIRDER.

## Free-Game Trigger Flow

At the end of `simulateOneSpin()`:

```text
hat_counts = allHatCount(active_pay_window)
if hat_counts >= 6:
  free_game = 1
```

The free-game flag is binary for the spin. The current reference implementation records whether a free game was triggered, but it does not simulate a free-spin round or bonus sequence.

## Single Spin Flow In Detail

`simulateOneSpin()` performs this exact order:

1. Initialize:
   - `active_rows = 3`
   - `free_game = 0`
   - probability table object
2. Generate full 6x5 pay window.
3. Build initial active window from rows `3..5`.
4. Run EXPHAT check on that initial active window.
5. Update `active_rows` if active EXPHAT expands the window.
6. Calculate ways win using the full pay window and final `active_rows`.
7. Rebuild active window using final `active_rows`.
8. Count active hat-like symbols.
9. If at least one active hat exists:
   - Draw GIRDER random value.
   - If below `0.1`, mutate active window through `girderTrigger()`.
10. Recount active hat-like symbols.
11. If active hat count is at least 6, set `free_game = 1`.
12. Return:
   - `round_win`
   - `free_game`

The order is important for validation. In particular, ways win calculation happens before GIRDER injection.

## Simulation Aggregation

`simulateAll(N)` runs `simulateOneSpin()` `N` times.

For each spin:

```text
round_win_xbet = round_win / bet
mean += round_win_xbet
var += round_win_xbet * round_win_xbet
total_free_games += free_game
```

After all spins:

```text
mean = mean / N
var = var / N - mean * mean
```

The printed RTP value is `mean`, where each spin win is normalized by `bet = 20`.

The free-game frequency output is:

```text
free_game_trigger = N / total_free_games
```

if at least one free-game trigger occurred, otherwise it remains `0.0`.

The current `main()` calls:

```cpp
simulateAll(10);
```

so the reference executable runs only 10 spins by default.

## Important Implementation Observations

- `core.cpp` is non-deterministic because RNG is seeded from high-resolution time.
- `std::random_device rd` is declared but not used.
- `SymbolChar` only includes names through `HAT`, not all enum symbols.
- `printPayWindow()` prints `EXPHAT` as `XHAT`; `HORHAT`, `VERTHAT`, and `BLANK` fall through to `UNK`.
- `SRSWOR()` declares `totalSize` but does not use it.
- `waysWinCalculation()` creates temporary vectors repeatedly through `sliceReels()`.
- `waysWinCalculation()` assigns `WinTable[symbol][left2right] = symbol_win` instead of accumulating.
- `HitTable` is incremented for every seed symbol evaluation, even if the match length pays zero.
- EXPHAT affects the active-row start index before ways payout.
- GIRDER affects only bonus/free-game trigger state for the current spin, not the already-calculated ways payout.

## Validation-Sensitive Invariants

Implementations that claim to match `core.cpp` should preserve:

- 6 rows and 5 reels.
- Initial `active_rows = 3`.
- Active-window interpretation as rows `active_rows..5`.
- EXPHAT lookup limited to the current active window.
- EXPHAT row expansion probabilities `75% / 20% / 5%`.
- Ways evaluation after EXPHAT and before GIRDER.
- Ways seed symbols coming only from paying symbols present on the first active reel.
- WILD substitution for paying symbols.
- HAT-like symbols counted as `HAT`, `EXPHAT`, `HORHAT`, and `VERTHAT`.
- GIRDER eligibility only when at least one active hat-like symbol exists.
- GIRDER trigger threshold of `0.1`.
- GIRDER injection target of at least 6 active hat-like symbols.
- Free-game trigger when active hat-like count is at least 6 after optional GIRDER.
- Base spin return value containing only `round_win` and `free_game`; no free-spin loop is simulated in `core.cpp`.
