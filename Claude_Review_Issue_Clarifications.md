# Claude Review Issue Clarifications

## Purpose

This file clarifies whether each issue from `Claude_Review.md` requires a code fix, explanation only, or no action.

## Confirmed Valid Issues

### Saw Paths Touch BRICK Frames

- Issue summary: saw paths applied `applyFrameHit()` to BRICK-frame cells.
- Reason it is valid: `Game_Rules/Free_Spins_2.json` excludes BRICK-frame positions from saw evaluation.
- Code area affected: `applySawPath()`.
- Fixed in `core_new2.cpp`: yes.

### Pre-Feature Saw Execution Missing

- Issue summary: trigger-window saw symbols did not fire before spin 1.
- Reason it is valid: `Game_Rules/Free_Spins_1.json` and Excel `SuperSaw_FS_Tables` support pre-spin saw execution.
- Code area affected: `playFreeGames()` and feature trigger setup in `spinBaseGame()`.
- Fixed in `core_new2.cpp`: yes.

### Base-Game HAZARD Reel Restriction

- Issue summary: base-game HAZARD placement could use any reel.
- Reason it is valid: `Game_Rules/BaseGame_1.json` restricts HAZARD to reels 1, 3, and 5.
- Code area affected: `placeHardHats()`.
- Fixed in `core_new2.cpp`: yes.

### Normal FS HAZARD / H-SAW / V-SAW Reel Restriction

- Issue summary: Normal FS special hard hats could use any reel.
- Reason it is valid: `Game_Rules/Free_Spins_3.json` restricts these symbols to reels 1, 3, and 5.
- Code area affected: `placeHardHats()`.
- Fixed in `core_new2.cpp`: yes.

### RTP Subcomponent Reporting

- Issue summary: overflow and jackpot rows could be read as additive to feature totals.
- Reason it is valid: counters are subcomponents already included in feature totals.
- Code area affected: `printResults()`.
- Fixed in `core_new2.cpp`: yes, by relabeling and adding an explanatory note.

### Base Reelset Usage Reporting

- Issue summary: base R1-R4 usage labels made R2-R4 zero counts look suspicious.
- Reason it is valid: base spins start at 3 rows and do not re-spin after Hazard expansion.
- Code area affected: `printResults()`.
- Fixed in `core_new2.cpp`: yes, by clarifying the label.

## Valid But No Code Change Required

### Overflow Prize Uses FS-D

- Claude correctly identified the JSON/Excel discrepancy.
- Existing code follows Excel Table FS-D, which is the Task 5 primary source.
- No code change required.

### House Prize JSON Conflicts

- Claude correctly identified JSON differences for Stick max, Brick max, and `Minix2`.
- Existing code follows Excel `Prize_Tables`.
- No code change required.

### Frame Progression

- Claude correctly noted the JSON frame file is incomplete.
- Existing code follows Excel `Normal_FS_Tables`.
- No code change required.

### Ways Wins Are Stubbed

- Claude correctly noted `evaluateWaysWins()` returns zero.
- Workbook does not provide final paytable values.
- No code change should be made by inventing pay values.

### Additional Validation Statistics

- Claude's suggested counters are useful.
- They are not corrections to confirmed faulty logic.
- No code change required for Task 5.

## Invalid Issues

No consolidated Claude issue was rejected as purely invalid. Several claims are only ambiguous because the Excel and JSON conflict or the Excel omits required parameters.

## Ambiguous Issues / Needs Human Confirmation

### Normal FS WILD Reel Restriction

- What is unclear: JSON says WILD only appears on reels 2 and 3, but Excel `Reelsets_12` gives Normal_FS WILD counts on reels 2, 3, 4, and 5.
- Which source is ambiguous: Excel and `Game_Rules/Free_Spins_3.json` conflict.
- Question to ask: Should the Excel skeleton wild counts be treated as authoritative, or should Normal_FS WILDs be restricted to reels 2 and 3?
- Suggested code approach if confirmed: change `loadTables()` Normal_FS wild count to `(reel == 1 || reel == 2) ? 5 : 0`.

### Girder Super Saw Throw at Six Non-Super-Saw HHs

- What is unclear: probability and add/replace behavior are not defined.
- Which source is ambiguous: `Game_Rules/Girder_Mystery.json` mentions the behavior, but Excel `Base_Tables` has Girder probabilities only for counts 1-5.
- Question to ask: What probability applies at count 6, and does the Super Saw add a seventh HH or replace one of the six?
- Suggested code approach if confirmed: add a separate post-Girder count-6 branch using the supplied probability and placement/replacement rule.

### Normal FS WILD Source Priority

- What is unclear: whether `Game_Rules/` should override Excel for symbol reel restrictions.
- Which source is ambiguous: Task 5 says Excel is primary, while earlier Task 4 source priority put Game Rules first for game flow and symbol behavior.
- Question to ask: For reel-strip counts specifically, should `Reelsets_12` always win?
- Suggested code approach if confirmed: update `loadTables()` and document the source-priority decision in code comments.

### Super Saw FS Overflow Prize Table

- What is unclear: whether Super Saw FS should use the same FS-D overflow table.
- Which source is ambiguous: Excel has no SS-D table.
- Question to ask: Is FS-D shared by both features?
- Suggested code approach if confirmed different: add a state-aware overflow prize helper with a Super Saw table supplied by the math model.

### Super Saw FS Reel Restrictions for HAZARD / H-SAW / V-SAW

- What is unclear: whether Normal FS reel restrictions also apply to Super Saw FS.
- Which source is ambiguous: `Game_Rules/Free_Spins_3.json` names FREE SPINS FEATURE, while Super Saw rules do not repeat the restriction.
- Question to ask: Should Super Saw FS special hard hats also be restricted to reels 1, 3, and 5?
- Suggested code approach if confirmed: extend `requiresOddReelRestriction()` to `SUPER_SAW_FREE_SPINS`.

### Normal FS Retrigger Source Definition

- What is unclear: whether future final feature reel strips may contain organic HH symbols and whether those count as newly landed.
- Which source is ambiguous: Excel says `newly landed/placed HH-type symbols`; current placeholder feature strips have zero HH-on-strip count.
- Question to ask: If final feature reel strips include HH symbols, should organic reel-landed HHs count toward retrigger?
- Suggested code approach if confirmed: count retrigger symbols before and after placement separately according to final design.
