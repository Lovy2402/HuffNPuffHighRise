# Claude Review Verification Report

## Source Files Reviewed

- `core_new.cpp`
- `Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx`
- `Game_Rules/`
- `Claude_Review.md`

## Executive Summary

- Consolidated Claude issues reviewed: 15
- Valid and changed in `core_new2.cpp`: 6
- Already handled correctly or no code change required: 6
- Invalid against the Excel primary source: 0
- Partially valid / ambiguous source conflict: 3
- `core_new2.cpp` contains behavioral changes for saw handling, trigger-window saw execution, and verified reel restrictions.
- `core_new2.cpp` also contains reporting-only changes for RTP subcomponent labels and base reelset usage wording.
- Compilation status: passed with `g++ -std=c++17 -O2 -Wall -Wextra -pedantic core_new2.cpp -o /tmp/core_new2_check`.
- Smoke status: passed with `/tmp/core_new2_check --spins 1000 --seed 123 --output outputs/RTP_summary_core_new2_smoke.md`.

## Issue-by-Issue Verification

### Issue 1: Saw Paths Touch BRICK Frames

#### Claude's Claim
`applySawPath()` incorrectly calls `applyFrameHit()` on cells that already have `BRICK_FRAME`, causing overflow behavior from saw paths.

#### Source of Truth Check
`Game_Rules/Free_Spins_2.json` says each saw-touched position without BRICK FRAME is evaluated as a hard-hat landing. Excel `SuperSaw_FS_Tables` defines saw path coverage and unique-cell handling but does not override the no-BRICK rule.

#### Code Check
`core_new.cpp::applySawPath()` did not skip `BRICK_FRAME` before calling `applyFrameHit()`.

#### Verdict
Valid

#### Explanation
The JSON rule explicitly excludes BRICK positions from saw effects. The current implementation allowed saws to trigger overflow logic on BRICK cells.

#### Required Action
Changed `core_new2.cpp::applySawPath()` to return immediately for `BRICK_FRAME` cells.

### Issue 2: Normal FS WILD Reel Restriction

#### Claude's Claim
Normal Free Spins WILD symbols should appear only on reels 2 and 3, so `loadTables()` should not add WILDs to reels 4 and 5.

#### Source of Truth Check
`Game_Rules/Free_Spins_3.json` says WILD appears only on reels 2 and 3. However, Excel `Reelsets_12` rows 42-61 give Normal_FS suggested wild counts of 0 on reel 1 and 5 on reels 2, 3, 4, and 5.

#### Code Check
`core_new.cpp::loadTables()` matches the Excel skeleton: `wild_count = reel == 0 ? 0 : 5`.

#### Verdict
Ambiguous

#### Explanation
The Excel model is the primary source for Task 5 and directly contradicts the JSON rule. Because `core_new.cpp` follows the Excel values, this was not treated as a confirmed code defect.

#### Required Action
No code change. Human confirmation is needed before changing Normal_FS WILD strip counts.

### Issue 3: Pre-Feature Saw Execution

#### Claude's Claim
Saw hard hats in the trigger window should execute saw paths before spin 1, but `playFreeGames()` only initializes straw frames.

#### Source of Truth Check
`Game_Rules/Free_Spins_1.json` says before the feature begins and during each free spin, HORIZONTAL and VERTICAL SAW HARD HAT symbols generate saws. Excel `SuperSaw_FS_Tables` setup row 8 says triggering HH positions are `Straw Frames + optional saw path` and recommends executing saw paths before spin 1.

#### Code Check
`core_new.cpp::playFreeGames()` accepted only trigger coordinates and did not retain the trigger symbol type, so it could not execute initial saw paths.

#### Verdict
Valid

#### Explanation
The written rules support initial saw execution, and the Excel Super Saw setup specifically calls it out. The existing code omitted that behavior.

#### Required Action
Changed `core_new2.cpp` to pass trigger symbol types into `playFreeGames()` and execute saw paths after trigger STRAW frames are initialized and before the first spin.

### Issue 4: Base Game HAZARD Reel Restriction

#### Claude's Claim
Base-game HAZARD HARD HAT placement should be restricted to reels 1, 3, and 5.

#### Source of Truth Check
`Game_Rules/BaseGame_1.json` explicitly says HAZARD HARD HAT only appears on reels 1, 3, and 5 during the base game. Excel `Base_Tables` provides Table G reel weights but does not define a type-specific exception.

#### Code Check
`core_new.cpp::placeHardHats()` used `weightedReel()` across all five reels for all placed symbols.

#### Verdict
Valid

#### Explanation
The Excel table gives position weights, while the rules provide the symbol-specific restriction. These can be combined by using Table G weights only over reels 1, 3, and 5.

#### Required Action
Changed `core_new2.cpp::placeHardHats()` to restrict base-game HAZARD placement to 0-indexed reels 0, 2, and 4 using proportional Table G weights.

### Issue 5: Normal FS HAZARD / H-SAW / V-SAW Reel Restriction

#### Claude's Claim
Normal Free Spins HAZARD, HORIZONTAL SAW, and VERTICAL SAW hard hats should appear only on reels 1, 3, and 5.

#### Source of Truth Check
`Game_Rules/Free_Spins_3.json` explicitly states this restriction. Excel `Normal_FS_Tables` gives type weights but no separate reel-position table.

#### Code Check
`core_new.cpp::placeHardHats()` used all five reels for Normal FS placed symbols.

#### Verdict
Valid

#### Explanation
The source rules provide a clear type-specific restriction, and the Excel does not contradict it.

#### Required Action
Changed `core_new2.cpp::placeHardHats()` to restrict Normal FS HAZARD, H-SAW, and V-SAW placement to 0-indexed reels 0, 2, and 4.

### Issue 6: Girder Super Saw Throw at Six Non-Super-Saw HHs

#### Claude's Claim
If exactly 6 non-Super-Saw hard hats land, a Super Saw hard hat may be thrown, but the code never performs that roll.

#### Source of Truth Check
`Game_Rules/Girder_Mystery.json` mentions 5 or 6 scatters except SUPER SAW HARD HAT may throw a SUPER SAW HARD HAT. Excel `Base_Tables` Table E defines Girder trigger chances only for visible HH counts 1-5 and does not provide a probability or replacement/addition rule for count 6.

#### Code Check
`core_new.cpp::rollGirderTrigger()` returns false at `hard_hat_count >= 6`.

#### Verdict
Ambiguous

#### Explanation
The rule text confirms a possible behavior, but the Excel primary source does not provide the probability or whether the Super Saw is added as a seventh symbol or replaces one of the six.

#### Required Action
No code change. This requires math/design clarification.

### Issue 7: RTP Breakdown Double-Counts Overflow and Jackpot Rows

#### Claude's Claim
The RTP table prints Normal FS / Super Saw FS totals and also prints overflow and jackpot totals as if additive, even though those are included in feature totals.

#### Source of Truth Check
This is an internal reporting consistency issue. Excel `RTP_Targets` lists component categories, but the current code's counters store overflow and jackpot as subcomponents of feature wins.

#### Code Check
`core_new.cpp::playFreeGames()` adds overflow into `feature.total_win`; `updateStatistics()` also tracks `stats.overflow_win` and `stats.jackpot_win` separately for reporting.

#### Verdict
Valid

#### Explanation
The total RTP calculation is not affected, but the printed table was easy to misread as an additive breakdown.

#### Required Action
Changed `core_new2.cpp::printResults()` labels to state overflow and jackpot rows are included in feature totals, and added a note below the table.

### Issue 8: Base Reelset Usage Label Is Misleading

#### Claude's Claim
Base reelset usage R1-R4 will show R1 only, which can look like a bug.

#### Source of Truth Check
Excel `Execution_Logic` starts with `Select BaseReelset[activeRows]`, and `core_new.cpp` starts base spins at 3 active rows. Hazard expansion happens after the spin on the same window.

#### Code Check
`core_new.cpp::spinBaseGame()` always increments `reelset_usage_base[0]` because each base spin starts at 3 rows.

#### Verdict
Valid

#### Explanation
The behavior is intentional, but the report wording was insufficient.

#### Required Action
Changed the report label in `core_new2.cpp::printResults()` to explain R1 is the 3-row base spin and R2-R4 are not re-spun after Hazard expansion.

### Issue 9: Overflow Prize JSON Says 10x While Excel Has FS-D

#### Claude's Claim
`Free_Game_1.json` says overflow awards 10x, while code uses the Excel FS-D weighted table.

#### Source of Truth Check
Excel `Normal_FS_Tables` Table FS-D defines weighted overflow prizes 5x, 10x, 15x, 25x, 50x, and 100x.

#### Code Check
`core_new.cpp::awardOverflowPrize()` implements FS-D exactly.

#### Verdict
Already correctly implemented

#### Explanation
Task 5 treats the Excel math model as primary. The code follows Excel.

#### Required Action
No code change.

### Issue 10: House Prize JSON Discrepancies

#### Claude's Claim
House prize JSON conflicts with code/Excel for Stick max prize, Brick max credit prize, and the `Minix2` jackpot tier.

#### Source of Truth Check
Excel `Prize_Tables` includes Stick 7.5x, Brick credit max 100x, and jackpots Mini/Minor/Major/Grand only. `House_Prizes.json` lists Stick 1.5x-4.5x, Brick 7.5x-150x, and `Minix2`.

#### Code Check
`core_new.cpp::awardHousePrize()` matches Excel `Prize_Tables`.

#### Verdict
Already correctly implemented

#### Explanation
The JSON appears inconsistent with the Excel primary source. The code should not add `Minix2` or 150x without the math model being updated.

#### Required Action
No code change.

### Issue 11: Frame Progression JSON Looks Incomplete

#### Claude's Claim
`FreeGame_Frames.json` is incomplete, but code implements None -> Straw -> Stick -> Brick.

#### Source of Truth Check
Excel `Normal_FS_Tables` rows 36-41 define None -> Straw, Straw -> Stick, Stick -> Brick, Brick -> Overflow.

#### Code Check
`core_new.cpp::applyFrameHit()` implements that chain.

#### Verdict
Already correctly implemented

#### Explanation
The code follows Excel.

#### Required Action
No code change.

### Issue 12: Normal FS Retrigger Counting

#### Claude's Claim
The code may count all HH symbols in the window, not only newly landed/placed symbols.

#### Source of Truth Check
Excel `Normal_FS_Tables` setup row 9 says retrigger is `3+ newly landed/placed HH-type symbols`. The Normal_FS reel skeleton has `Optional HH-on-strip Count` of 0 on all reels, so hard hats in feature windows currently come from placement.

#### Code Check
`core_new.cpp::playFreeGames()` counts `hardHatPositions(window, active_rows)` after placement.

#### Verdict
Already correctly implemented under current reel skeleton

#### Explanation
Because feature strips contain no hard hats, all counted HH symbols are placed under the current model. This could need revision if final feature strips add HH symbols.

#### Required Action
No code change.

### Issue 13: Super Saw FS Overflow Table Missing

#### Claude's Claim
Code uses FS-D for Super Saw overflow, but no SS-D table exists.

#### Source of Truth Check
Excel provides only `Normal_FS_Tables` Table FS-D for overflow prizes. No separate Super Saw overflow table was found.

#### Code Check
`core_new.cpp::awardOverflowPrize()` is shared by both features.

#### Verdict
Ambiguous

#### Explanation
The shared table is the safest implementation from available Excel data, but it should be confirmed by the math owner.

#### Required Action
No code change.

### Issue 14: Ways Wins Are Stubbed

#### Claude's Claim
`evaluateWaysWins()` returns zero, so total RTP cannot be fully validated.

#### Source of Truth Check
The workbook includes RTP targets and placeholder strip skeletons but no final symbol paytable values.

#### Code Check
`core_new.cpp::evaluateWaysWins()` explicitly documents the missing paytable and returns zero.

#### Verdict
Valid but no code change required

#### Explanation
This is a known data gap, not a correctable implementation bug without inventing paytable values.

#### Required Action
No code change.

### Issue 15: Missing Validation Statistics

#### Claude's Claim
Additional statistics such as average Normal/Super Saw win, frame-tier counts, jackpot frequencies, and histograms would improve validation.

#### Source of Truth Check
Excel `RTP_Targets` includes average Normal FS and Super Saw FS target ranges plus general validation expectations.

#### Code Check
`core_new.cpp::printResults()` prints a basic RTP/frequency summary but not all suggested counters.

#### Verdict
Valid but no code change required for Task 5

#### Explanation
These are useful enhancements, but Task 5 requested changes only for confirmed Claude issues requiring correction. Adding new reporting counters would be broader than needed.

#### Required Action
No code change.

## Final Conclusion

`Claude_Review.md` should be accepted partially. The confirmed issues fixed in `core_new2.cpp` are:

- Saw paths must skip BRICK frames.
- Trigger-window saw symbols must execute before spin 1.
- Base-game HAZARD placement must be restricted to reels 1, 3, and 5.
- Normal FS HAZARD/H-SAW/V-SAW placement must be restricted to reels 1, 3, and 5.
- RTP reporting must clarify overflow/jackpot subcomponents.
- Base reelset usage output needed clarification.

The main unresolved issues needing human confirmation are Normal FS WILD reel counts, Girder Super Saw throw probability/mechanism at six non-Super-Saw HHs, and whether Super Saw overflow uses the same FS-D table.

`core_new2.cpp` should be reviewed as a corrected alternative to `core_new.cpp`, not automatically promoted without math-owner signoff on the ambiguous source conflicts.
