# Huff n Puff High Rise Slot Project Summary

## Repo Roles

- `core/`: authoritative reference and debug/testing harnesses.
- `efficient_core/`: optimized RTP-sweep implementation and optimization documentation.
- `validation/`: validation tooling for equivalence checks.
- `outputs/`: generated logs, summaries, and benchmark outputs.

## Critical Invariants

- Do not modify `core/core.cpp` unless explicitly instructed. It is the reference logic.
- Keep generated outputs under `outputs/`.
- Keep optimized implementations under `efficient_core/`.
- Keep validation scripts/tools under `validation/`.
- Preserve gameplay order:
  1. Generate 6x5 pay window.
  2. Apply EXPHAT active-row adjustment.
  3. Calculate ways win.
  4. Build active pay window.
  5. Evaluate/apply GIRDER if hats are active.
  6. Recount hats and determine free-game trigger.
- HAT/EXPHAT/HORHAT/VERTHAT count for bonus state but are not paying ways symbols.
- WILD substitutes for paying symbols but is not seeded as its own paying symbol.
- Ways wins are calculated before GIRDER modifies the active window.
- Fixed seeds should produce deterministic results in single-thread mode.

## Completed Task 1

Artifacts:

- `core/core_test.cpp`: deterministic debug/testing clone of the core logic.
- `core/edge_cases.md`: documented gameplay and implementation edge cases.
- `outputs/test.txt`: generated debug log.

`core/core_test.cpp` currently:

- Uses deterministic seed `123456789`.
- Runs 25 general randomized spins.
- Logs pay windows, active layouts, RNG draws, detected wins, payout calculations, hat counts, bonus triggers, final summary, hit table, and win table.
- Includes clearly separated forced feature tests:
  - GIRDER via injected HAT.
  - EXPHAT active-row expansion via injected EXPHAT.
  - HAT on first active reel / non-paying ways seed behavior.

Useful command:

```bash
g++ -std=c++17 -Wall -Wextra core/core_test.cpp -o /tmp/core_test
/tmp/core_test
```

## Task 2 Current State

Artifacts:

- `efficient_core/efficient_core.cpp`: optimized simulator.
- `efficient_core/optimization_config.md`: optimization assumptions, decisions, threading, RNG policy, validation, and commands.
- `validation/validate_efficient_core.py`: deterministic validation script.
- `outputs/efficient_summary.txt`: 25-spin optimized validation output.
- `outputs/efficient_benchmark.txt`: 1,000,000-spin development benchmark output.
- `outputs/efficient_thread_smoke.txt`: small multithread smoke output.

Optimization choices:

- Replaced hot-loop `vector<vector<Symbol>>` with fixed-size `std::array` windows.
- Removed per-reel `sliceReels()` temporary allocations.
- Avoided active-window copying except for mutable GIRDER feature logic.
- Replaced position-vector symbol scans with direct count/boolean scans.
- Stored reel strips and paytable as compile-time constants.
- Kept RNG distribution APIs conservative to preserve probability behavior.
- Kept debug logging out of the optimized hot path.
- Implemented simple optional threading with thread-local aggregates and joined worker threads.

Threading assumptions:

- Single-thread mode is the deterministic validation mode.
- Multithread mode is deterministic for the same seed, spin count, and thread count.
- Multithread mode is not expected to be spin-by-spin identical to single-thread mode because RNG streams are partitioned by worker.
- `--threads N` is an explicit cap; workers are joined before exit; no detached threads are used.

## Validation Strategy

Primary deterministic validation:

```bash
g++ -std=c++17 -Wall -Wextra core/core_test.cpp -o /tmp/core_test
/tmp/core_test
g++ -O3 -std=c++17 -Wall -Wextra efficient_core/efficient_core.cpp -o /tmp/efficient_core
python3 validation/validate_efficient_core.py
```

Expected validation result:

```text
validation passed: efficient_core summary and feature markers match outputs/test.txt
```

Manual optimized run:

```bash
/tmp/efficient_core --spins 25 --seed 123456789 --single-thread --feature-tests --output outputs/efficient_summary.txt
```

Development benchmark only, not a large production sweep:

```bash
/tmp/efficient_core --benchmark --seed 123456789 --threads 1 --output outputs/efficient_benchmark.txt
```

Thread smoke test:

```bash
/tmp/efficient_core --spins 1000 --seed 123456789 --threads 2 --output outputs/efficient_thread_smoke.txt
```

## Notes For Fresh Sessions

- Start by reading `AGENTS.md`, `core/core.cpp`, `core/core_test.cpp`, `efficient_core/optimization_config.md`, and this file.
- If core behavior changes, update `core/core_test.cpp` first, regenerate `outputs/test.txt`, then update/validate `efficient_core/efficient_core.cpp`.
- Use `outputs/test.txt` as the observable behavior reference for deterministic tests.
- Do not run extremely large simulations automatically. Use 1,000,000 spins only for development benchmarking.
