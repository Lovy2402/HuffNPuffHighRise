# Slot Game Development Instructions

## Context
You are assisting in the development of a slot game project.

Repository structure:
- `core/` → authoritative reference implementation
- `efficient_core/` → optimized implementations
- `validation/` → correctness and equivalence testing
- `outputs/` → generated outputs and logs


# Core Rules

## Read-Only Reference Logic
`core/core.cpp` is the authoritative implementation of the game logic.

This file is strictly read-only and must NEVER be modified directly.

You may:
- read it
- analyze it
- compile it
- execute it
- compare against it

You must NOT:
- edit it
- overwrite it
- delete it
- refactor it in-place

All experimental or optimized implementations must be created separately.



# Task 1

Create:

core/core_test.cpp
This file should:
-preserve the exact behavior of core.cpp
-act as a testing/debugging version of the core implementation
-generate deterministic debug outputs useful for validation

The purpose of core_test.cpp is observability and testing, NOT optimization.

## Testing Philosophy

Testing should include both:
- general randomized test cases
- feature-specific deterministic test cases

Feature-specific tests are important for validating bonus mechanics and edge-case behavior.

For feature-specific testing:
- first generate a normal/random pay window
- then manually inject the required feature symbols into the pay window
- force-trigger the corresponding feature logic
- log both the original and modified pay windows
- log symbol injection locations
- log triggered feature behavior and resulting payouts/state transitions

Examples:
- HAT injection for GIRDER testing
- EXPHAT injection for EXPHAT testing

Feature-specific tests should remain clearly separated from generic randomized tests in output logs.

Important edge cases and feature interactions should be documented in:
`core/edge_cases.md`


Output Requirements:

core_test.cpp should generate: outputs/test.txt

The log should include:

-generated pay windows
-symbol layouts
-detected wins
-payout calculations
-bonus triggers
-any important intermediate states useful for debugging

-Prefer readable formatting.

If deterministic seeds are useful, include them.

If important gameplay or implementation edge cases are identified, create:
-core/edge_cases.md

This file should describe:
-the edge case
-why it matters
-expected behavior
-possible failure modes
-suggested validation strategy
-You are encouraged to proactively identify useful edge cases.

## TASK 2

Create an optimized implementation inside efficient_core/.

Goals:
- improve simulation performance for large-scale RTP sweeps
- target workloads up to 1 billion spins
- preserve exact gameplay behavior from core/core.cpp

Optimization ideas may include:
- STL optimization
- reduced allocations
- cache-friendly structures
- avoiding repeated computations
- minimizing branching where appropriate

## Multithreading Rules

Optimized simulations may use multithreading, but thread usage must be limited.

Rules:
- Do not use all available CPU threads by default.
- Default thread usage should be conservative, around 8–10% of available hardware threads.
- Also support an explicit user-provided thread limit, e.g. `--threads 4`.
- Never exceed the user-provided thread limit.
- Keep per-thread RNG/state independent and deterministic when seeds are fixed.
- Avoid shared mutable state in hot simulation loops.
- Aggregate thread-local results at the end.

For development and validation:
- Do NOT actually run 1 billion simulations.
- Use smaller test runs such as 1,000,000 spins.
- Large-scale runs should be configurable but not executed automatically.

## Thread Lifecycle Requirements

If multithreading is used:
- all worker threads must terminate cleanly after execution
- no detached background threads should remain alive
- all threads must be joined safely before program exit
- thread pools must release resources correctly
- avoid persistent idle threads unless explicitly required

## Optimization Documentation

Every optimized implementation inside `efficient_core/` should include or update:

`efficient_core/optimization_config.md`

This file should document:
- optimization assumptions
- benchmark settings
- default spin counts
- thread limits
- RNG policy
- validation strategy
- commands needed to reproduce tests


## TASK 3:

Use the optimized implementation in efficient_core/ to create:

game.cpp

Purpose:
- provide a user-facing executable for the slot game
- support spin-by-spin play
- support RTP simulation/reporting
- support symbol win distribution reporting

Requirements:
1. Do NOT modify core/core.cpp.
2. Do NOT duplicate gameplay logic unnecessarily.
3. Reuse efficient_core implementation as the backend.
4. Preserve exact gameplay behavior validated previously.
5. Keep game.cpp as an interface/runner layer, not a new engine.

game.cpp should support at least two modes:

Mode 1: interactive / spin-by-spin
- user can trigger one spin at a time
- print the pay window
- print wins/payouts/features for that spin
- print running balance/RTP if useful

Mode 2: simulation/report mode
- user can provide number of spins, e.g. --spins 1000000
- compute RTP
- compute total wager, total win, net result
- compute hit counts and win totals by symbol and left-to-right match length

For symbol win distribution, use the same idea as:

HitTable[uniqueSymbols[s]][left2right]++;
WinTable[uniqueSymbols[s]][left2right] += symbol_win;
Look into gameRule/example_symbol_dist.csv to understand what is meant by symbol win distribution. Mode 2 should output
the things in example_symbol_dist.csv.

Important:
- If current code uses assignment instead of accumulation for WinTable, check whether that is intentional.
- For distribution reporting, cumulative win per symbol/length is usually preferred.
- Output should be readable and optionally saved to outputs/.

Also inspect gameRule/ for the example symbol win distribution format.
That example is not specific to this game, but use it as formatting guidance.

If needed, create:
- outputs/rtp_report.txt
- outputs/symbol_win_distribution.txt

Before implementing, first provide:
1. planned CLI interface
2. files to create/modify
3. reporting format
4. how validation will be done


## Constraints
    Do NOT modify core/core.cpp
    Keep generated outputs inside outputs/
    Keep testing/debugging files inside core/
    Create optimized code only inside efficient_core/.
    Validate optimized behavior against outputs/test.txt.
    Prefer minimal and readable changes
    Preserve deterministic behavior whenever possible
    After every major task, update SUMMARY.md.
    After completing a task, stop and wait for user review before any git commit.
    Never automatically commit unless explicitly instructed.


## How to update SUMMARY.md after every task completion

The summary should be concise and operationally useful for starting a fresh Codex session.

Include:
    1. Completed tasks
    2. Important files created/modified
    3. Current repository structure assumptions
    4. Validation status
    5. Important invariants/rules
    6. Known issues or unresolved concerns
    7. Benchmark/optimization status
    8. Recommended next step

Do not write long prose.
Prefer structured bullet points and sections.



## TASK 4

## Objective

Create a new C++ simulation code file for the game using the attached math model Excel file and the attached Game_Rules document as the primary sources of truth.

There is also an existing reference C++ code file under name core.cpp in the folder. That file is provided only for coding style, structure, naming conventions, logging format, simulation flow style, and general implementation reference.

Do not modify, overwrite, delete, or rename the reference code.

Create a completely new C++ source file named:

core_new.cpp

The new file should implement the game simulation logic based on the Excel math model and Game_Rules document.

---

## Source Priority

Use the files in this priority order:

1. Game_Rules folder  
   - Use this for game flow, feature rules, trigger conditions, symbol behavior, win evaluation rules, and player-facing logic.

2. Excel Math Model under name Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx  
   - Use this for reel strips, paytables, probability tables, weights, feature tables, RTP components, trigger odds, values, and mathematical parameters.

3. Reference C++ Code  
   - Use this only for code style and structure.
   - Do not assume its math logic applies to the new game unless it clearly matches the Excel and Game_Rules files.
   - Do not modify the reference code.

If there is a mismatch between the Excel file and Game_Rules file, add a clear comment in `core_new.cpp` explaining the ambiguity and choose the interpretation that is most consistent with the Game_Rules file.

---

## Hard Restrictions

- Do not edit the existing reference code.
- Do not overwrite any existing file.
- Do not remove or rename any existing file.
- Create only the new file `core_new.cpp`.
- Do not invent mechanics that are not present in the Game_Rules or Excel file.
- Do not simplify the simulation unless the simplification is mathematically equivalent.
- Do not hardcode RTP results.
- Do not directly force outcomes to match expected RTP.
- Do not skip feature logic just because an EV value is available, unless the Excel explicitly says to use a direct EV substitution.
- Do not change symbol definitions, reel strips, weights, or paytable values unless directly taken from the Excel file.

---

## Required Implementation Scope

The new `core_new.cpp` file should include a complete simulation of the game, including:

1. Base game spin logic
2. Reel/window generation
3. Symbol evaluation
4. Line wins, ways wins, scatter wins, or other win logic as defined in the files
5. Wild behavior
6. Scatter or bonus trigger logic
7. Free game logic, if present
8. Respin / hold-and-spin / special feature logic, if present
9. Jackpot / prize / credit symbol logic, if present
10. Feature retrigger logic, if present
11. Total win calculation
12. RTP tracking
13. Hit frequency tracking
14. Feature frequency tracking
15. Max win tracking
16. Volatility / standard deviation tracking
17. Segment-wise RTP reporting

---

## Required RTP Breakdown

Track and print RTP separately for every major component available in the game, such as:

- Base game RTP
- Line win RTP
- Scatter win RTP
- Free game RTP
- Respin feature RTP
- Bonus feature RTP
- Jackpot RTP
- Any special feature RTP
- Total RTP

Only include components that actually exist in the Game_Rules or Excel file.

---

## Required Statistics Output

At the end of the simulation, print a clear summary under name RTP_summary.md including:

- Total spins simulated
- Total bet
- Total win
- Total RTP
- Base game RTP
- Feature RTP breakdown
- Hit frequency
- Base game hit frequency
- Feature trigger frequency
- Free game trigger frequency, if applicable
- Respin trigger frequency, if applicable
- Average feature win
- Maximum win observed
- Standard deviation / volatility estimate
- Any other important counters needed to validate the math model

Use a clean and readable console output format similar to the reference code style.

---

## Code Style Requirements

Follow the style of the reference C++ code for:

- File structure
- Function naming style
- Variable naming style
- Random number generation style, unless unsuitable
- Output formatting
- Class/struct usage
- Constants and table declarations
- Simulation loop structure

However, the actual game logic must come from the Excel and Game_Rules files, not from the reference code.

---

## Code Structure Requirements

Organize the code clearly using functions such as:

- `loadTables()` or equivalent, if needed
- `spinBaseGame()`
- `generateWindow()`
- `evaluateBaseGame()`
- `evaluateLineWins()` / `evaluateWaysWins()`, depending on the game
- `checkFeatureTrigger()`
- `playFreeGames()`, if applicable
- `playRespinFeature()`, if applicable
- `evaluateSpecialFeature()`, if applicable
- `updateStatistics()`
- `printResults()`

Use structs/classes where useful, for example:

- `GameConfig`
- `SpinResult`
- `FeatureResult`
- `Statistics`
- `ReelSet`
- `Paytable`
- `Symbol`
- `Window`

Do not over-engineer the code, but make it modular enough for validation and future changes.

---

## Excel Data Handling

Read the Excel file carefully and extract all relevant values manually into the C++ code as constants/tables unless the project already has an Excel-reading framework.

For every major table copied from Excel, add a comment showing:

- Which Excel sheet it came from
- What the table represents
- Any assumptions made while converting it into C++

Example:

```cpp
// Source: Excel sheet "Base_Reels"
// Represents base game reel strips for the default reel set.
If any required table is unclear, missing, or inconsistent, add a TODO comment in core_new.cpp and implement the safest interpretation based on Game_Rules.

Validation Requirements

Where possible, include counters that help validate the simulation against the Excel model, such as:

Trigger counts by feature type
Symbol occurrence counts
Reelset usage counts
Window size counts, if the game has changing windows
Feature entry counts
Feature completion counts
Retrigger counts
Jackpot hit counts
Bonus table selection counts
Any weighted table selection distribution

The goal is to make the simulation easy to verify against the math model.

Random Selection Requirements

For all weighted tables:

Implement weighted random selection correctly.
Use integer weights where possible.
Do not normalize weights unless required.
Ensure zero-weight entries cannot be selected.
Add helper functions for weighted selection.

Example expected helper:

int weightedPick(const std::vector<int>& weights);

or equivalent matching the reference style.

Important Comments

Add comments for all non-obvious math or feature logic.

Especially comment:

Feature trigger conditions
Feature progression
Prize assignment
Retrigger rules
Jackpot rules
Multiplier rules
Caps and max win handling
Any direct EV substitution, if used
Any assumption caused by unclear documentation
Output File

Create the final C++ code in a new file:

core_new.cpp

Do not create or modify any other source code file unless absolutely required for compilation. If additional files are necessary, explain why in comments and do not touch the reference file.

Final Response Requirement

After generating core_new.cpp, provide a short summary listing:

What files were used as source references
What game components were implemented
What assumptions or TODOs remain
How to compile and run the new file
What output/statistics the simulation prints

Do not claim the simulation is fully validated unless it has actually been cross-checked against expected Excel outputs.




## Task 5

##Task: Verify Claude_Review.md Against Excel Math Model and Game Rules

You are reviewing a C++ slot game simulation project.

## Context

The current project contains:

- `core_new.cpp`  
  This is the C++ simulation file previously generated/modified after comparison with the math model and game rules.

- `Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx`  
  This Excel file is the primary math model and source of truth for reelsets, feature logic, weights, tables, credit values, RTP structure, window expansion, and intended game behavior.

- `Game_Rules/`  
  This folder contains the written rules and game flow instructions. Use this as the secondary source of truth after the Excel file.

- `Claude_Review.md`  
  This file contains Claude’s review/analysis and the issues/differences that need to be independently verified.

## Goal

Verify whether the issues, differences, and correction suggestions raised in `Claude_Review.md` are actually correct.

You must compare Claude’s claims against:

1. `Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx`
2. `Game_Rules/`
3. The current implementation in `core_new.cpp`

Do not assume Claude’s review is correct. Treat `Claude_Review.md` as a hypothesis that needs independent verification.

---

## Important Rules

- Treat the Excel math model as the primary source of truth.
- Treat the `Game_Rules/` folder as the secondary source of truth.
- Treat `core_new.cpp` as the current implementation to be reviewed.
- Treat `Claude_Review.md` only as a list of claims to verify, not as the source of truth.
- Do not modify `core_new.cpp`.
- Do not modify the Excel file.
- Do not modify files inside `Game_Rules/`.
- Do not modify `Claude_Review.md`.
- Do not rewrite the full simulation unless absolutely necessary.
- Preserve the existing code style and structure as much as possible.
- Only make code changes if an issue from `Claude_Review.md` is confirmed to be valid.
- If an issue is unclear, ambiguous, or contradicted by the Excel/Game Rules, do not blindly implement it.

---

## Required Review Process

For each issue, difference, or correction suggestion mentioned in `Claude_Review.md`, perform the following:

### 1. Identify Claude’s Claim

Clearly restate the issue Claude raised.

Include:

- What Claude says is wrong or inconsistent
- Which function, section, table, feature, or game behavior Claude is referring to
- Whether Claude suggested a code change
- Whether the claim relates to:
  - Base game
  - Free game
  - Feature trigger
  - Respins
  - Window expansion
  - Credit collection
  - Reelsets
  - Weights/tables
  - RTP reporting
  - Hit frequency reporting
  - Simulation output
  - Any other section

### 2. Validate Against Source of Truth

Check the issue against:

- Relevant Excel sheets/tables/cells
- Relevant files or sections inside `Game_Rules/`
- Relevant code sections in `core_new.cpp`

Determine whether Claude’s issue is:

- Correct
- Incorrect
- Partially correct
- Ambiguous / needs clarification
- Already handled correctly in `core_new.cpp`

### 3. Explain the Verdict

For each issue, explain:

- Whether the issue is valid or not
- Why it is valid or invalid
- Which Excel table/sheet/rule supports the conclusion
- Which code section confirms or contradicts it
- Whether a code change is required

### 4. Apply Changes Only If Needed

If one or more issues from `Claude_Review.md` are confirmed as valid and require code changes:

- Create a new C++ file named:

```text
core_new2.cpp
Base it on the current core_new.cpp
Apply only the necessary changes
Do not remove unrelated code
Do not refactor unrelated sections
Do not change formatting unnecessarily
Add comments only where useful for explaining corrected logic

If all Claude issues are invalid, ambiguous, or do not require code changes:

Still create core_new2.cpp as a copy of core_new.cpp
Do not change behavior
Mention clearly in the reports that no behavioral changes were required
Required Output Files

You must create the following files:

1. core_new2.cpp

This should contain the corrected implementation if valid issues are found.

Rules:

Start from core_new.cpp
Make only verified corrections
Preserve existing structure and style
Do not delete existing functionality
Do not introduce unrelated improvements
Do not alter simulation reporting unless the verified issue specifically requires it
Every behavioral change must be traceable to a verified issue from Claude_Review.md
2. Claude_Review_Verification_Report.md

This file should explain whether the issues in Claude_Review.md are correct or not.

Use this structure:

# Claude Review Verification Report

## Source Files Reviewed

- core_new.cpp
- Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx
- Game_Rules/
- Claude_Review.md

## Executive Summary

Briefly summarize:

- How many issues/differences Claude raised
- How many were valid
- How many were invalid
- How many were partially valid
- How many were ambiguous
- Whether code changes were made in core_new2.cpp
- Whether core_new2.cpp has behavioral changes or is only a copy of core_new.cpp

---

## Issue-by-Issue Verification

### Issue 1: <Issue Name>

#### Claude’s Claim
Explain what Claude claimed in Claude_Review.md.

#### Source of Truth Check
Mention relevant Excel sheets/tables/rules.

#### Code Check
Mention relevant function/logic in core_new.cpp.

#### Verdict
Choose one:

- Valid
- Invalid
- Partially valid
- Ambiguous
- Already correctly implemented

#### Explanation
Explain the reasoning clearly.

#### Required Action
Mention whether code change was needed.

---

Repeat this section for every issue/difference mentioned in Claude_Review.md.

---

## Final Conclusion

Explain whether Claude_Review.md should be accepted fully, partially, or rejected.

Mention:

- Which issues were genuinely valid
- Which issues were not valid
- Which issues need human confirmation
- Whether core_new2.cpp should replace core_new.cpp or only be reviewed as an alternative
3. Claude_Review_Changes_And_Purpose.md

This file should explain every code change made in core_new2.cpp.

Use this structure:

# Changes Made in core_new2.cpp

## Summary

Explain whether code changes were made.

Mention clearly:

- Whether core_new2.cpp contains behavioral changes
- Whether it is only a copy of core_new.cpp
- Which Claude_Review.md issues resulted in code changes

---

## Change 1: <Change Name>

### Related Claude Review Issue
Mention which issue from Claude_Review.md this change addresses.

### Why This Change Was Needed
Explain the mismatch between core_new.cpp and the Excel/Game Rules.

### What Was Changed
Explain the exact logic/code modified.

### Expected Impact
Explain how this affects any relevant area:

- Feature logic
- RTP
- Hit frequency
- Window behavior
- Credit collection
- Respin behavior
- Reelset selection
- Free game behavior
- Base game behavior
- Reporting output
- Any other simulation result

### Risk / Notes
Mention any assumptions or areas requiring further validation.

---

Repeat for each change.

---

## No-Change Items

List any Claude_Review.md issues that did not result in code changes and explain why.
4. Claude_Review_Issue_Clarifications.md

Create this file to separate confirmed issues from unclear or invalid ones.

Use this structure:

# Claude Review Issue Clarifications

## Purpose

This file clarifies whether each issue from Claude_Review.md requires a code fix, explanation only, or no action.

---

## Confirmed Valid Issues

List issues that were valid and required code changes.

For each one, include:

- Issue summary
- Reason it is valid
- Code area affected
- Whether it was fixed in core_new2.cpp

---

## Valid But No Code Change Required

List issues where Claude’s concern is conceptually correct, but the existing code already handles it or no code change is needed.

---

## Invalid Issues

List issues where Claude’s claim is contradicted by the Excel model, Game Rules, or existing code.

For each one, include:

- Claude’s claim
- Why it is incorrect
- Supporting evidence from Excel/Game Rules/code

---

## Ambiguous Issues / Needs Human Confirmation

List issues where the Excel model or Game Rules are unclear.

For each one, include:

- What is unclear
- Which source is ambiguous
- What question should be asked before changing code
- Suggested code approach if the interpretation is confirmed
Additional Validation Requirements

After creating core_new2.cpp, verify that:

It compiles successfully.
It does not break existing simulation flow.
It preserves existing output behavior unless a verified issue requires output changes.
Any new logic is isolated and easy to review.
Any changes are traceable back to a specific issue in Claude_Review.md.
No unverified Claude suggestion has been implemented.
No unrelated refactoring or cleanup has been done.

If compilation is not possible in the environment, state this clearly in the markdown reports.

Final Response Required From Codex

At the end, summarize:

Whether Claude_Review.md was mostly correct, partially correct, or mostly incorrect
Which issues were valid
Which issues were rejected
Which issues were ambiguous
Whether core_new2.cpp contains behavioral changes
Which markdown files were created
Any remaining questions that need human confirmation

Do not make assumptions beyond the Excel file, Game Rules, current code, and Claude_Review.md.