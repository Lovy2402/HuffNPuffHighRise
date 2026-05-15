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



## Constraints
    Do NOT modify core/core.cpp
    Keep generated outputs inside outputs/
    Keep testing/debugging files inside core/
    Create optimized code only inside efficient_core/.
    Validate optimized behavior against outputs/test.txt.
    Prefer minimal and readable changes
    Preserve deterministic behavior whenever possible