# Copilot Instructions (Project Policy)

These directives govern all AI-generated contributions. Treat them as mandatory acceptance criteria.

## Core Principles
1. Correctness first: produce compiling, warning-free C11 code (warnings treated as errors).
2. 100% Line Coverage: all lines in `src/` must be executed by tests. Defensive/error branches must be testable; refactor to inject fakes if needed. CI fails below 100%.
3. Modularity: one responsibility per file; prefer small focused functions (< ~60 LOC).
4. Determinism: expose state via accessors (e.g., frame counters) instead of hidden globals.
5. Refactoring: if adding feature to an overgrown file, first extract cohesive modules before implementing feature logic.

## Mandatory Workflow for Any Change
1. Identify scope & list affected modules.
2. If a file grows beyond ~300 LOC or a function beyond ~60 LOC, refactor BEFORE adding new logic.
3. Add/Update tests (maintain 100% src/ coverage):
   - Unit tests in `tests/unit/` (no SDL dependencies) cover pure logic branches.
   - Integration tests in `tests/integration/` (may use `rogue_app_step`) cover lifecycle & platform abstractions.
4. Run (or simulate) `format-check`, `clang-tidy`, and `ctest` steps; ensure green.
5. Update documentation (README or inline comments) when introducing new public APIs.
 6. Ensure pre-commit hooks pass (install via `pre-commit install`).

## Coding Standards
* C11 only; avoid non-portable extensions.
* Headers self-contained: each must compile alone.
* No unchecked dynamic allocations—verify return value and handle failure path.
* No magic numbers: use `#define` or `enum` constants for clarity.
* Keep interfaces narrow: pass explicit context structs instead of global state.

## Logging & Errors
* Use `ROGUE_LOG_ERROR/WARN/INFO/DEBUG`; never printf directly in engine code.
* Functions returning bool: true = success, false = failure (always log on failure path).

## Performance & Memory
* Favor stack allocation for small, short-lived objects.
* Zero-initialize or explicitly set all struct fields.
* Document any micro-optimizations with justification.

## Renderer & Platform Layer
* Keep SDL-specific code isolated; grow abstraction in `graphics/`.
* Provide accessors instead of exposing raw SDL pointers once stable.

## Input System
* New input logic must route through internal enums (no stray SDL scancodes elsewhere).

## Tile/World/Entities
* Pure functions for coordinate/data transformations (improves testability).
* Separate update and draw passes (no rendering in update functions).

## Adding a New Feature (Checklist)
[] Describe feature & affected modules in commit/PR description.
[] Create/update interface header(s) with concise comments.
[] Implement logic in new or well-scoped existing module.
[] Add unit tests (happy path + edge/failure) so all new/changed lines execute.
[] Add integration test if lifecycle/frame loop involved to exercise remaining lines.
[] Verify `tools/coverage_check.sh` passes (100%).
[] Run format-check (no diffs expected).
[] Ensure clang-tidy reports no new warnings.
[] All tests pass locally.
[] Update README or docs if externally visible API changes.
[] Pre-commit hooks (format + tests) pass.

## Prohibited Patterns
* Monolithic files mixing unrelated concerns.
* Silent failures (always log with context).
* Unbounded busy-wait loops lacking TODO about timing.
* Hidden state mutations via implicit singletons.

## Refactoring Guidance
* Preserve observable behavior (tests guide changes).
* Commit extraction separately from new features when feasible.
* Introduce small adapter layers to stage larger subsystem rewrites.

## Commit & PR Messaging
* Imperative mood: "Add player movement system".
* Reference tests added/updated: e.g. "Adds test_player_movement.c (unit) and test_player_boot_move.c (integration)."

---
Following these instructions is required for AI-assisted contributions. Deviations need justification plus added tests.
