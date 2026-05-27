# Technical Review: Huff N' Puff Highrise Simulator

## C++ Code vs Game Rules (JSON) vs Excel Math Model

**Review Date:** 2026-05-26  
**Source Files:** `core.cpp`, `Game\_Rules/\*.json`, `Huff\_N\_Puff\_Highrise\_Math\_Model\_12\_Reelsets.xlsx`  
**Reviewer Role:** Slot Math / Simulation Validation  
**Action Status:** Analysis only. No code changes made. All corrections require approval.

\---

## A. Executive Summary

### Overall Verdict

The C++ simulator is **structurally well-formed** and faithfully implements large portions of the game logic, including the Mystery Stack, Girder trigger probabilities, Hazard row expansion, frame upgrade chain (None → Straw → Stick → Brick), Brick overflow priority rules, house prize tables, and the retrigger/spin-count logic. However, **five critical bugs and three high-severity issues** mean the simulation output cannot be trusted for RTP validation in its current state.

### Biggest Mismatches

|#|Issue|Severity|
|-|-|-|
|1|`applySawPath` does not skip BRICK positions — triggers overflow rule when a saw touches an already-Brick cell, which is forbidden by Free\_Spins\_2.json|**CRITICAL**|
|2|Normal FS reel strips assign WILDs to all reels 2–5 (0-indexed 1–4); rules allow WILDs only on reels 2–3 (1-indexed)|**CRITICAL**|
|3|Pre-feature saw execution is absent — SAW symbols in the trigger window should fire their saw paths *before* spin 1, as stated in Free\_Spins\_1.json and the Excel SS setup note|**CRITICAL**|
|4|HAZARD HH placement has no reel restriction in base game or Normal FS — rules require reels 1, 3, 5 (1-indexed) only|**CRITICAL**|
|5|H-SAW / V-SAW placement has no reel restriction in Normal FS — rules require reels 1, 3, 5 only|**CRITICAL**|
|6|Girder "Super Saw throw" for exactly 6 non-Super-Saw HHs on screen is not implemented|**HIGH**|
|7|RTP breakdown table double-counts overflow and jackpot wins (they are already included inside the reported feature win totals)|**HIGH**|
|8|Base game reelset usage stats will always read R1=N, R2=0, R3=0, R4=0 — misleading for validation sign-off|**HIGH**|

### Highest-Risk Items

Issues 1–5 directly distort the frame-building logic and the reel composition of the most valuable feature. Together they make all feature RTP figures incorrect. **The simulation cannot be used to validate feature RTP, average feature win, jackpot exposure, or saw contribution until these are fixed.**

### Can the Current Output Be Trusted?

* **Base game hit-frequency figures:** Partially. The placement trigger, count, type and position weights match the Excel exactly. However, the HAZARD HH reel restriction is missing, which slightly skews Hazard-unlock frequency.
* **Feature RTP:** No. Issues 1–5 corrupt both the feature frame-building logic and the reel composition.
* **Overall RTP:** No. The ways paytable is a stub (returns 0), and feature RTP is wrong.
* **Trigger frequency (Normal FS / Super Saw FS):** Partially usable as a first-pass check, but the missing Girder Super Saw throw (Issue 6) suppresses Super Saw FS odds slightly.

\---

## B. Game Flow Summary

### Intended Flow (Game Rules + Excel)

**Base Spin:**

1. Select base reelset for current active row count (starts at 3).
2. Spin reels; populate full 6-row window.
3. Apply Mystery Stack (Tables I–L) before HH placement.
4. Roll HH placement trigger (Table A); if triggered, roll count (B), type (C per active rows), position (G, H).
5. Resolve HAZARD HH expansion (Table D); active rows increase and are capped at 6.
6. If HH count is 1–5 (excluding Super Saw HH per Girder\_Mystery.json), roll Girder trigger (Table E); fill to 6 using Table F symbols.
7. If 6 non-Super-Saw HHs are on screen, an additional Super Saw throw may occur (Girder\_Mystery.json).
8. If total HH count ≥ 6: trigger Normal FS (or Super Saw FS if any Super Saw HH present).
9. Award base-ways wins (paytable, not yet supplied).

**Free Spins Feature (Normal FS or Super Saw FS):**

1. Initialize each trigger position as STRAW FRAME.
2. Execute saw paths for any SAW symbols in the trigger window *before* spin 1.
3. Award 6 starting spins.
4. Each spin: select reelset by state + active rows; spin; place HH-type symbols (Tables FS-A/B/C or SS-A/B/C); resolve Hazard expansion before saws; upgrade frames; execute saw paths last.
5. Retrigger: 3+ HH-type symbols (Normal FS) or 3+ Saw/Hazard symbols (Super Saw FS) → +1 spin.
6. At feature end: each framed position becomes a House; award prizes per Tables P-A/B/C.

### Actual Flow in C++ Code

The code follows the above flow closely **except:**

* Pre-feature saw execution (step 2) is absent.
* Saw paths hit BRICK positions instead of skipping them.
* HAZARD/SAW placement is not reel-restricted.
* Normal FS reel strips have WILDs on all reels 2–5 instead of only 2–3.
* The Girder Super Saw throw at count=6 is not implemented.
* Ways wins always return 0.

\---

## C. C++ Code Structure Summary

|Function|Purpose|
|-|-|
|`loadTables()`|Constructs 12 placeholder reel sets from the Excel skeleton (wild counts, HH-on-strip counts per reel).|
|`buildPlaceholderStrip()`|Fills a reel strip with WILDs, optional HH, then balanced paying symbols.|
|`findReelSet()`|Selects the correct reelset for a given state + active-row combination.|
|`generateWindow()`|Picks a random stop and reads 6 consecutive positions from each reel into the window.|
|`applyMysteryStack()`|Rolls Tables I–L and overwrites random reel positions with mystery symbols.|
|`placeHardHats()`|Rolls Tables A/B/C/G/H (base) or FS-A/B/C or SS-A/B/C (feature) and places HH symbols.|
|`resolveHazardExpansion()`|Checks window for HAZARD\_HH; if found and rows < 6, rolls Table D unlock count.|
|`applyGirder()`|Rolls Table E probability; fills missing positions to reach 6 using Table F symbols.|
|`playFreeGames()`|Main feature loop: initializes frames, runs spins, handles retriggers, collects overflow wins.|
|`applyFrameHit()`|Implements None→Straw→Stick→Brick upgrade chain and Brick overflow priority.|
|`applySawPath()`|Propagates horizontal/vertical/super saw effect to neighbouring cells.|
|`awardHousePrize()`|Draws prize from P-A/B/C tables; flags jackpot wins.|
|`awardOverflowPrize()`|Draws from Table FS-D; called when all active positions are BRICK.|
|`spinBaseGame()`|Orchestrates one base-game spin including feature trigger.|
|`updateStatistics()`|Accumulates win components and squared wins for variance estimation.|
|`evaluateWaysWins()`|**Stub. Returns 0.** Placeholder for the missing paytable.|
|`printResults()`|Outputs RTP breakdown, frequencies, and reelset usage to stdout and file.|

\---

## D. Rule Comparison Table — C++ vs Game Rules (JSON)

|#|Rule / Logic Area|Expected (Game\_Rules JSON)|Actual (C++ Code)|Match Status|Impact|Suggested Correction|Affected Code|
|-|-|-|-|-|-|-|-|
|D1|Saw path — BRICK cell handling|Free\_Spins\_2.json: saws only affect "positions **without** BRICK FRAME"|`applySawPath` calls `applyFrameHit` on ALL touched cells, including BRICK, which triggers the overflow rule|**INCORRECT**|Inflates overflow awards; misattributes wins; distorts feature RTP|Add `if (frames\[r]\[c] == BRICK\_FRAME) return;` guard inside the `hit` lambda in `applySawPath`|`applySawPath`, `hit` lambda|
|D2|Pre-feature saw execution|Free\_Spins\_1.json: "Before the FREE SPINS FEATURE begins…every SAW HARD HAT symbol…generates a saw"|`playFreeGames` only assigns STRAW\_FRAME to trigger positions; no saw paths are executed before spin 1|**MISSING**|Under-counts initial frame upgrades; lowers feature EV, especially in Super Saw FS where trigger screen is saw-dense|After initialising trigger STRAW\_FRAMEs, iterate over trigger positions; for each SAW symbol call `applySawPath`|`playFreeGames` initialisation block|
|D3|HAZARD HH reel restriction — base game|BaseGame\_1.json: "HAZARD HARD HAT symbol only appears on reels 1, 3, and 5 during the base game"|`placeHardHats` uses `weightedReel()` which picks from all 5 reels|**MISSING**|HAZARD can land on reels 2 or 4 in base game; skews Hazard-unlock frequency and symbol distribution|After resolving type: if placed symbol is HAZARD\_HH and state is BASE\_GAME, restrict reel picks to {0,2,4}|`placeHardHats`, reel-selection logic|
|D4|HAZARD / H-SAW / V-SAW reel restriction — Normal FS|Free\_Spins\_3.json: "HAZARD HARD HAT, HORIZONTAL SAW HARD HAT, VERTICAL SAW HARD HAT only appears on reels 1, 3, and 5"|No reel filter applied during placement in `placeHardHats` for Normal FS|**MISSING**|These symbols can appear on even-numbered reels; alters saw-path coverage and Hazard-unlock distribution|For Normal FS, after symbol type is resolved: if type is HAZARD, HSAW, or VSAW, restrict reel pick to {0,2,4}|`placeHardHats`, reel-selection logic|
|D5|H-SAW / V-SAW — base game reel restriction|BaseGame\_2.json: "HORIZONTAL SAW HARD HAT, VERTICAL SAW HARD HAT do not appear on reel 4 during the base game, except via placement"|No restriction; placement can target any reel for any symbol type|**NOTE**|Per "except via placement" wording, placement to reel 4 is explicitly allowed; code is technically not wrong. Confirm intent|Confirm whether "except via placement" makes the restriction moot. If confirmed, no change needed|`placeHardHats`|
|D6|WILD in Normal FS — reel restriction|Free\_Spins\_3.json: "WILD symbol only appears on reels 2 and 3"|`buildPlaceholderStrip` assigns 5 WILDs to every reel except reel 1 (0-indexed: reels 1–4 all get WILDs)|**INCORRECT**|Reels 4 and 5 (1-indexed) should have 0 WILDs in Normal FS. Inflates WILD hit rate; distorts feature RTP significantly|In `loadTables` for `NORMAL\_FREE\_SPINS`: set `wild\_count = 0` for reels other than 1 and 2 (0-indexed)|`loadTables`, `buildPlaceholderStrip`|
|D7|Overflow prize when all cells are BRICK|Free\_Game\_1.json: "Otherwise, **10x total bet** is awarded"|`awardOverflowPrize()` uses weighted Table FS-D (5x–100x)|**Discrepancy between JSON and Excel**|Code follows Excel FS-D (correct as the primary math source). JSON appears simplified. Flag for game-team confirmation|Confirm which is authoritative. If Excel, no code change needed; update JSON for consistency|`awardOverflowPrize`|
|D8|Girder — Super Saw throw at HH count = 6|Girder\_Mystery.json: "if 5 **or 6** scatters except SUPER SAW HARD HAT land, a SUPER SAW HARD HAT may be thrown onto the reels"|`rollGirderTrigger` returns false when `hard\_hat\_count >= 6`, so no Super Saw throw occurs when 6 non-Super-Saw HHs are present|**MISSING**|When 6 non-Super-Saw HHs land naturally, there should be a separate probability to convert Normal FS to Super Saw FS. Slightly under-represents Super Saw FS frequency|Add a separate post-trigger check: if count=6 and no Super Saw present, roll probability (undefined in Excel — needs table) to add a Super Saw HH|`spinBaseGame`, post-Girder block|
|D9|Frame progression — NONE to BRICK via FreeGame\_Frames.json|FreeGame\_Frames.json table shows only NONE→BRICK and BRICK→BRICK (2 entries)|Code correctly implements the 4-step progression: None→Straw→Stick→Brick|**JSON Incomplete vs Excel**|Code matches the Excel (4-step chain), which is the correct truth. JSON is a simplified extract|No code change needed. Flag JSON as incomplete for documentation alignment|`applyFrameHit`|
|D10|Stick House max prize|House\_Prizes.json: "1.5x – 4.5x total bet"|Code implements 1.5x–7.5x per Excel Table P-B (7.5x at weight 500)|**Discrepancy between JSON and Excel**|Code correctly includes the 7.5x tier. JSON appears to omit it|Confirm 7.5x is intended. If yes, update JSON for consistency; no code change needed|`awardHousePrize`, Stick branch|
|D11|Brick House max credit prize|House\_Prizes.json: "7.5x – 150x total bet"|Code implements 100x as the highest credit prize, matching Excel Table P-C|**Discrepancy between JSON and Excel**|JSON lists 150x but Excel max credit is 100x. Code follows Excel|Confirm max credit. If 100x, no code change needed; update JSON|`awardHousePrize`, Brick branch|
|D12|"Minix2" jackpot tier|House\_Prizes.json lists "Mini, Minix2, Minor, Major, Grand"|Code has Mini, Minor, Major, Grand per Excel (no Minix2)|**Missing in Excel and code**|If Minix2 is a real prize tier, it is absent from both the Excel and the code|Clarify with game design whether Minix2 exists. If yes, add to Prize\_Tables and code|`awardHousePrize`, Brick branch|
|D13|Normal FS retrigger condition|Free\_Spins\_2.json: "3 or more SCATTER symbols land on active rows"|Code counts all HH symbols in the window after placement — includes both reel-landed and placed symbols|**Partially Matches**|"Land" could mean reel-generated only. Including placed symbols slightly inflates retrigger frequency|Clarify whether placed HHs count for retrigger. If reel-only: count HHs before calling `placeHardHats`|`playFreeGames` spin loop|
|D14|Super Saw FS retrigger condition|Excel SS setup: "3+ Saw/Hazard symbols"|Code counts: `isSawHat(s) \|\| s == HAZARD\_HARD\_HAT`|**Matches**|—|None|`playFreeGames` spin loop|

\---

## E. Math Model Comparison Table — C++ vs Excel

|#|Excel Sheet / Table|Expected (Excel)|Actual (C++)|Match Status|Impact|Suggested Correction|Affected Code|
|-|-|-|-|-|-|-|-|
|E1|Reelsets\_12 — WILD count per reel, Normal FS|Reels 1,3,4,5 (0-indexed): R5 reel1=5, reel2=5, reel3=5, reel4=5 wilds|Code sets wild=5 for all reels except reel 0 (all reelsets regardless of state)|**INCORRECT for Normal FS**|WILDs on wrong reels for Normal FS (reels 3 and 4 should be 0); over-represents WILD frequency; inflates feature RTP|In `loadTables` for NORMAL\_FREE\_SPINS: `wild\_count = (reel == 1 \|\| reel == 2) ? 5 : 0`|`loadTables`|
|E2|Reelsets\_12 — WILD count per reel, Base Game|R1–R4: Reel1=0, Reels2–5=3 wilds|Code: `wild\_count = reel==0 ? 0 : 3`|**Matches**|—|None|`loadTables`|
|E3|Reelsets\_12 — WILD count, Super Saw FS|R9–R12: Reel1=0, Reels2–5=6 wilds|Code: `wild\_count = reel==0 ? 0 : 6`|**Matches**|—|None|`loadTables`|
|E4|Reelsets\_12 — HH-on-strip count, Base|Reel1=1, Reel2=0, Reel3=1, Reel4=0, Reel5=1 (same for R1–R4)|Code: `hh\_count = (reel==0\|\|reel==2\|\|reel==4) ? 1 : 0`|**Matches**|—|None|`loadTables`|
|E5|Reelsets\_12 — HH-on-strip count, Normal FS / Super Saw FS|All reels: 0|Code: `hh\_count = 0` for both feature states|**Matches**|—|None|`loadTables`|
|E6|Base\_Tables A — Placement trigger|No: 7200, Yes: 2800|`{{0, 7200}, {1, 2800}}`|**Matches**|—|None|`placeHardHats`|
|E7|Base\_Tables B — HH count|1:4200, 2:2800, 3:1700, 4:900, 5:350, 6:50|`{{1,4200},{2,2800},{3,1700},{4,900},{5,350},{6,50}}`|**Matches**|—|None|`placeHardHats`|
|E8|Base\_Tables C — HH type by active rows|3-row total=10000; 6-row Hazard=0|`baseHardHatType`: weights match exactly; 6-row Hazard weight=0|**Matches**|—|None|`baseHardHatType`|
|E9|Base\_Tables D — Hazard row unlock|+1:7800, +2:1900, +3:300|`{{1,7800},{2,1900},{3,300}}`|**Matches**|—|None|`resolveHazardExpansion`|
|E10|Base\_Tables E — Girder trigger chance|1HH:0.05%, 2HH:0.15%, 3HH:0.75%, 4HH:3%, 5HH:18%|`if (hard\_hat\_count == 1) return p < 0.0005` etc.|**Matches**|—|None|`rollGirderTrigger`|
|E11|Base\_Tables F — Girder completion symbol|Normal:8500, Hazard:700, HSAW:300, VSAW:300, SSAW:200|`girderCompletionSymbol`: same weights|**Matches**|—|None|`girderCompletionSymbol`|
|E12|Base\_Tables G — Reel position weights|Reel1:2200, Reel2:1600, Reel3:2600, Reel4:1600, Reel5:2000|`weightedReel`: same values|**Matches**|—|None|`weightedReel`|
|E13|Base\_Tables H — Row position weights|4 active-row configurations with correct totals of 10000|`weightedRow`: 0-indexed mapping verified correct for all 4 cases|**Matches**|—|None|`weightedRow`|
|E14|Base\_Tables I — Mystery Stack trigger|No:8800, Yes:1200|`{{0,8800},{1,1200}}`|**Matches**|—|None|`applyMysteryStack`|
|E15|Base\_Tables J — Mystery Stack reels affected|1:6000, 2:3000, 3:900, 4:100|`{{1,6000},{2,3000},{3,900},{4,100}}`|**Matches**|—|None|`applyMysteryStack`|
|E16|Base\_Tables K — Mystery Stack symbol|Pig1:2500, Pig2:2500, Pig3:2000, Toolbox:1800, Tape:1200|`mysteryStackSymbol`: same weights|**Matches**|—|None|`mysteryStackSymbol`|
|E17|Base\_Tables L — Mystery Stack size|2:5000, 3:3000, 4:1500, Full:500|`{{2,5000},{3,3000},{4,1500},{active\_rows,500}}`|**Matches**|—|None|`applyMysteryStack`|
|E18|Normal\_FS\_Tables FS-A — Placement trigger|No:4500, Yes:5500|Same weights|**Matches**|—|None|`placeHardHats`|
|E19|Normal\_FS\_Tables FS-B — HH count|1:4000, 2:3000, 3:1700, 4:900, 5:300, 6:100|Same weights|**Matches**|—|None|`placeHardHats`|
|E20|Normal\_FS\_Tables FS-C — HH type|Normal:6800, Hazard:1000, HSAW:1100, VSAW:1100; No SSAW|`featureHardHatType(NORMAL\_FREE\_SPINS)`: same weights, no SSAW|**Matches**|—|None|`featureHardHatType`|
|E21|Normal\_FS\_Tables FS-D — Overflow prize|5x:4500, 10x:3000, 15x:1600, 25x:700, 50x:180, 100x:20|`awardOverflowPrize`: same weights and values|**Matches**|—|None|`awardOverflowPrize`|
|E22|SuperSaw\_FS\_Tables SS-A — Placement trigger|No:2500, Yes:7500|Same weights|**Matches**|—|None|`placeHardHats`|
|E23|SuperSaw\_FS\_Tables SS-B — HH count|1:2500, 2:3000, 3:2300, 4:1400, 5:600, 6:200|Same weights|**Matches**|—|None|`placeHardHats`|
|E24|SuperSaw\_FS\_Tables SS-C — HH type|Normal:4200, Hazard:700, HSAW:2500, VSAW:2500, SSAW:100|`featureHardHatType(SUPER\_SAW\_FREE\_SPINS)`: same weights|**Matches**|—|None|`featureHardHatType`|
|E25|Prize\_Tables P-A — Straw House|0.5x:5000, 0.75x:3000, 1x:2000|Same weights and values|**Matches**|—|None|`awardHousePrize`|
|E26|Prize\_Tables P-B — Stick House|1.5x:3500, 2x:2500, 3x:2000, 4.5x:1500, 7.5x:500|Same weights and values|**Matches**|—|None|`awardHousePrize`|
|E27|Prize\_Tables P-C — Brick House|10 prize tiers; Jackpots: Mini=25x, Minor=100x, Major=500x, Grand=5000x|Same weights and values; `idx >= 6` flags jackpot wins|**Matches**|—|None|`awardHousePrize`|
|E28|RTP\_Targets — Target game RTP|96%|N/A (target, not a hardcoded value)|**N/A**|—|—|—|
|E29|RTP\_Targets — Normal FS odds|1 in 140|Measurable from simulation output|**Measurable**|—|Validate after fixes|`printResults`|
|E30|RTP\_Targets — Super Saw FS odds|1 in 900|Measurable from simulation output|**Measurable**|—|Validate after fixes|`printResults`|
|E31|RTP\_Targets — Max win cap|10,000x bet|`max\_win\_xbet = 10000`, `max\_win\_credits = 10000 \* 20 = 200000`|**Matches**|—|None|Global constants|
|E32|Execution\_Logic — Mystery Stack before HH placement|Base 3 → Base 4 ordering|`applyMysteryStack` called before `placeHardHats`|**Matches**|—|None|`spinBaseGame`|
|E33|Execution\_Logic — Hazard resolved before saws|Feature 3 ordering|`resolveHazardExpansion` called before `hardHatPositions` scan|**Matches**|—|None|`playFreeGames`|
|E34|Execution\_Logic — Saws after all frame updates|Feature 4 ordering|Two separate loops: frame-update first, saw-path second|**Matches**|—|None|`playFreeGames` spin loop|
|E35|Execution\_Logic — Retrigger before decrement|Feature 5 ordering|Retrigger check increments `remaining\_spins` then `remaining\_spins--`|**Matches**|—|None|`playFreeGames`|
|E36|Execution\_Logic — Super Saw behavior|Same row + same column; landing cell upgrades once only|`applySawPath`: excludes own row/col intersection via `if (c != reel)` / `if (r != row)` guards|**Matches**|—|None|`applySawPath`|
|E37|Execution\_Logic — Duplicate placement retry|Retry up to 20 times; cancel if duplicate persists|`for (int attempt = 0; attempt < 20 \&\& !placed; attempt++)`|**Matches**|—|None|`placeHardHats`|
|E38|Reelsets — Base game reelset selection|"Select BaseReelset\[activeRows]" (steps Base 1–2)|`spinBaseGame` always selects BASE\_GAME with `active\_rows=3`; reelsets R2–R4 are never selected during base game|**Partially Matches**|R2–R4 base reelsets are unused; base reelset usage stats always show R1=N, R2–R4=0. However the base spin always starts at 3 rows so this is correct behaviour; the printout is misleading|Add a comment and note in printResults clarifying that R2–R4 are intentionally unused in base game|`spinBaseGame`, `printResults`|
|E39|No overflow table defined for Super Saw FS|Excel provides FS-D only; no SS-D table exists|Code uses the same `awardOverflowPrize()` (FS-D) for both features|**Partially Matches / Gap**|Super Saw FS overflow values may differ from Normal FS if a SS-D table is ever added|Confirm whether SS-D should differ from FS-D. If same, document explicitly|`awardOverflowPrize`|

\---

## F. Detailed Mismatch Report

\---

### Issue F-1

**Severity:** CRITICAL  
**Source of truth:** Game\_Rules (Free\_Spins\_2.json), Excel (Execution\_Logic saw behaviour)  
**Area:** Saw path hitting BRICK frame cells

**Expected behaviour:** Free\_Spins\_2.json says "Each position **without BRICK FRAME** that a saw touches… will be evaluated as if HARD HAT symbol had landed there." Positions that already hold BRICK FRAME must be skipped entirely by the saw.

**Current code behaviour:** `applySawPath` calls `applyFrameHit(frames, r, c, active\_rows)` for every cell the saw traverses, including cells that are already BRICK\_FRAME. `applyFrameHit` on a BRICK cell runs the overflow priority chain (upgrade random Stick→Brick, or add Straw to frameless cell, or award a prize), inflating overflow awards.

**Mathematical impact:** Every saw event over a BRICK cell incorrectly triggers an overflow action or prize. In late-spin states where many cells are Brick, multiple spurious overflow prizes can be awarded per saw sweep. This inflates both overflow RTP and average feature win.

**Suggested correction:** Inside the `hit` lambda in `applySawPath`, add an early return if the frame is BRICK:

```cpp
auto hit = \[\&](int r, int c) {
    if (frames\[r]\[c] == BRICK\_FRAME) return;  // Rule: saws skip BRICK positions
    int idx = r \* no\_of\_reels + c;
    if (seen\[idx]) return;
    seen\[idx] = 1;
    win += applyFrameHit(frames, r, c, active\_rows);
};
```

**Approval required before coding:** Yes

\---

### Issue F-2

**Severity:** CRITICAL  
**Source of truth:** Game\_Rules (Free\_Spins\_3.json)  
**Area:** Wild symbol reel restriction — Normal Free Spins

**Expected behaviour:** "WILD symbol only appears on reels 2 and 3" (1-indexed) during Normal FS. Reels 1, 4, and 5 should have no WILDs.

**Current code behaviour:** `loadTables()` for `NORMAL\_FREE\_SPINS` sets `wild\_count = reel == 0 ? 0 : 5`. This gives WILDs to 0-indexed reels 1, 2, 3, and 4, which corresponds to 1-indexed reels 2, 3, 4, and 5. Reels 4 and 5 (1-indexed) incorrectly receive 5 WILDs each.

**Mathematical impact:** WILDs on reels 4 and 5 in Normal FS significantly increase ways-win frequency during feature spins. When the paytable stub is replaced, this will produce inflated base-ways RTP during free spins. Also distorts the Normal FS symbol distribution relative to the model.

**Suggested correction:** In `loadTables` for `NORMAL\_FREE\_SPINS`:

```cpp
wild\_count = (reel == 1 || reel == 2) ? 5 : 0;
```

**Approval required before coding:** Yes

\---

### Issue F-3

**Severity:** CRITICAL  
**Source of truth:** Game\_Rules (Free\_Spins\_1.json), Excel (SuperSaw\_FS\_Tables setup note)  
**Area:** Pre-feature saw execution

**Expected behaviour:** Free\_Spins\_1.json: "Before the FREE SPINS FEATURE begins… every HORIZONTAL SAW HARD HAT symbol in active rows generates a saw." Excel SS setup: "Triggering HH positions: Straw Frames + optional saw path. Recommended: execute saw paths before spin 1."

**Current code behaviour:** `playFreeGames` assigns STRAW\_FRAME to each trigger position and then immediately enters the spin loop. No saw paths are executed on the trigger symbols before spin 1.

**Mathematical impact:** Any SAW symbol in the trigger window provides zero saw benefit. In Super Saw FS — where Super Saw HH is the trigger — this means the entire cross-shaped saw effect of the trigger symbol is lost. This substantially underestimates average Super Saw FS win.

**Suggested correction:** After the STRAW\_FRAME initialisation block in `playFreeGames`, add a pre-spin saw execution pass:

```cpp
// Execute saw paths for any SAW symbols in the trigger window before spin 1
for (const auto\& pos : trigger\_positions) {
    Symbol s = /\* need to pass original window or symbol type with positions \*/;
    if (isSawHat(s)) {
        result.overflow\_win += applySawPath(frames, pos.first, pos.second, active\_rows, s);
    }
}
```

Note: `trigger\_positions` currently only stores `{row, col}` pairs. The function signature must also pass the symbol at each trigger position (or the full trigger window).

**Approval required before coding:** Yes

\---

### Issue F-4

**Severity:** CRITICAL  
**Source of truth:** Game\_Rules (BaseGame\_1.json)  
**Area:** HAZARD HH reel restriction — base game

**Expected behaviour:** "HAZARD HARD HAT symbol only appears on reels 1, 3, and 5 during the base game." (1-indexed = 0-indexed reels 0, 2, 4)

**Current code behaviour:** `placeHardHats` for BASE\_GAME calls `weightedReel()` which returns any of reels 0–4 for any symbol type. HAZARD HH can be placed on reels 1 or 3 (0-indexed = 1-indexed reels 2 and 4), violating the restriction.

**Mathematical impact:** HAZARD HH placed on unrestricted reels slightly over-represents Hazard-unlock events. It also potentially distorts H-SAW / V-SAW paths (if HAZARD is inadvertently treated as a saw via `isSawHat`). Minor effect on base game, but contributes to model inaccuracy.

**Suggested correction:** In `placeHardHats`, after the symbol type is selected, check and re-restrict the reel:

```cpp
if (state == BASE\_GAME \&\& placed\_symbol == HAZARD\_HARD\_HAT) {
    // Reels 1, 3, 5 (1-indexed) = 0-indexed: 0, 2, 4
    static const int hazard\_reels\[] = {0, 2, 4};
    reel = hazard\_reels\[getRandom(0, 2)];  // Or use a weighted pick from G restricted to {0,2,4}
}
```

(Apply proportionally-weighted pick among {0,2,4} from Table G if exact frequency preservation is needed.)

**Approval required before coding:** Yes

\---

### Issue F-5

**Severity:** CRITICAL  
**Source of truth:** Game\_Rules (Free\_Spins\_3.json)  
**Area:** HAZARD / H-SAW / V-SAW reel restriction — Normal FS

**Expected behaviour:** "HORIZONTAL SAW HARD HAT, VERTICAL SAW HARD HAT symbols and HAZARD HARD HAT symbol only appears on reels 1, 3, and 5" during Normal FS.

**Current code behaviour:** `placeHardHats` for NORMAL\_FREE\_SPINS applies no reel filtering for these symbol types.

**Mathematical impact:** These symbols can appear on reels 2 and 4 (1-indexed) in Normal FS. HAZARD on wrong reels distorts expansion rates; H-SAW / V-SAW on wrong reels produces incorrect saw paths (H-SAW on reel 2 generates a different row sweep than if restricted to reels 1, 3, 5). This materially affects feature EV and frame upgrade distributions.

**Suggested correction:** Same approach as F-4. After symbol type selection in `placeHardHats` for NORMAL\_FREE\_SPINS: if symbol is HAZARD, HSAW, or VSAW, restrict reel to {0, 2, 4}.

**Approval required before coding:** Yes

\---

### Issue F-6

**Severity:** HIGH  
**Source of truth:** Game\_Rules (Girder\_Mystery.json)  
**Area:** Girder Super Saw throw for count = 6 non-Super-Saw HHs

**Expected behaviour:** "During the base game if 5 **or 6** scatters except SUPER SAW HARD HAT land, a SUPER SAW HARD HAT may be thrown onto the reels."

**Current code behaviour:** `rollGirderTrigger` returns false when `hard\_hat\_count >= 6`, so the throw never occurs when exactly 6 non-Super-Saw HHs land naturally (count = 6, Girder disabled, no Super Saw in window → Normal FS triggered without Super Saw conversion chance).

**Mathematical impact:** Some fraction of Natural 6-HH trigger events that should convert to Super Saw FS remain as Normal FS. Under-represents Super Saw FS odds and average win. The probability and mechanism for this throw are not specified in the Excel (no probability table provided), so this may be intentionally deferred.

**Suggested correction:** After the Girder block in `spinBaseGame`, add: if `trigger\_count >= 6 \&\& !hasSuperSawHat(window, active\_rows)`, roll a probability (needs to be defined in the Excel or rules) to add a Super Saw HH to a random non-HH active position.

**Approval required before coding:** Yes (and requires probability value from game design)

\---

### Issue F-7

**Severity:** HIGH  
**Source of truth:** Internal consistency — reporting only  
**Area:** RTP breakdown double-counts overflow and jackpot

**Expected behaviour:** The printed RTP breakdown should sum to the total RTP without double-counting. Overflow and jackpot wins are sub-components of the feature win, not additive on top of it.

**Current code behaviour:**

* `stats.normal\_fs\_win += feature.total\_win` — `feature.total\_win` includes overflow and jackpot wins.
* `stats.overflow\_win += result.overflow\_win` — same overflow added again separately.
* `stats.jackpot\_win += result.jackpot\_win` — same jackpot added again separately.
* In `printResults`, all four components (Normal FS, Super Saw FS, Overflow, Jackpot) are shown with their own RTP rows, implying they are additive. Summing them produces a number larger than Total RTP.

**Mathematical impact:** Zero effect on the `total\_win` figure used for RTP calculation. However, any analyst reading the breakdown table will believe the sum of components equals the total — it does not. This will cause incorrect RTP attribution and potentially cause regulatory/certification review failures.

**Suggested correction:** Either:
(a) Track overflow and jackpot as sub-components that are already inside feature\_win, and annotate the table clearly.
(b) Maintain separate `overflow\_win` and `jackpot\_win` counters but SUBTRACT them from `normal\_fs\_win` / `super\_saw\_fs\_win` before printing, so the rows sum to total.

**Approval required before coding:** Yes

\---

### Issue F-8

**Severity:** HIGH  
**Source of truth:** Internal consistency — validation reporting  
**Area:** Base game reelset usage stats are misleading

**Expected behaviour:** The RTP summary claims to show "Base reelset usage R1–R4", implying all four base reelsets are used. A reviewer who sees R2=R3=R4=0 after 1M spins will investigate unnecessarily.

**Current code behaviour:** `stats.reelset\_usage\_base\[active\_rows - 3]++` is called with `active\_rows = 3` always (base game always spins with 3-row reelset); R2–R4 are structurally correct to be zero but the output makes it appear like a bug.

**Mathematical impact:** None. This is a validation reporting issue only.

**Suggested correction:** Rename the output label to "Base reelset usage (3-row reelset = R1; R2–R4 unused by design)" and add a note that expansion occurs on the existing window post-spin, not by re-spinning with a wider reelset.

**Approval required before coding:** Yes

\---

### Issue F-9

**Severity:** MEDIUM  
**Source of truth:** Game\_Rules (Free\_Game\_1.json) vs Excel (Normal\_FS\_Tables FS-D)  
**Area:** Overflow prize value — JSON says 10x, Excel FS-D gives a variable table

**Expected behaviour (JSON):** "Otherwise, 10x total bet is awarded."  
**Expected behaviour (Excel):** Table FS-D, weighted 5x–100x.

**Current code behaviour:** `awardOverflowPrize()` uses Table FS-D. This matches the Excel.

**Mathematical impact:** If the JSON were the authority, the average overflow prize would be 10x. The Excel EV for FS-D = 5×0.45 + 10×0.30 + 15×0.16 + 25×0.07 + 50×0.018 + 100×0.002 = 2.25+3.0+2.4+1.75+0.9+0.2 = **10.5x** average. Very close to the JSON "10x" — the JSON is likely a rounded EV. Code is correct per Excel.

**Suggested correction:** Clarify in the JSON that 10x is the approximate EV of FS-D, not a fixed prize. No code change needed.

\---

### Issue F-10

**Severity:** MEDIUM  
**Source of truth:** Game\_Rules (House\_Prizes.json)  
**Area:** "Minix2" jackpot tier

**Expected behaviour (JSON):** "Mini, **Minix2**, Minor, Major or Grand" — implies a second mini tier between Mini and Minor.  
**Current code behaviour:** No Minix2 in code or Excel.

**Mathematical impact:** If this is a real prize tier, jackpot EV and frequency figures are wrong. Probability high that JSON contains a transcription error.

**Suggested correction:** Confirm with game design. If Minix2 is a real tier, add it to both the Excel Prize\_Tables sheet and the `awardHousePrize` function.

\---

### Issue F-11

**Severity:** LOW  
**Source of truth:** Code (known TODO)  
**Area:** Base game / feature ways paytable missing

**Expected behaviour:** `evaluateWaysWins` should return ways-win credits based on window symbols.  
**Current code behaviour:** Always returns 0.

**Mathematical impact:** Base game RTP component = 0% (should be \~32.64% per RTP target). Feature ways wins also = 0 (should contribute in feature). Total RTP is therefore only the feature house prizes + overflow (correct for simulation of the feature-prize system only). Ways wins are the largest single RTP component per the model.

**Suggested correction:** Once symbol paytable values are supplied, implement the ways-win evaluator using standard all-ways evaluation (count matching symbols per reel, multiply across).

\---

## G. Missing Statistics / Validation Outputs

### What the Code Currently Reports

* Total bet, total win, total RTP
* Base ways win (always 0 due to stub)
* Normal FS win, Super Saw FS win (with double-counted overlap noted above)
* Overflow win, jackpot win (double-counted vs feature wins)
* Hit frequency, base hit frequency
* Normal FS trigger frequency, Super Saw FS trigger frequency
* Girder trigger count
* Mystery Stack trigger count
* Retrigger count, total feature spins
* Average feature win x-bet
* Maximum observed win, standard deviation
* Reelset usage counters

### What Is Missing and Why It Matters

|Missing Output|Why It Matters|Target from Excel|
|-|-|-|
|Average Normal FS win (x-bet) separately printed|Needed to validate against target 35x–55x|35–55x|
|Average Super Saw FS win (x-bet) separately printed|Needed to validate against target 90x–160x|90–160x|
|Separate ways-win RTP during free spins|Feature spins should also award ways wins; needed to separate frame/prize RTP from ways-win RTP|\~8.5% of 96% RTP from Free Spins|
|Per-active-row breakdown of Normal FS / Super Saw FS spins played|Validates that row expansion is correctly weighted|N/A (first-pass validation)|
|Average number of frames at feature end (by tier)|Needed to validate house prize EV against P-A/P-B/P-C tables|P-A avg ≈ 0.65x, P-B avg ≈ 2.6x, P-C avg ≈ 22.6x|
|Overflow prize count and average value|Validates FS-D table calibration|Average ≈ 10.5x|
|Jackpot hit frequency per tier (Mini/Minor/Major/Grand)|Required for regulatory jackpot reporting|Mini: 1 in 55,556 Brick houses; Grand: 1 in 2,000,000 Brick houses|
|Feature win distribution histogram (win buckets)|Needed to validate high-end volatility and max-win exposure|Max win 10,000x cap|
|Mystery Stack — hits per component (Pig1, Pig2, etc.)|Validates Table K weights|Per-symbol probability|
|Retrigger frequency as a fraction of feature entries|Validates spin-length distribution|Low (feature triggers on average 3+ HHs per feature spin)|
|Girder — average symbols added per trigger|Validates completion-symbol composition and Super Saw addition rate|Avg ≈ 1 symbol × Table F|
|HAZARD unlock frequency per trigger (1/2/3 rows)|Validates Table D; important for active-rows distribution|+1:78%, +2:19%, +3:3%|

\---

## H. Correction Plan Without Code Changes

### 1\. Must-Fix Before Trusting Any RTP Figure

|Priority|Issue|Why Critical|
|-|-|-|
|H1.1|Saw path must skip BRICK cells (F-1)|Inflates overflow awards on every saw sweep over a mature board|
|H1.2|WILD count fix for Normal FS reel strips (F-2, E1)|Over-represents WILDs in most valuable feature; will corrupt ways wins when paytable added|
|H1.3|Pre-feature saw execution before spin 1 (F-3)|Super Saw FS EV is significantly underestimated without initial saw paths|
|H1.4|HAZARD HH reel restriction in base game (F-4)|Needed for model accuracy per BaseGame\_1.json|
|H1.5|HAZARD / H-SAW / V-SAW reel restriction in Normal FS (F-5)|Needed for model accuracy per Free\_Spins\_3.json|
|H1.6|Fix double-counted overflow/jackpot in RTP breakdown (F-7)|Reports are unusable for certification until breakdown sums are coherent|

### 2\. Should-Fix for Better Validation

|Priority|Issue|Why Important|
|-|-|-|
|H2.1|Add average Normal FS and Super Saw FS win to printResults (G)|Required to verify against RTP\_Targets (35–55x / 90–160x)|
|H2.2|Add per-frame-tier count at feature end (G)|Required to verify P-A/P-B/P-C table calibration|
|H2.3|Add jackpot hit frequency per tier (G)|Required for regulatory sign-off|
|H2.4|Add win distribution histogram (G)|Required for volatility validation|
|H2.5|Clarify base reelset usage printout (F-8)|Avoids false alarm during code review|
|H2.6|Implement Girder Super Saw throw for count=6 (F-6)|Once probability is defined by game design|

### 3\. Optional Improvements

|Priority|Issue|Notes|
|-|-|-|
|H3.1|Separate overflow tracking from feature win totals (F-7 option b)|Makes breakdown table strictly additive; low risk change|
|H3.2|Confirm "Minix2" jackpot tier (F-10)|Low probability of being a real tier, but should be confirmed|
|H3.3|Confirm single vs. variable overflow table for Super Saw FS (E39)|Add SS-D table if game design determines different overflow profile for Super Saw|
|H3.4|Retrigger counting: land-only vs. land+placed (D13)|Clarify game design intent; small quantitative impact|
|H3.5|Add per-active-row reelset usage tracking inside feature loop (E38)|Better validation of row-expansion distribution during feature|

\---

## I. Questions and Ambiguities

The following items in the rules or Excel require clarification from the game design or math team before the corresponding code changes can be finalised.

|#|Source|Question|Impact if Unresolved|
|-|-|-|-|
|I-1|Free\_Game\_1.json|"Otherwise, 10x total bet is awarded" — is this a fixed prize or the approximate EV of FS-D? Code uses FS-D (correct per Excel). JSON should be updated to match.|Minor documentation; code appears correct|
|I-2|Girder\_Mystery.json|What is the probability of throwing a Super Saw HH when exactly 6 non-Super-Saw HHs are on screen? No table in Excel covers this case.|Cannot implement F-6 without this value|
|I-3|Girder\_Mystery.json|When the Super Saw throw occurs for count=6, is the Super Saw added as a 7th symbol or does it replace one of the 6?|Affects total HH count and saw path count|
|I-4|BaseGame\_2.json|"HORIZONTAL SAW HARD HAT, VERTICAL SAW HARD HAT do not appear on reel 4 during the base game, **except via placement**." Does this mean placement to reel 4 for these symbols IS allowed with no restriction?|If yes, no code change needed for H-SAW/V-SAW in base game|
|I-5|Free\_Spins\_3.json|"WILD symbol only appears on reels 2 and 3" — is this 1-indexed? If so, 0-indexed reels 1 and 2. Confirm reel numbering convention used throughout.|Affects F-2 / E1 correction|
|I-6|House\_Prizes.json|What is "Minix2"? Is it a second tier of Mini jackpot between Mini (25x) and Minor (100x)?|If real, affects jackpot EV and frequency|
|I-7|House\_Prizes.json|Brick House listed with max credit prize of 150x, but Excel Table P-C shows 100x. Which is correct?|Prize table discrepancy|
|I-8|House\_Prizes.json|Stick House max listed as 4.5x in JSON, but Excel Table P-B includes 7.5x. Is 7.5x intended?|Code correctly uses 7.5x per Excel|
|I-9|Excel (no SS-D table)|Should Super Saw FS overflow prizes use a different distribution than Normal FS (FS-D)?|If different table needed, must be added to Excel and code|
|I-10|Excel / JSON|Is `evaluateWaysWins` intended to return the same ways-wins as base game during free spins (per Free\_Spins\_3.json)? Or are free-spin ways wins suppressed / handled differently?|Affects feature RTP structure|
|I-11|Free\_Spins\_1.json|Pre-feature saw execution: should the OVERFLOW prize also be possible from the initial saw paths (i.e., if initial saws hit BRICK positions — which should be impossible since all positions start as STRAW — can overflow occur)? Theoretically impossible at feature start but should be confirmed.|Minor edge case|
|I-12|General|The retrigger rule for Normal FS says "3+ newly landed/placed HH-type symbols." Does "newly" exclude HH symbols that appeared on the reel strip organically (not placed)? If so, organic HH from reel strips should NOT count toward retrigger.|Minor effect; current code counts all HH in window|
|I-13|SuperSaw\_FS\_Tables|Are HAZARD / H-SAW / V-SAW reel-restricted to reels 1, 3, 5 in Super Saw FS (as they are in Normal FS), or are they unrestricted? The Super Saw FS rules do not explicitly state a reel restriction.|If restricted, same fix as F-5 should apply to Super Saw FS|

\---

*End of Technical Review. All suggested corrections are pending approval. No code changes have been made.*

