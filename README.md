<div align="center">

# Roguelike (Top‑Down Zelda‑like) – C / SDL2

[![Build Status](https://github.com/ChubbyChuckles/Roguelike/actions/workflows/ci.yml/badge.svg)](../../actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Standard: C11](https://img.shields.io/badge/standard-C11-blue.svg)]()

Clean, **modular**, and **test‑driven** 2D action roguelike foundation written in portable C11.
Focused on deterministic simulation, incremental feature layering, and maintainable pipelines.

<em>“A teaching & experimentation sandbox for loot, combat, and procedural systems.”</em>

</div>

---

## Table of Contents
1. Overview
2. Feature Matrix
3. Loot System (Current Focus)
4. Architecture & Code Layout
5. Build & Run
6. Configuration & Assets
7. Testing & Quality Gates
8. Development Workflow
9. Performance & Determinism
10. Roadmap (Implementation Plan Snapshot)
11. Changelog & Latest Highlights
12. Contributing
13. License
14. Screenshots / Media Placeholders

---

## 1. Overview
This repository provides a layered implementation of a top‑down roguelike / Zelda‑like engine:
* Core game loop, entity & world subsystems.
* SDL2 platform abstraction (optional headless build for CI).
* Progressive loot & rarity system evolving through documented phases.
* Comprehensive unit & integration test coverage for regression safety.

Design goals:
* **Determinism** for tests, reproducibility & telemetry.
* **Cohesive modules** with narrow headers & internal static helpers.
* **Gradual complexity**: each roadmap phase introduces isolated functionality + tests.
* **Tooling first**: formatting, static analysis, telemetry, and debug instrumentation early.

---

## 2. Feature Matrix (Implemented vs Planned)

| Domain | Key Implemented Features | Upcoming / Planned |
|--------|--------------------------|--------------------|
| Core Loop | App lifecycle, frame stepping, player/enemy basics | Advanced AI states, scheduling |
| World | Tilemap load, generation config, vegetation system | Biome theming, dynamic events |
| Combat | Basic damage calc, projectiles, skill system | DOTs, resistances, status effects |
| Loot – Base | Item & loot table parsing, rarity tiers, batching | Global drop rate tuning (9.x) |
| Loot – Advanced | Dynamic rarity weights, pity & floor, affixes, telemetry histogram/JSON, generation context (8.x) | Adaptive balancing, economy & vendors |
| Affixes | Prefix/suffix parsing, weighted selection, stat rolls, persistence | Multi-affix slots, rerolls, crafting |
| Instrumentation | Logging categories, stats window, histogram export | Drift alerts, heatmaps |
| Persistence | Inventory + affix round‑trip | Versioned migrations beyond affixes |
| Tooling | Pre‑commit hooks, formatting, test harness | Asset validation CLI, balancing scripts |

See full, fine‑grained roadmap in `implementation_plan.txt` (kept continuously updated).

### 2.1 Implemented Feature Deep Dive

#### Core Loop & Lifecycle
* Deterministic stepping (`rogue_app_step`) enabling exact state capture in unit tests.
* Central `g_app` registry consolidates pointers (instances, inventory, player) reducing accidental global sprawl.
* Modular update phases (input→simulation→persistence checkpoints→render) keep side effects isolated for easier reasoning.

#### World & Vegetation
* Tilemap sprites cached via atlas coordinates; avoids per-frame lookups.
* Vegetation collision distinguishes trunk (blocking) from canopy (visual) validated by trunk/canopy tests.
* Biome directory structure prepped for per-biome loot modifiers (context already carries biome id).

#### Combat & Skills
* Skill system supports active & passive examples (fireball, dash, passives) with forced short cooldowns under test macro.
* Damage numbers decoupled from combat logic; spawn & lifetime tested for determinism.
* Frame-accurate combo / crit / buffer chain tests ensure animation & timing regressions surface quickly.

#### Loot Foundations (Phases 1–4)
* Item definition parser tolerant to optional later-added rarity column (backward compatibility).
* Loot tables support weighted entries + quantity range (min..max) with deterministic LCG-driven batch roll.
* Ground item pool: fixed-cap static array; O(1) spawn + simple scan for free slot; stack merge radius squared check; no dynamic allocation.
* Persistence round-trip ensures inventory & instances survive reload; affix fields appended later without format breakage.

#### Rarity System (Phase 5)
* Rarity enum centralized; color mapping utility avoids scattering switch statements across UI layers.
* Dynamic rarity weights (5.4) adjust under-performing tiers based on rolling window counts (proportional additive damping to avoid oscillation).
* Per-rarity spawn sound & VFX hook table (5.5) decouples presentation from sampling.
* Despawn overrides (5.6) allow premium tiers to linger longer on ground (checked each update cycle).
* Progressive floor (5.7) from global config + enemy level context (level/10 clamp) ensures progression pacing.
* Pity counter (5.8) separate from RNG distribution: deterministic upgrade trigger after N misses; resets once fired.

#### Instrumentation (Phase 6)
* Logging categories toggled at runtime; high-volume categories suppressible for statistical tests.
* Rolling rarity window stats feed histogram and dynamic weighting.
* Histogram command (6.4) prints aligned count + percentage; stable ordering aids diffing in CI artifacts.
* Telemetry snapshot (6.5) exports JSON-like structure for offline analysis dashboards.

#### Affix Framework (Phase 7)
* Parsing supports duplicate line detection path (currently accepts duplicates; future validation step planned in tooling 21.x).
* Weighted selection per rarity tier: zero weight excludes affix at that tier without extra filtering overhead.
* Value rolling uniform `[min,max]` kept pure; scaled variant (8.4) layered without altering baseline determinism tests.
* Derived damage aggregation picks up flat damage from both prefix & suffix; design anticipates expansion to percent modifiers.
* Persistence: prefix/suffix indices + rolled values serialized; round‑trip test validates binary compatibility.

#### Advanced Generation Pipeline (Phase 8)
* Context object mixes enemy + player factors into seed (distinct large odd multipliers reduce collision risk).
* Multi-pass ordering (rarity→item→affixes) mirrors ARPG pipeline; easier to insert future quality adjustments pre-affix.
* Affix gating rules (initial hard-coded) enforce semantic item/affix compatibility (e.g., damage only on weapons).
* Duplicate avoidance filters candidate set before weight sampling.
* Quality scalar mapping uses diminishing returns curve `luck/(5+luck)` then linear interpolation to global min/max.
* Scaled roll biases upward via exponent approximation avoiding dependency on `powf` (portable for constrained builds).

#### Telemetry & Analytics Hooks
* Rarity histogram + snapshot ready for drift alert phase (18.x) without structural changes.

#### Determinism & RNG
* Single LCG: `state = state * 1664525 + 1013904223` used everywhere; affix and item phases derive local seeds via xor/mix.
* Dedicated deterministic tests ensure identical seeds + contexts reproduce full item (definition + rarity + affixes + values).

#### Memory / Performance
* Static arrays for item defs, affixes, instances; index references instead of pointers ease persistence & future network replication.
* Affix selection uses on-stack arrays bounded by `ROGUE_MAX_AFFIXES`; early break prevents unnecessary iteration.

#### Testing Highlights
* Phase-specific test executables (name pattern `test_loot_phaseX_...`) provide granular failure localization.
* Generation quality test compares low vs high luck contexts ensuring no downward bias under identical seeds.
* Persistence round-trip for affixes guards against format regressions.

#### Logging & Debuggability
* Consistent structured log format includes location for grep-based triage.

#### Intentional Simplifications (Documented)
* Single prefix + suffix slot (multi-slot & rerolling deferred to economy/crafting phases).
* Hard-coded gating rules (will migrate to data-driven rule graph in 8.2+ extended or tooling 21.x).
* Simple additive dynamic weighting (PID / smoothing reserved for 9.x adaptive balancing).

---

---

## 3. Loot System (Current Focus Area)
Recent development concentrated on Phases 5–8 of the roadmap:
* Rarity Enhancements: dynamic weighting, per‑rarity spawn sound hooks, despawn overrides, rarity floor, pity counter.
* Affix Framework: definition parsing, rarity weighting, deterministic stat rolls, instance attachment & persistence.
* Advanced Generation Pipeline (8.x): context (enemy level / biome / archetype / player luck), multi‑pass rarity→base→affixes, seed mixing, affix gating, quality scalar bias, duplicate avoidance.
* Telemetry: rolling rarity window stats, on‑demand histogram print, JSON export snapshot.

These layers enable reproducible progression tuning while preserving deterministic seeds for testability.

---

## 4. Architecture & Code Layout
```
src/
  core/      Game loop, systems (loot_*, combat, persistence, skills)
  entities/  Player & enemy logic
  graphics/  Renderer & sprite handling
  input/     Input mapping & event processing
  world/     Tilemap, procedural world generation, vegetation
  util/      Logging & utility helpers
  platform/  SDL2 platform layer (can be disabled)
tests/
  unit/      Deterministic small-scope tests
  integration/ Boot, world gen, combat & longer flows
assets/      Item, loot, affix, and visual asset configs
cmake/       Toolchain & warning configuration
```

Key module naming conventions:
* `loot_*` – Self‑contained loot subsystems (tables, rarity, affixes, generation, instrumentation).
* `*_adv` – “Advanced” augmentation layer to a baseline system.
* `path_utils` – Centralized path resolution (avoids brittle relative paths in tests).

---

## 5. Build & Run
### Dependencies
* CMake ≥ 3.20
* C11 compiler (MSVC, Clang, GCC)
* (Optional) SDL2 (+ SDL_image, SDL_mixer) for rendering/audio.

### Configure & Build (Examples)
Windows (PowerShell):
```powershell
cmake -S . -B build -DROGUE_ENABLE_SDL=ON -DCMAKE_BUILD_TYPE=Debug -DROGUE_SDL2_ROOT=C:/libs/SDL2
cmake --build build --config Debug
```
Linux / macOS:
```bash
cmake -S . -B build -DROGUE_ENABLE_SDL=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -- -j$(nproc)
```

Headless (logic only):
```bash
cmake -S . -B build -DROGUE_ENABLE_SDL=OFF
cmake --build build
```

Run game (if SDL enabled):
```bash
./build/roguelike   # or build/Release/roguelike.exe on Windows
```

### Tests
```bash
ctest --test-dir build -C Debug --output-on-failure
```
Filter specific tests:
```bash
ctest -C Release -R loot_phase8_generation_quality -V
```

Formatting:
```bash
cmake --build build --target format
```
Static analysis (if clang-tidy found):
```bash
cmake --build build --target tidy
```

---

## 6. Configuration & Assets
Item, loot table, and affix definitions live under `assets/` with CSV‑like schemas. Examples:
* `test_items.cfg` – Base item fields: id, name, category, level req, stack, value, damage/armor, sprite, rarity.
* `test_loot_tables.cfg` – Weighted loot entries + quantity ranges + rarity band overrides.
* `affixes.cfg` – Prefix/suffix definitions with stat ranges and per‑rarity weights.

Runtime path resolution uses `rogue_find_asset_path()` ensuring tests remain portable.

---

## 7. Testing & Quality Gates
Principles:
* **Deterministic RNG**: simple LCG; seeds passed explicitly; reproducibility prioritized.
* **Layered Tests**: each roadmap phase contributes new dedicated test(s).
* **Instrumentation Safety**: histogram / telemetry utilities validated for format & counts.

Quality gates enforced by CI & optional pre‑commit:
* Warnings-as-errors (configurable).
* clang-format compliance.
* clang-tidy (if available) informational target.
* Statistical rarity distribution tests (baseline probability regression guard).
* Persistence round‑trip tests for affix data integrity.

---

## 8. Development Workflow
1. Update or consult `implementation_plan.txt` for next milestone steps.
2. Implement feature in isolated module (`loot_generation.c`, etc.).
3. Add/extend a unit test capturing deterministic behavior and boundary cases.
4. Update roadmap statuses & craft descriptive commit message (stored as `.pending_commit_message.txt`).
5. Run full test suite locally before pushing.

Hooks (optional):
```bash
pip install pre-commit
pre-commit install
```
Environment overrides:
* `ROGUE_PRECOMMIT_COVERAGE=1` – enable extra coverage pass (if lcov available).

---

## 9. Performance & Determinism
* Fixed LCG RNG for item/affix rolls.
* Limited dynamic allocations; most pools are static arrays.
* Affix selection uses small stack arrays; no heap fragmentation during generation.
* Ready for later profiling hooks (see roadmap 17.x).

---

## 10. Roadmap Snapshot (Excerpt)
| Phase | Status | Summary |
|-------|--------|---------|
| 5.x Rarity Enhancements | Done | Dynamic weights, floor, pity, per‑rarity overrides |
| 6.x Instrumentation | Done | Logging categories, histogram & telemetry export |
| 7.x Affix Framework | Done | Parsing, weighted rolls, stat aggregation, persistence |
| 8.x Generation Pipeline | Partially Done | Contextual generation complete (up through duplicate avoidance, quality bias) |
| 9.x Dynamic Balancing | Pending | Adaptive drop rates, global rate layers |
| 10.x Economy & Vendors | Pending | Shop generation, price formulas |

Refer to `implementation_plan.txt` for granularity (over 150 line milestones).

---

## 11. Changelog & Latest Highlights

### [Unreleased]
Planned: Dynamic drop balancing (9.x), economy systems (10.x), crafting (11.x), loot filtering UI (12.x).

### v0.8.x (Current Development Branch)
**Added**
* Advanced loot generation context (enemy level / biome / archetype / player luck).
* Seed mixing & deterministic multi‑pass generation.
* Affix gating rules by item category.
* Quality scalar (luck‑driven) influencing upper stat roll bias.
* Duplicate affix avoidance in prefix/suffix selection.
* Telemetry exports & histogram command.

**Improved**
* Rarity sampling integrates global floor + pity adjustments seamlessly.
* Persistence ensures affix data round‑trips without loss.

**Fixed**
* Path resolution in tests via central `path_utils` abstraction.

Previous milestones summarized within the roadmap file; formal tagged releases forthcoming once 9.x balancing ships.

---

## 12. Contributing
See `CONTRIBUTING.md` for coding standards, review checklist, and branching guidance. External contributions should:
* Include/update unit tests.
* Avoid unrelated formatting churn.
* Provide a succinct commit message (imperative mood, scope prefix e.g. `feat(loot): ...`).

Security / integrity considerations (future): server authoritative mode, roll verification (22.x).

---

## 13. License
Distributed under the MIT License. See `LICENSE` for details.

---

## 14. Screenshots / Media (Placeholders)

| Gameplay | Loot Drop | Telemetry Overlay | Inventory | Histogram Export |
|----------|-----------|------------------|-----------|------------------|
| ![Gameplay WIP](docs/images/placeholder_gameplay.png) | ![Loot Drop](docs/images/placeholder_loot.png) | ![Telemetry](docs/images/placeholder_telemetry.png) | ![Inventory](docs/images/placeholder_inventory.png) | ![Histogram](docs/images/placeholder_histogram.png) |

Additional diagrams planned:
* Generation pipeline flowchart.
* Rarity weighting adjustment graph.
* Affix selection gating decision tree.

---

### Quick Reference Cheatsheet
| Task | Command |
|------|---------|
| Configure Debug (SDL ON) | `cmake -S . -B build -DROGUE_ENABLE_SDL=ON -DCMAKE_BUILD_TYPE=Debug` |
| Build (Win) | `cmake --build build --config Debug` |
| Run Tests | `ctest --test-dir build -C Debug --output-on-failure` |
| Specific Test | `ctest -C Release -R loot_phase8_generation_basic -V` |
| Format Code | `cmake --build build --target format` |
| Static Analysis | `cmake --build build --target tidy` |

---

Happy hacking & may your drops be ever in your favor.

