# RTP Summary

## Run Settings

- Total spins simulated: 1000
- Bet per spin: 20
- Seed: 275
- Source model: `Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx`
- Source rules: `Game_Rules/*.json`

## RTP

| Component | Credits | RTP |
|---|---:|---:|
| Total bet | 20000 | |
| Total win | 63484 | 317.4200% |
| Base ways wins | 35004 | 175.0200% |
| Normal free spins | 22855 | 114.2750% |
| Super Saw free spins | 5625 | 28.1250% |
| Overflow prizes (included in feature totals) | 0 | 0.0000% |
| Jackpot prizes (included in feature totals) | 10000 | 50.0000% |

Overflow and jackpot rows are subcomponent counters already included in the Normal/Super Saw feature totals.

## Frequencies

| Metric | Count | Frequency |
|---|---:|---:|
| Any hit | 301 | 1 in 3.32 |
| Base hit | 294 | 1 in 3.40 |
| Normal FS trigger | 11 | 1 in 90.91 |
| Super Saw FS trigger | 1 | 1 in 1000.00 |
| Any feature trigger | 12 | 1 in 83.33 |
| Girder trigger | 6 | 1 in 166.67 |
| Mystery Stack trigger | 128 | 1 in 7.81 |
| Feature retriggers | 13 | |
| Feature spins played | 85 | |

## Volatility And Limits

- Average feature win: 118.6667x bet
- Maximum win observed: 559.0000x bet
- Max win cap configured: 10000x bet
- Standard deviation estimate: 23.7656x bet

## Validation Counters

- Base reelset usage R1-R4 (R1 is the 3-row base spin; R2-R4 are not re-spun after Hazard expansion): 1000, 0, 0, 0
- Normal FS reelset usage R5-R8: 3, 0, 5, 3
- Super Saw FS reelset usage R9-R12: 0, 0, 1, 0

## Assumptions And TODOs

- TODO: Add final reel strips. Workbook sheet `Reelsets_12` currently provides placeholder strip counts only.
- Ways wins use the paytable and all-ways symbol-pay logic from `core/core.cpp`.
- Hazard row unlock in free spins reuses Base Table D because no separate free-spin hazard table is supplied.
- Placeholder strip construction uses balanced paying symbols plus workbook wild/HH counts and is not final math validation data.
