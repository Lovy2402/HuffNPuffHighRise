# RTP Summary

## Run Settings

- Total spins simulated: 1000
- Bet per spin: 20
- Seed: 123
- Source model: `Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx`
- Source rules: `Game_Rules/*.json`

## RTP

| Component | Credits | RTP |
|---|---:|---:|
| Total bet | 20000 | |
| Total win | 7290 | 36.4500% |
| Base ways wins | 0 | 0.0000% |
| Normal free spins | 7290 | 36.4500% |
| Super Saw free spins | 0 | 0.0000% |
| Overflow prizes (included in feature totals) | 0 | 0.0000% |
| Jackpot prizes (included in feature totals) | 0 | 0.0000% |

Overflow and jackpot rows are subcomponent counters already included in the Normal/Super Saw feature totals.

## Frequencies

| Metric | Count | Frequency |
|---|---:|---:|
| Any hit | 9 | 1 in 111.11 |
| Base hit | 0 | 1 in 0.00 |
| Normal FS trigger | 9 | 1 in 111.11 |
| Super Saw FS trigger | 0 | 1 in 0.00 |
| Any feature trigger | 9 | 1 in 111.11 |
| Girder trigger | 5 | 1 in 200.00 |
| Mystery Stack trigger | 135 | 1 in 7.41 |
| Feature retriggers | 11 | |
| Feature spins played | 65 | |

## Volatility And Limits

- Average feature win: 40.5000x bet
- Maximum win observed: 80.7500x bet
- Max win cap configured: 10000x bet
- Standard deviation estimate: 4.3758x bet

## Validation Counters

- Base reelset usage R1-R4 (R1 is the 3-row base spin; R2-R4 are not re-spun after Hazard expansion): 1000, 0, 0, 0
- Normal FS reelset usage R5-R8: 3, 0, 4, 2
- Super Saw FS reelset usage R9-R12: 0, 0, 0, 0

## Assumptions And TODOs

- TODO: Add final reel strips. Workbook sheet `Reelsets_12` currently provides placeholder strip counts only.
- TODO: Add symbol paytable. The workbook does not provide line/ways pay values, so `evaluateWaysWins()` returns zero.
- Hazard row unlock in free spins reuses Base Table D because no separate free-spin hazard table is supplied.
- Placeholder strip construction uses balanced paying symbols plus workbook wild/HH counts and is not final math validation data.
