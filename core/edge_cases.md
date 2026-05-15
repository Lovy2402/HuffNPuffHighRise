# Gameplay Edge Cases

## Free-game trigger denominator can be zero

- Edge case: `simulateAll()` can see `total_free_games == 0` for short deterministic runs.
- Why it matters: free-game frequency reporting must not divide by zero.
- Expected behavior: the guarded harness reports `0.0` when no free games trigger.
- Possible failure modes: validation runs with small `N`, alternate seeds, or altered strips may report an invalid frequency or fail before summary output.
- Suggested validation strategy: include deterministic seeds that produce zero and nonzero free-game counts and verify both summary paths.

## HAT symbols do not pay in ways wins

- Edge case: `HAT`, `EXPHAT`, `HORHAT`, and `VERTHAT` are bonus symbols, not paying symbols in `waysWinCalculation()`.
- Why it matters: only symbols present in `symbolArray` are evaluated for ways pays.
- Expected behavior: hats may trigger bonus logic but should not contribute symbol paytable wins.
- Possible failure modes: optimized implementations may treat all enum values as payable or may count hats as blockers incorrectly.
- Suggested validation strategy: build windows with hats on the first active reel and verify that no hat pay is calculated while bonus counts still include hat variants.

## Girder changes bonus state after ways payout

- Edge case: girder logic runs after `waysWinCalculation()`.
- Why it matters: forced GIRDER tests can change hat counts and free-game state without changing the already-calculated ways payout for that spin.
- Expected behavior: payout remains the pre-girder ways win; bonus state is evaluated after girder modifications.
- Possible failure modes: optimized implementations may recalculate ways after girder symbol injection and overstate payouts.
- Suggested validation strategy: force GIRDER on an active window and compare pre-feature payout with post-feature hat counts and free-game state.

## EXPHAT can expose non-paying symbols in the expanded active region

- Edge case: EXPHAT expansion lowers `active_rows`, adding more rows to the active pay window. Those newly active rows can include HAT, EXPHAT, or WILD on the first reel.
- Why it matters: expanded windows affect both payout evaluation and bonus hat counts.
- Expected behavior: expanded active rows are included in ways and hat-count calculations; non-paying first-reel symbols are not paying ways seeds.
- Possible failure modes: optimized implementations may expand the visual window but forget to use the expanded row set for payout or bonus counts.
- Suggested validation strategy: inject EXPHAT, force each expansion threshold bucket, and compare active row count, ways payout, and hat counts before and after expansion.

## Current reference summary guard has invalid C++ syntax

- Edge case: the current `core/core.cpp` uses `if total_free_games > 0:` in `simulateAll()`.
- Why it matters: the authoritative reference file does not compile until that guard is expressed as valid C++.
- Expected behavior: use `if (total_free_games > 0) { ... }` style syntax while preserving the intended zero-denominator guard.
- Possible failure modes: validation cannot compile the reference, blocking equivalence testing.
- Suggested validation strategy: compile `core/core.cpp` before equivalence runs and treat compiler failures as setup blockers.

## Non-paying symbols on the first active reel can index outside `symbol_count`

- Edge case: `waysWinCalculation()` allocates `symbol_count` with `no_of_symbols == 8`, then indexes it directly by enum value from the first active reel. `WILD`, `HAT`, and hat variants have enum values outside `0..7`.
- Why it matters: a first active reel containing `WILD` or any hat symbol can write outside the vector bounds.
- Expected behavior: `core_test.cpp` logs and ignores non-paying first-reel symbols for ways seeding so the debug run can complete while making the reference-risk path visible.
- Possible failure modes: crashes, memory corruption, seed-dependent validation differences, or optimized implementations that produce different results after adding bounds checks.
- Suggested validation strategy: include deterministic windows where each non-paying symbol appears on the first active reel; run with sanitizers in a separate diagnostic build to expose the out-of-bounds write.

## Wilds substitute but are not evaluated as their own paying symbol

- Edge case: `WILD` contributes to occurrences for each paying symbol after a paying symbol appears on the first active reel, but `WILD` itself is not in `symbolArray`.
- Why it matters: a first active reel containing only wilds will not seed any symbol evaluation.
- Expected behavior: wild-only first reels produce no ways wins in the current reference logic.
- Possible failure modes: alternate implementations may evaluate wilds as a standalone symbol or expand them into every possible paying symbol.
- Suggested validation strategy: test windows with wild-only first active reel, mixed wild/paying symbols, and wilds on later reels.

## `WinTable` stores the latest symbol win, not cumulative win

- Edge case: `WinTable[symbol][match] = symbol_win` overwrites prior values while `HitTable` increments.
- Why it matters: `WinTable` is not an aggregate payout table despite the name.
- Expected behavior: debug and validation code should treat `WinTable` as last-observed value unless the reference is intentionally changed elsewhere.
- Possible failure modes: validation may compare cumulative expected wins against a table that only stores the latest spin contribution.
- Suggested validation strategy: inspect per-spin detected wins in `outputs/test.txt` rather than relying only on final `WinTable` values.
