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
	- Run tests: ctest -C Debug -j8 --timeout 10 --output-on-failure (use -R <regex> for targeted runs)
- Loaders are resilient to working directories: asset/doc paths like `assets/...` are resolved via internal fallbacks so tests can run from build/tests subfolders.

## Start Screen

The start screen includes:
 - Smooth transition to gameplay: Start Screen fades out and, unless reduced-motion is enabled, a brief world fade-in overlay plays to ease the cut. Overlays are cancelled safely on exit.
- Reduced-motion compliance (skips animated fades) and day/night tinting.

 - ROGUE_START_DEV_ESCAPE=1: in developer builds, pressing Esc while in gameplay returns to the Start Screen (for rapid iteration). Disabled by default.
Load overlay basics:
- Opens from Load Game and lists existing slots (up to configured slot count).
- Each entry shows a placeholder thumbnail tinted by a seeded color, plus basic header info.
- Up/Down navigate; Enter loads the selected slot and transitions to gameplay.
 - Delete a slot with a confirmation modal from within the list; deletion uses the save manager and is non-destructive to other slots.
 - Virtualized list: only visible rows render, and the selection is kept in view as you scroll. In headless/test mode the list defaults to slot 0 only for determinism.

Robustness:
- Continue and Load are gated by a descriptor sanity check (version match, non-zero sections, required components present) and re-validate the selected save header immediately before attempting to load. Corrupt or incompatible headers are ignored and will not trigger a transition.
- For deterministic tests, the Start Screen considers slot 0 when deriving Continue visibility and when populating the Load overlay in headless mode.

Environment overrides:
- ROGUE_START_BG: absolute or relative path to a background image.
- ROGUE_REDUCED_MOTION=1: skip fades and animations for accessibility.
 - ROGUE_START_LIST_ALL=1: list all save slots in the Load overlay (default behavior lists slot 0 only in headless/tests to keep snapshots deterministic).
 - ROGUE_START_CONFIRM_NEW=1: require a confirmation modal for New Game (default off; headless auto-accepts).
 - ROGUE_START_BUDGET_MS: override the Start Screen frame-time budget in milliseconds for the early-frame baseline guard (default 1.0). If the guard detects a regression (absolute or +25% relative), optional visuals (spinner/parallax) are suppressed.
	- Note: The relative regression check only applies after baseline sampling completes; setting the threshold negative disables the relative check (useful for perf smoke tests). Tests use a large absolute budget to avoid false positives on shared CI runners.
 - ROGUE_LOG_LEVEL=debug|info|warn|error: control console verbosity.

Credits & Legal overlay:
- Access from the Start screen menu (Credits).
- Tabs: Credits, Licenses, Build. LEFT/RIGHT switches tabs; ESC closes.
- Scroll with UP/DOWN; inertial scrolling with reduced-motion compliance.
- Build tab shows git hash, branch, and build time compiled in via CMake.

Logging (quieter console by default):
- Default log level is WARN. DEBUG/INFO messages are suppressed in normal runs.
- Override with ROGUE_LOG_LEVEL. Accepted values: debug, info, warn, error (or 0..3).
	- PowerShell (Windows): `$env:ROGUE_LOG_LEVEL = 'debug'` then run the app.
	- bash/cmd: `ROGUE_LOG_LEVEL=debug ./roguelike` (bash) or `set ROGUE_LOG_LEVEL=debug && roguelike.exe` (cmd).
	- Reset in PowerShell: `Remove-Item Env:ROGUE_LOG_LEVEL`.
	The app also auto-reads the env on first log, and main() initializes it early.

	Additional noise guards:
	- PNG loader (Windows/WIC) warns once per unique missing/broken asset path to avoid flooding logs during headless tests.

	Persistence robustness:
	- The save system tolerates empty/initial saves (no registered components) by computing CRC/SHA over an empty payload and still writing integrity footers. This enables the initial New Game save path to succeed in minimal test harnesses.
	- Save/Load debug spam has been routed through the central logger at DEBUG level; default WARN keeps the console quiet unless explicitly enabled.

## Audio & VFX (Phases 1–6 Snapshot)

Current capabilities:
- Unified FX bus with deterministic ordering & frame compaction (merged repeat counts) feeding audio (SDL_mixer-backed) and VFX spawning.
- Audio registry with lazy load, per-category + master mixer gains, positional attenuation stub, deterministic variant selection (id suffixed _N), and music category isolation.
- Music system (Phase 6.1–6.4): logical state machine (explore/combat/boss) with per-state track registration, linear cross-fade (configurable ms), bar-aligned deferred transitions (tempo & beats per bar) via `rogue_audio_music_set_tempo` + `rogue_audio_music_set_state_on_next_bar`, side-chain ducking envelope (attack/hold/release), and procedural layering (one deterministic sweetener layer chosen per activation from per-state registry) integrated into effective gain.
- VFX registry & instance pool (lifetime scaling, layers BG→UI, world vs screen space, time scaling/freeze) plus particle emitter pool (variation distributions UNIFORM/NORMAL for scale & lifetime) with composite effects (CHAIN/PARALLEL) and per-instance overrides (scale, lifetime, color).
- Gameplay mapping layer (keys → audio/vfx with priority) wired to damage, skills, buffs, loot triggers.
- Config loader (CSV) with hot-reload watcher & validation error surfacing.
- Deterministic RNG seed override for reproducible particle/audio variant selection.

Recent Phase 6 additions:
- APIs: rogue_audio_music_register, rogue_audio_music_set_state, rogue_audio_music_set_state_on_next_bar, rogue_audio_music_set_tempo, rogue_audio_music_update, rogue_audio_duck_music, rogue_audio_music_current, rogue_audio_music_track_weight.
- Cross-fade weights and duck envelope integrated into rogue_audio_debug_effective_gain for testability; future SDL channel volume automation will hook into these scalars.
- New tests:
	* test_audio_vfx_phase6_1_4_music_system (cross-fade midpoint & completion + duck envelope)
	* test_audio_vfx_phase6_2_beat_aligned (verifies bar-aligned deferred transition & fade timing)

Planned next (remaining Phase 6 / early Phase 7):
- Beat grid & bar/tempo metadata for musical (bar-aligned) transitions (implemented; see APIs above).
- Procedural layering sweeteners (random one-shot accents blended over base loop) with spawn budgeting.
- Screen shake manager & blend modes (Phase 7) to broaden VFX expressiveness.
