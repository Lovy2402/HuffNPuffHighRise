# changes275.md

## Purpose

Updated `core_new2.cpp` to use the paytable and all-ways symbol-pay logic from `core/core.cpp`.

## Files Changed

- `core_new2.cpp`

## Source Reference Used

- `core/core.cpp`
  - `PayTable`
  - `symbolArray`
  - `waysWinCalculation()`

## Changes Made

### 1. Added Core Paytable Constants

Added the eight paying symbols:

- `HV1`
- `HV2`
- `HV3`
- `HV4`
- `LV1`
- `LV2`
- `LV3`
- `LV4`

Added `PayTable` values copied from `core/core.cpp`:

| Symbol | 3x | 4x | 5x |
|---|---:|---:|---:|
| HV1 | 20 | 100 | 500 |
| HV2 | 15 | 80 | 400 |
| HV3 | 10 | 60 | 300 |
| HV4 | 8 | 40 | 200 |
| LV1 | 5 | 25 | 100 |
| LV2 | 5 | 20 | 80 |
| LV3 | 3 | 15 | 60 |
| LV4 | 3 | 10 | 50 |

### 2. Added Paying Symbol Helper

Added `isPayingSymbol(Symbol s)` so only `HV1` through `LV4` seed ways wins from reel 1.

### 3. Replaced Stubbed Ways Win Logic

Replaced `evaluateWaysWins()` returning `0` with all-ways evaluation based on `core/core.cpp::waysWinCalculation()`:

- Seed unique paying symbols from the first active reel.
- WILD does not seed its own win.
- WILD substitutes for each seeded paying symbol on every reel.
- Count consecutive left-to-right reels until a reel has zero occurrences.
- Multiply paytable value by the number of ways.
- Sum symbol wins into the base ways win.

### 4. Updated Output Assumption Text

Updated the RTP summary assumption text to state that ways wins use the paytable and all-ways symbol-pay logic from `core/core.cpp`.

## Behavior Impact

- `Base ways wins` in `core_new2.cpp` are no longer always zero.
- Total RTP now includes base ways wins generated from the placeholder reel strips.
- Feature prize, saw, frame, hazard, Girder, and reporting logic from the prior `core_new2.cpp` changes was not removed.

## Validation

Compilation command:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic core_new2.cpp -o /tmp/core_new2_check
```

Smoke command:

```bash
/tmp/core_new2_check --spins 1000 --seed 275 --output outputs/RTP_summary_core_new2_275.md
```
