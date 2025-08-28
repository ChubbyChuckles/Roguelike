<div align="center">

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

## Audio & VFX (Phases 1–7 Snapshot)

Current capabilities:
- Unified FX bus with deterministic ordering & frame compaction (merged repeat counts) feeding audio (SDL_mixer-backed) and VFX spawning.
- Audio registry with lazy load, per-category + master mixer gains, positional attenuation stub, deterministic variant selection (id suffixed _N), and music category isolation.
- Music system (Phase 6.1–6.6): logical state machine (explore/combat/boss) with per-state track registration, linear cross-fade (configurable ms), bar-aligned deferred transitions (`rogue_audio_music_set_tempo`, `rogue_audio_music_set_state_on_next_bar`), side-chain ducking envelope, procedural layering (deterministic sweetener selection), environmental reverb preset stubs (`rogue_audio_env_set_reverb_preset` smoothing wet mix), and distance-based low-pass attenuation (`rogue_audio_enable_distance_lowpass`, `rogue_audio_set_lowpass_params`).
- VFX registry & instance pool (lifetime scaling, layers BG→UI, world vs screen space, time scaling/freeze) plus particle emitter pool (variation distributions UNIFORM/NORMAL for scale & lifetime) with composite effects (CHAIN/PARALLEL) and per-instance overrides (scale, lifetime, color).
- Gameplay mapping layer (keys → audio/vfx with priority) wired to damage, skills, buffs, loot triggers.
- Config loader (CSV) with hot-reload watcher & validation error surfacing.
- Deterministic RNG seed override for reproducible particle/audio variant selection.

Recent additions (Phase 6–7):
- APIs: rogue_audio_music_register, rogue_audio_music_set_state, rogue_audio_music_set_state_on_next_bar, rogue_audio_music_set_tempo, rogue_audio_music_update, rogue_audio_duck_music, rogue_audio_music_current, rogue_audio_music_track_weight.
- Cross-fade weights and duck envelope integrated into rogue_audio_debug_effective_gain for testability; future SDL channel volume automation will hook into these scalars.
- New tests:
	* test_audio_vfx_phase6_1_4_music_system (cross-fade midpoint & completion + duck envelope)
	* test_audio_vfx_phase6_2_beat_aligned (verifies bar-aligned deferred transition & fade timing)

Phase 7 (now expanded):
- Blend modes (registry only): RogueVfxBlend enum (ALPHA default; ADD, MULTIPLY) with set/get APIs; renderer will map to GPU states later.
- Screen shake manager: pooled shakes (amplitude, frequency, duration) aggregated into a composite camera offset each frame with decay, deterministic for tests.
- Performance scaling: global 0..1 emission multiplier (rogue_vfx_set_perf_scale / get) applied to particle emission accumulator for adaptive density.
- GPU batch flag stub: enable/disable + query hook to gate future batched sprite/particle path (no batching logic yet).
- Trails: per‑VFX trail emitters (trail_hz, trail_life_ms, trail_max) with per‑instance accumulators; particles flagged as trails for metrics; respects perf scaling.
- Post‑processing stubs: global bloom enable + threshold/intensity params; color grade LUT id + strength; public getters/setters with clamping; renderer hookup pending.
- Decals: registry + instance pool with lifetime aging; spawn with pos/angle/scale; per‑layer counts and screen‑space collection helper.
Test coverage: Phase 6 and 7 tests validate cross‑fade/ducking/layering, emission math, trails/post/decals behaviors.

Phase 8 (performance & budgeting):
- Per-frame VFX stats snapshot and profiler API: `rogue_vfx_profiler_get_last(RogueVfxFrameStats*)` with counters for spawned_core/trail, culled_{pacing,soft,hard}, and active pools.
- Spawn control: `rogue_vfx_set_pacing_guard(enable, threshold_per_frame)` runs before `rogue_vfx_set_spawn_budgets(soft, hard)` each frame; culled counts are attributed to the stage that clamps.
- Pool audits: `rogue_vfx_particle_pool_audit` and `rogue_vfx_instance_pool_audit` expose active/free and simple run metrics for fragmentation checks.
- Stress: 100 simultaneous impacts test ensures pacing/soft/hard caps work under load without pool corruption.
All Audio/VFX tests are green locally in Debug with SDL2 and `-j8`.

Phase 9 (determinism & replay):
- Ordering determinism: dispatcher sorts by (emit_frame, priority, id, seq). Sequence normalized post-sort, eliminating producer-order ties.
- Order-insensitive digest: frame digest is XOR of per-event hashes over (type, priority, id, repeats), excluding seq; exposed via `rogue_fx_hash_accumulate_frame` and `rogue_fx_hash_get` for replay.
- Replay & hashing: `rogue_fx_replay_begin_record/end_record/is_recording`, `rogue_fx_replay_load/enqueue_frame/clear`, `rogue_fx_events_hash` (FNV-1a 64) for test validation and divergence checks.
- Tests: ordering invariance and replay/hash stability are covered by `test_audio_vfx_phase9_1_ordering_tuple` and `test_audio_vfx_phase9_2_replay_and_hash` (Debug SDL2, -j8).

## Skills – Effect Processing Pipeline (Phase 3 slice)

Initial slice of the EffectSpec pipeline is in place:
- EffectSpec extended with stacking rule, snapshot flag, periodic pulse scheduler, and simple child chaining (up to 4 children with per-child delay).
- Minimal pending-event queue processes pulses and child chains deterministically via `rogue_effects_update(now_ms)`; called from the rotation simulator loop and can be invoked from the main loop.
- Buff system integration uses existing stacking behaviors (UNIQUE/REFRESH/EXTEND/ADD). Snapshot flag is forwarded to buff records.
- Tests: `test_effectspec_tick_and_chain` validates periodic pulses at exact 100ms quanta over the duration window and a parent→child delayed apply at 50ms.
Additional stacking rules:
- MULTIPLY and REPLACE_IF_STRONGER were added alongside UNIQUE/REFRESH/EXTEND/ADD. Multiplicative interprets incoming magnitude as percent (150 = +50%); replace-if-stronger updates magnitude only when higher and preserves the longer remaining duration. See `tests/unit/test_effectspec_stack_variants.c`.

EffectSpec parser (Phase 3.1):
- New parser reads simple key=value text (and files) to register EffectSpecs. Fields:
	- effect.<index>.kind = STAT_BUFF
	- effect.<index>.buff_type = STAT_STRENGTH | POWER_STRIKE
	- effect.<index>.magnitude = <int>
	- effect.<index>.duration_ms = <ms>
	- effect.<index>.stack_rule = ADD|REFRESH|EXTEND|UNIQUE|MULTIPLY|REPLACE_IF_STRONGER
	- effect.<index>.snapshot = 0|1
	- effect.<index>.pulse_period_ms = <ms>
	- effect.<index>.childN.id / effect.<index>.childN.delay_ms
- Registration preserves ascending index and defaults stack_rule to ADD if unspecified; explicit UNIQUE is respected.
- Multiplicative effects are a no-op when no baseline buff of the same type exists (avoids creating a stack from zero). Covered in `tests/unit/test_effectspec_parser.c`.

Preconditions and deterministic ordering (Phase 3.2):
- Each EffectSpec may declare preconditions `require_buff_type` and `require_buff_min` to gate application. If set, the effect only applies when the total of the specified buff type meets the minimum (default min=1). When not set, a sentinel value disables the gate.
- The pending-effect scheduler is deterministic: events are processed ordered by (when_ms, seq), where seq is a per-event sequence assigned at schedule time to break ties for identical timestamps. Reset clears the sequence counter. See `tests/unit/test_effectspec_preconditions_and_order.c`.

Scaling (Phase 3.4):
- Per-attribute scaling lets an effect’s magnitude scale with another buff’s total: fields `scale_by_buff_type` and `scale_pct_per_point` compute `effective = magnitude * (100 + pct*total)/100`.
- `snapshot_scale=1` captures the scaled magnitude at apply time and uses it for all scheduled pulses; `snapshot_scale=0` recomputes at each pulse based on live totals.
- Parser keys supported: `scale_by_buff_type`, `scale_pct_per_point`, `snapshot_scale`.
- Covered by `tests/unit/test_effectspec_snapshot_scale.c`.

Same-timestamp ordering (determinism): when a periodic pulse and a child effect are scheduled for the exact same `when_ms`, the system processes the pulse before the child. Locked by unit `tests/unit/test_effectspec_pulse_child_order.c`.

DOTs (Phase 5 summary):
- Debuff flag: DOT specs default to harmful (debuff=1) unless explicitly overridden.
- Stacking semantics: UNIQUE blocks reapply while active; REFRESH cancels and realigns pulses from the latest apply; EXTEND keeps existing pulse schedule and extends end time. Internally tracks `last_apply_ms` and skips stale pulses for REFRESH.
- Crit modes: per-application crit snapshot vs per-tick crit (deterministic hash); test hooks support a one-shot/global override for determinism.
- Mitigation & logging: Each tick routes through combat mitigation and records damage events including overkill.
- Tests: `test_effectspec_dot_basic`, `test_effectspec_dot_stack`, and `test_effectspec_dot_crit` validate duration, stacking, and crit behavior alongside existing ordering/scaling tests. All pass in Debug with SDL2 (-j8).

### Skill Events on the Event Bus (Phase 7.1)

Two new skill events are emitted by the skills runtime and delivered via the central event bus:
- ROGUE_EVENT_SKILL_CHANNEL_TICK (0x0701): fired at deterministic channel tick boundaries. Payload: skill_id (u16), tick_index (1-based, u16), when_ms (double scheduled time).
- ROGUE_EVENT_SKILL_COMBO_SPEND (0x0702): fired when a combo spender consumes points (instant, cast-complete, or channel). Payload: skill_id (u16), amount (u8), when_ms (double).

Usage pattern (tests and gameplay):
- Subscribe with `rogue_event_subscribe(event_id, callback, user)`.
- Publish occurs inside skills runtime; to deliver callbacks in headless/unit tests, call `rogue_event_process_priority(now_ms)` periodically. The queue is deterministic and callbacks run in publish order by (when_ms, seq).
- See `tests/unit/test_skills_phase7_event_bus.c` for a minimal example that subscribes, advances skills over simulated time, pumps the bus, and asserts receipt.

### Auras & Area Effects (Phase 6 – slice)

- New EffectSpec kind AURA with fields: `aura_radius` and `pulse_period_ms`.
- Runtime: each pulse applies damage to enemies within radius of the player, flowing through the standard mitigation and damage-event pipeline. Debuff defaults to 1 unless specified.
- Determinism: per-tick crits use a deterministic hash; pulse schedule uses the same pending-event queue as DOTs (tick at t=0 then every `pulse_period_ms` until duration end).
- Defaults: unspecified `aura_radius` defaults to 1.5f for safety; harmful magnitude implies `debuff=1` if unset.
- Exclusivity: optional `aura_group_mask` enables replace-if-stronger behavior across mutually exclusive aura groups; weaker re-applies are ignored. Covered by `test_effectspec_aura_exclusive`.
- Test: `test_effectspec_aura_basic` validates entry/exit and pulse timing determinism. All EffectSpec tests pass in Debug with SDL2 (-j8).

## Buffs – Phase 4 snapshot

- Handle-based pool with free list and 16-bit generations prevents stale handle reuse; legacy API remains for simple integer totals.
- Public handle API: apply_h, refresh_h, remove_h, query_h; expiration callback hook via `rogue_buffs_set_on_expire`.
- FX triggers on gain/expire: keys `buff/<type>/gain` and `buff/<type>/expire` are published for the audio/VFX layer.
- Dampening window (default 50ms) to prevent oscillation from rapid re-apply; configurable via `rogue_buffs_set_dampening(ms)`.
- Persistence: save writes compact (type, magnitude, remaining_ms); load re-applies with snapshot=1, respecting remaining durations against current `now_ms`.
- Tests: save/load roundtrip tests report expected active buffs; skills/effects tests for snapshot scaling and ordering pass in Debug with SDL2 (-j8).

Phase 4.3–4.6 additions:
- Categories & CC flags: `RogueBuffCategoryFlags` (OFFENSIVE, DEFENSIVE, MOVEMENT, UTILITY) with CC subflags (STUN, ROOT, SLOW). Combat reads live flags for gating.
- CC diminishing returns (DR): anchored window (default 15s) with factors 1.0 → 0.5 → 0.25 → 0.0 applied before stacking/dampening on every apply path. Zero-duration CC increments DR without allocating an instance. Expired instances are skipped during dampening/stacking so natural on_expire still fires.
- Combat semantics: STUN/DISARM block buffering and attack start; ROOT allows buffering but blocks attack start. Unit `test_combat_phase4_cc` codifies behavior.
- Tests: `test_buffs_phase4_dr_and_handles` validates DR progression/decay, natural vs manual expire callbacks, and handle reuse invalidation. Both tests pass in Debug with SDL2 (-j8).
