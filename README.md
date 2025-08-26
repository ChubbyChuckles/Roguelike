<div align="center>

# Roguelike (Top‑Down Zelda‑like) – C / SDL2

[![Build Status](https://github.com/ChubbyChuckles/Roguelike/actions/workflows/ci.yml/badge.svg)](../../actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Standard: C11](https://img.shields.io/badge/standard-C11-blue.svg)]()

![alt text](https://github.com/ChubbyChuckles/Roguelike/blob/main/assets/vfx/start_bg.jpg?raw=true)


Clean, **modular**, and **test‑driven** 2D action roguelike foundation written in portable C11.
Focused on deterministic simulation, incremental feature layering, and maintainable pipelines.

<em>“A teaching & experimentation sandbox for loot, combat, procedural generation, progression, and systems design.”</em>

</div>

---

## 0. New Structured Overview
This README has been refactored for fast navigation while preserving every byte of the prior detailed phase logs. A concise, task‑oriented section layout now fronts the document; the full historical/phase narrative appears verbatim in Appendix A ("Full Phase Logs – Original Content"). Nothing was deleted; only reorganized.

### Quick Jump Index
| Section | Purpose |
|---------|---------|
| 1. High‑Level Overview | Elevator pitch & design pillars |
| 2. Feature Matrix | Snapshot of implemented vs planned features |
| 3. Systems Overview | One‑paragraph summaries of each major subsystem |
| 4. Build & Run | Configure, build, test commands |
| 5. Configuration & Assets | Data formats & asset locations |
| 6. Testing & Quality Gates | Determinism, CI gates, fuzz/stat suites |
| 7. Development Workflow | Everyday contributor loop |
| 12. Contributing | Contribution standards |
| 13. License | MIT license reference |
| 14. Media / Screenshots | Placeholder images / planned diagrams |
| 15. Quick Reference Cheatsheet | Frequently used commands |
| Appendix A | Full original README phase logs (complete detail) |

---

## 1. Description
Layered, deterministic top‑down action roguelike engine emphasizing: modular boundaries, reproducible simulation, incremental phase roadmaps, strong test coverage (unit, integration, fuzz, statistical), and data‑driven extensibility (hot reload, schema docs, JSON/CSV/CFG ingest). Systems include loot & rarity, equipment layering (implicits → uniques → sets → runewords → gems → affixes → buffs), dungeon & overworld procedural generation, AI behavior trees + perception + LOD scheduling, skill graph & action economy, persistence with integrity hashing, UI virtualization & accessibility, and emerging economy/crafting pipelines.

### Core Design Pillars
* Determinism first (explicit seeds, reproducible golden snapshots)
* Progressive complexity (phased roadmaps with tests per slice)
* Data before code (config/schema/hot reload/tooling surfaces early)
* Integrity & telemetry (hashes, anomaly detectors, analytics export)
* Maintainability (module boundaries audit, minimal cross‑module coupling)

Note for Windows contributors: prefer ASCII punctuation in docs (e.g., '-' instead of '–') to avoid codepage‑dependent test failures when tests read fixed‑size buffers.

### Testing & Quality Gates (quick)
- Build Debug with SDL2 and run tests in parallel with a per‑test 10s timeout:
	- Build: use CMake multi‑config generators with parallelism (e.g., -j8)
	- Run tests: ctest -C Debug -j8 --timeout 10 --output-on-failure
- Recent stability: configuration version manager now resets its event‑type registry on shutdown to ensure subtest isolation (fixes cross‑phase duplicate registrations in test_config_system_validation).
