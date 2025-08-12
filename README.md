# Roguelike (Zelda-like) 2D Game (C)

Clean, modular C codebase scaffold for a top‑down Zelda‑like game. Enforces separation of concerns, code quality, and regression safety via tests & tooling.

## Architecture
```
src/
  core/        Lifecycle & game loop
  platform/    (SDL2) window/events wrappers
  graphics/    Renderer abstraction (future)
  input/       Input state & mapping
  entities/    Entity & player logic
  world/       Tile map data & loading
  math/        Small math helpers
  util/        Logging & common utilities
tests/
  unit/        Pure logic tests
  integration/ Boot & system smoke tests
cmake/         Build helpers
```

## Build (Windows PowerShell example)
```powershell
cmake -S . -B build -DROGUE_ENABLE_SDL=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Run tests:
```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Format sources:
```powershell
cmake --build build --target format
```

Static analysis placeholder:
```powershell
cmake --build build --target tidy
```

## SDL2
Provide SDL2 dev libraries (e.g. set SDL2_DIR or SDL2_ROOT). Disable with `-DROGUE_ENABLE_SDL=OFF` for headless logic-only builds (unit tests still run).

## Game Loop Control
* `rogue_app_run()` runs until exit requested
* `rogue_app_step()` advances exactly one frame (useful in deterministic tests)

## Quality Gates
* Warnings → errors (except third-party)
* clang-format & clang-tidy targets (auto-detected)
* Unit + integration tests via CTest
* Optional pre-commit hook enforces formatting + green tests locally

## Pre-commit Hooks
Install Python (3.8+) & pre-commit, then install hooks:
```powershell
pip install pre-commit
pre-commit install
```
Ensure CMake is installed and available on PATH (on Windows you can set `$env:CMAKE_EXE` to the full path to cmake.exe if not on PATH).
On each commit the following run:
* Formatting / whitespace & YAML sanity
* clang-format (must be clean)
* Fast build (Debug, SDL OFF) + all tests

Manual hooks:
```powershell
pre-commit run commit-msg-lint -c .pre-commit-config.yaml
pre-commit run clang-tidy-optional --all-files
```
Optional full coverage check (if lcov present, mostly non-Windows) can be run manually:
```powershell
pre-commit run coverage-optional --all-files
```
Or automatically on commits by exporting:
```powershell
$env:ROGUE_PRECOMMIT_COVERAGE=1
```

## Next Steps (Suggested)
1. Tile rendering (atlas + camera)
2. Player movement & collision system
3. Enemy AI (finite state machine)
4. Asset pipeline (maps, sprites metadata)
5. Save / load (serialize game state)
6. Hash-based frame capture tests (optional determinism guard)
7. CI add SDL-enabled job (currently headless) & artifact builds

## Contribution Guidelines
See `CONTRIBUTING.md` for workflow & review checklist.
