# Changes Made in core_new2.cpp

## Summary

- `core_new2.cpp` contains behavioral changes.
- It is based on `core_new.cpp`.
- Code changes were made only for verified issues from `Claude_Review.md`.
- Behavioral changes address saw handling, pre-feature saw execution, and reel restrictions.
- Reporting-only changes clarify RTP subcomponents and base reelset usage.

## Change 1: Skip BRICK Frames During Saw Paths

### Related Claude Review Issue
Issue F-1 / D1: saw paths should not affect BRICK-frame positions.

### Why This Change Was Needed
`Game_Rules/Free_Spins_2.json` says saw-touched positions are evaluated only if they are without BRICK FRAME. `core_new.cpp` called `applyFrameHit()` even on BRICK cells.

### What Was Changed
Added a BRICK-frame guard inside the `applySawPath()` `hit` lambda:

```cpp
if (frames[r][c] == BRICK_FRAME) return;
```

### Expected Impact
Saw paths no longer trigger overflow behavior from already-BRICK cells. This should reduce invalid overflow awards and change feature RTP/frame progression.

### Risk / Notes
This relies on the JSON no-BRICK rule. Excel saw behavior notes do not contradict it.

## Change 2: Execute Trigger-Window Saw Paths Before Spin 1

### Related Claude Review Issue
Issue F-3 / D2: pre-feature saw execution was absent.

### Why This Change Was Needed
`Game_Rules/Free_Spins_1.json` says saw hard hats generate saws before the feature begins. Excel `SuperSaw_FS_Tables` recommends executing trigger-position saw paths before spin 1.

### What Was Changed
Added a `TriggerSymbol` struct so trigger positions retain their symbol type. Changed `playFreeGames()` to accept trigger symbols, initialize STRAW frames, then execute saw paths for trigger saw symbols before the spin loop.

### Expected Impact
Initial H-SAW/V-SAW/SUPER-SAW trigger symbols can upgrade frames before the first free spin. This affects feature RTP, especially Super Saw FS.

### Risk / Notes
Excel marks the Super Saw setup saw path as "optional" while the JSON rule is explicit. This should be reviewed by the math owner.

## Change 3: Restrict Base-Game HAZARD Placement Reels

### Related Claude Review Issue
Issue F-4 / D3: HAZARD hard hats in base game should appear only on reels 1, 3, and 5.

### Why This Change Was Needed
`Game_Rules/BaseGame_1.json` states the HAZARD reel restriction. `core_new.cpp` placed all hard-hat types using all five reels.

### What Was Changed
Added `weightedRestrictedOddReel()` and `requiresOddReelRestriction()`. Base-game HAZARD placement now uses 0-indexed reels 0, 2, and 4 with proportional Table G weights.

### Expected Impact
Base-game HAZARD placements no longer occur on reels 2 and 4. Hazard expansion frequency by reel position will align with the rule text.

### Risk / Notes
Excel Table G is not type-specific; the implementation preserves its relative weights across the allowed reels.

## Change 4: Restrict Normal FS HAZARD / H-SAW / V-SAW Placement Reels

### Related Claude Review Issue
Issue F-5 / D4: Normal FS HAZARD, H-SAW, and V-SAW should appear only on reels 1, 3, and 5.

### Why This Change Was Needed
`Game_Rules/Free_Spins_3.json` states the restriction. `core_new.cpp` used all five reels for these placed symbols.

### What Was Changed
`requiresOddReelRestriction()` now restricts Normal FS `HAZARD_HARD_HAT`, `HORIZONTAL_SAW_HARD_HAT`, and `VERTICAL_SAW_HARD_HAT` to 0-indexed reels 0, 2, and 4.

### Expected Impact
Normal FS hazard and saw path geometry changes to match the rule text. Feature RTP and retrigger behavior may change.

### Risk / Notes
No equivalent restriction was applied to Super Saw FS because neither Excel nor `Game_Rules/` clearly states it for Super Saw FS.

## Change 5: Clarify Overflow and Jackpot RTP Rows

### Related Claude Review Issue
Issue F-7: RTP breakdown can appear to double-count overflow and jackpot wins.

### Why This Change Was Needed
Feature totals already include overflow and jackpot wins. The prior labels made separate rows look additive.

### What Was Changed
Renamed the rows to:

- `Overflow prizes (included in feature totals)`
- `Jackpot prizes (included in feature totals)`

Added a note below the RTP table.

### Expected Impact
No simulation behavior change. Reporting is clearer and less likely to be misread.

### Risk / Notes
The table remains non-additive by design because the subcomponent counters are retained for validation.

## Change 6: Clarify Base Reelset Usage Label

### Related Claude Review Issue
Issue F-8 / E38: base reelset usage output is misleading.

### Why This Change Was Needed
Base spins always start with the 3-row reelset. Hazard expansion happens after the spin and does not re-spin R2-R4.

### What Was Changed
Updated the validation counter label to explain that R1 is the 3-row base spin and R2-R4 are not re-spun after Hazard expansion.

### Expected Impact
No simulation behavior change.

### Risk / Notes
None.

## No-Change Items

- Normal FS WILD reel restriction: no change because Excel `Reelsets_12` conflicts with `Game_Rules/Free_Spins_3.json` and `core_new.cpp` matches Excel.
- Girder Super Saw throw at 6 non-Super-Saw HHs: no change because Excel does not provide probability or add/replace mechanics.
- Overflow prize value: no change because code matches Excel Table FS-D.
- House prize discrepancies and `Minix2`: no change because code matches Excel `Prize_Tables`.
- Frame progression: no change because code matches Excel frame upgrade table.
- Normal FS retrigger counting: no change because current feature reel skeleton has no HH-on-strip symbols.
- Super Saw overflow table: no change because no separate SS-D table exists.
- Ways wins: no change because the workbook does not provide a paytable.
- Additional validation statistics: no change because they are enhancement requests, not required verified corrections.
