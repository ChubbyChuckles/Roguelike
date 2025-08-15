# Dependency Boundary Rules

Layer order (lower may not include higher):

`util` → `platform` → `core` / `world` / `graphics` / `input` → `entities` → `game` (if separated later) → `tests`

Current project merges some concerns; enforcement heuristic:

1. `src/util` must not include headers outside `util` (except the C standard library).
2. No module may include another module's `_internal.h` file (internal headers are private to their directory).
3. Public headers exported to other modules should live directly under their module root (avoid deep nesting of APIs that encourages broad coupling).
4. Cyclic include dependencies are forbidden (audit script performs a simple cycle detection on discovered include graph subset).
5. `platform` may depend on `util` but not on `core`; `core` may depend on `platform` via abstracted interfaces only (current SDL coupling tolerated until abstraction layer added).

Violations are reported by `tools/maint_audit.py` (added Phase M1) and surfaced in CI via `maint_audit` ctest.
