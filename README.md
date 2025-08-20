<div align="center>

# Roguelike (Top‑Down Zelda‑like) – C / SDL2

[![Build Status](https://github.com/ChubbyChuckles/Roguelike/actions/workflows/ci.yml/badge.svg)](../../actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Standard: C11](https://img.shields.io/badge/standard-C11-blue.svg)]()

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
| 8. Performance & Determinism | RNG model, perf strategies |
| 9. Tooling & Maintainability | Audits, hot reload, schema & integrity tools |
| 10. Roadmap Snapshot | Current milestone excerpt & where to find full plans |
| 11. Changelog (Highlights) | Recent notable changes (curated) |
| 12. Contributing | Contribution standards |
| 13. License | MIT license reference |
| 14. Media / Screenshots | Placeholder images / planned diagrams |
| 15. Quick Reference Cheatsheet | Frequently used commands |
| Appendix A | Full original README phase logs (complete detail) |

---

## 1. High‑Level Overview
Layered, deterministic top‑down action roguelike engine emphasizing: modular boundaries, reproducible simulation, incremental phase roadmaps, strong test coverage (unit, integration, fuzz, statistical), and data‑driven extensibility (hot reload, schema docs, JSON/CSV/CFG ingest). Systems include loot & rarity, equipment layering (implicits → uniques → sets → runewords → gems → affixes → buffs), dungeon & overworld procedural generation, AI behavior trees + perception + LOD scheduling, skill graph & action economy, persistence with integrity hashing, UI virtualization & accessibility, and emerging economy/crafting pipelines.

### Core Design Pillars
* Determinism first (explicit seeds, reproducible golden snapshots)
* Progressive complexity (phased roadmaps with tests per slice)
* Data before code (config/schema/hot reload/tooling surfaces early)
* Integrity & telemetry (hashes, anomaly detectors, analytics export)
* Maintainability (module boundaries audit, minimal cross‑module coupling)

---

## 2. Feature Matrix (Implemented vs Planned)
The table is unchanged from the original content; detailed subsystem write‑ups now reside in Appendix A. This condensed matrix guides newcomers to active development fronts.

| Domain | Key Implemented Features | Upcoming / Planned |
|--------|--------------------------|--------------------|
| Core Loop | Deterministic frame stepping, modular update phases | Advanced AI scheduling refinements |
| World Gen | Multi‑phase continents→biomes→caves→dungeons→weather→streaming | Advanced erosion SIMD, biome mod packs expansion |
| AI | BT engine, perception, LOD scheduler, pool, stress tests | Higher order tactics, adaptive difficulty tie‑ins |
| Combat & Skills | Attack chains, mitigation, poise/guard, reactions, AP economy, multi‑window attacks | DOTs, advanced status, combo extensibility |
| Loot & Rarity | Dynamic weights, pity, rarity floor, affixes, generation context | Economy balancing, adaptive vendor scaling |
| Equipment | Slots, implicits, uniques, sets, runewords, sockets, gems, durability, crafting ops, optimization, integrity gates | Deeper multiplayer authority, advanced enchant ecosystems |
| Persistence | Versioned sections, integrity hash/CRC/SHA256, incremental saves | Network diff sync, rolling signature policies |
| UI | Virtualized inventory, skill graph, animation, accessibility, headless hashing | Full theme hot swap diff + advanced inspector polish |
| Economy/Vendors | Pricing, restock rotation scaffold, reputation, salvage, repair | Dynamic demand curves, buyback analytics |
| Crafting | Recipes, rerolls, enchant/reforge, fusion, affix orb, success chance | Expanded material tiers, automation tooling |
| Progression | Infinite leveling design, Phase 0 stat taxonomy, Phase 1 infinite XP curve (multi-component, catch-up, 64-bit accum + tests), Phase 2 attribute allocation & re-spec (journal hash + tests), maze skill graph UI (initial), stat scaling | Full maze runtime integration, mastery loops |
| Difficulty | Relative ΔL model (enemy vs player level differential) + adaptive performance scalar (+/-12%) | Dynamic encounter budget + boss phase adaptivity |
| Tooling | Audit scripts, schema docs, diff tools, profilers, fuzz harnesses | Extended golden baselines & editor integration |

---

## 3. Systems Overview (Concise Summaries)
Below are brief system capsules. Full phase-by-phase narrative, metrics, and tests remain intact in Appendix A.

### Loot & Rarity
Deterministic multi‑pass pipeline (rarity → base item → affixes) with dynamic weighting, pity counts, rarity floors, adaptive balancing hooks, histogram & drift analytics, personal loot, and statistical QA (distribution + fuzz tests).

### Equipment Layering
Layer order: implicit → unique → set (with interpolation) → runeword → sockets/gems → affix → buffs → derived metrics. Budgets enforce power ceilings; durability, fracture, integrity hash chain, GUIDs, and snapshot gates guard regression & tampering. Optimization, proc analytics, fuzz, stress & mutation tests gate CI.

### Combat & Skills
Frame-accurate attack windows, multi-hit windows, poise/guard/hyper armor, reaction & CC system, AP economy, cast/channel infrastructure, damage event telemetry, penetration & resist layering, positional mechanics (backstab/parry), lock-on, hitbox authoring & broadphase.

### World Generation
Phased macro→micro pipeline: continents, elevation, rivers, biomes, caves, erosion, structures, dungeons, fauna ecology, resource nodes, weather patterns, streaming chunks, analytics metrics & anomaly detection, hot‑reloadable biome packs.

### AI System
Behavior tree engine, blackboard with TTL & policies, perception (LOS/FOV/hearing/threat), tactical nodes, LOD scheduler spreading computation, profiling & budget enforcement, agent pooling, stress tests (200 agents), debug visualization & determinism verifiers.

### UI & UX
Immediate‑mode deterministic UI with hash diffing, virtualization, inventory & vendor panels, skill graph viewport with animations, accessibility (colorblind modes, reduced motion), navigation graph, headless harness for golden tests, theme hot swap, HUD systems, alerts, metrics overlay.

### Persistence
Versioned TLV sections, varints, compression, CRC32 + SHA256 integrity, signature trailer, incremental mode, autosaves, replay hash, migrations, durability & equipment slot evolution, fast partial inventory saves, robust tamper detection & recovery.

### Economy & Crafting
Vendor inventory generation, rarity-based pricing ladder, reputation discounts, salvage yields, repair cost scaling, reroll/enchant/reforge workflows, upgrade stones, affix extraction/fusion, success chance skill gating, durability-integrated costs, socket/gem economic operations.

### Progression & Difficulty
Infinite leveling model (sublinear growth), maze-based skill graph (UI slice live), relative level differential (ΔL) scaling for enemy challenge vs trivialization, planned mastery rings & perpetual scaling curves.

### Tooling & Integrity
Maintenance audit script, schema export & diff, hot reload watchers, scripting sandbox (add/mul stat ops), budget analyzer, hashing snapshots (equipment + damage events), fuzz harnesses (parsers, equip sequences, mutation), micro-profiler & performance arenas.

---

## 4. Build & Run (Essentials)
The authoritative detailed commands remain unchanged (see Appendix A original Section 5). Quick examples:

Windows (PowerShell):
```powershell
cmake -S . -B build -DROGUE_ENABLE_SDL=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```
Headless logic build:
```powershell
cmake -S . -B build -DROGUE_ENABLE_SDL=OFF
cmake --build build --config Release
```
Run tests:
```powershell
ctest --test-dir build -C Debug --output-on-failure
```

---

## 5. Configuration & Assets
Configs live under `assets/` (items, affixes, loot tables, biomes, projectiles, skill graph prototypes). Hot reload exists for selected registries (biomes, equipment sets). See detailed schemas & examples in Appendix A and `docs/` folder (loot architecture, rarity style, ownership & boundaries).

---

## 6. Testing & Quality Gates
Extensive per-phase unit tests, fuzzers, statistical distribution checks, mutation robustness, performance guards, integrity snapshots, and labeled gating suites (e.g., Equipment Phase 18). Deterministic RNG across systems ensures reproducible failures.

---

## 7. Development Workflow (Abbreviated)
1. Pick next roadmap slice (roadmaps/ files).
2. Implement isolated module/code changes.
3. Add deterministic unit/integration tests.
4. Update phase status / docs.
5. Run full test suite & gating labels locally.
6. Commit with conventional scope prefix.

---

## 8. Performance & Determinism (Summary)
Single LCG & scoped RNG channels, arenas & pools minimizing allocations, optional SIMD stubs prepared, deterministic threading (parallel worldgen & async optimization produce identical results). Hashing & snapshots detect drift between refactors.

---

## 9. Tooling & Maintainability
Audit script (`tools/maint_audit.py`), schema docs/export, diff & budget analyzers, scripting sandbox, hot reload registry, integrity scanners (proc anomalies, banned affix pairs, hash chains), profiler & performance metrics collectors.

---

## 10. Roadmap Snapshot
See `roadmaps/` for subsystem implementation plans (inventory, crafting & gathering, character progression w/ infinite leveling & maze skill graph, enemy difficulty ΔL model, vendors, dungeons, world bosses). Snapshot table preserved in Appendix A original Section 10.

---

## 11. Changelog (Curated Recent Highlights)
* Infinite leveling + maze skill graph design integrated (progression roadmap).
* Enemy difficulty ΔL relative scaling model adopted.
* Added vendor, dungeon, world boss comprehensive implementation plans.
* Equipment integrity gates (fuzz, mutation, snapshot, proc stats) formalized.
* Expanded crafting: sockets, gems, enchant/reforge, fusion, affix orbs, success chance.
* Persistence: incremental saves, signature trailer, replay hash, compression, autosave throttle.
* Inventory System Phase 1: unified def->quantity entries, soft distinct cap + pressure metric, overflow guard, labels (materials/quest/gear), cap handler hook, dirty delta tracking API, and binary save integration (new inv_entries component id=9).
* Inventory System Phase 3 (complete): Favorites/lock flags + short tag metadata (inv_tags id=10); auto-tag rule engine (rarity/category predicates) + accent colors (inv_tag_rules id=11); enforcement (locked/favorite blocks salvage/drop) and determinism tests. Tests: `test_inventory_phase3_tags`, `test_inventory_phase3_rules`, `test_inventory_phase3_lock_rule_enforce`.
* Inventory System Phase 4 (in progress): Added query expression parser (rarity/qty/tag/equip_slot/affix_weight/quality/durability_pct/category) with AND/OR + comparison operators (basic diagnostics hook), trigram fuzzy search index with lazy build + incremental dirty-bit rebuild path, stable multi-key composite sort (rarity, qty, name, category with descending markers via `-key`), saved searches registry (persisted via save component id=12) plus quick-apply helper API & quick action bar wrappers (index-based enumerate/apply), query result LRU cache (32 entries, invalidated on inventory or instance mutation) with hit/miss stats, and mutation hook wiring to quantity & instance changes. Tests: `test_inventory_phase4_query_sort`, `test_inventory_phase4_saved_searches`, `test_inventory_phase4_saved_searches_persist`, `test_inventory_phase4_query_cache`, `test_inventory_phase4_quick_actions`. Pending: parser fuzz harness, cache hit ratio threshold test, leak/perf guards.
* AI: LOD scheduler, stress test, agent pool, time budget profiler, tactical nodes.
* Worldgen: streaming manager, weather scheduler, dungeon generator, analytics & anomalies.
* Hit System Phases 1–4: Weapon-length capsule sweep with directional normals; per-strike duplicate prevention; runtime tuning JSON; refined knockback scaling (level+strength differential); impact flash + particle bursts (overkill explosion variant); SFX hook stub; tests for geometry, sweep normals, knockback, and Phase 4 feedback.
* Hit System Phase 5: Replaced legacy slash sheet with 8-frame per-weapon pose overlay (JSON pose + 8 discrete frame images) rendered with per-frame dx/dy/angle/scale/pivot transforms; added `weapon_pose` loader module and integrated rendering path; unit test `test_weapon_pose_loader` verifies JSON load & defaults. Added MVP `weapon_pose_tool` (SDL2) for authoring frame transforms (keyboard controls) – PNG trail effect deferred.
* Hit System Phase 6 (partial-done): Added real-time hit debug overlay toggle (F) rendering sweep capsule, impact points, normals, and textual tuning stats; unit test for toggle; deferred enemy AABB and ring buffer playback for later integration.
* Hit System Phase 7 (integration): Removed legacy reach/arc logic (sweep authoritative), first-hit-only hitstop injection, maintained unchanged combo/timing FSM, ensured explosion vs normal death path preserved, integrated knockback magnitude function reuse, added integration test (single hitstop & non-duplicate hits). Profiling (7.7) pending.
* Hit Detection Pixel Masks Slice A: Introduced `hit_pixel_mask` module (frame/set structs, packed bit storage, placeholder procedural masks) and unit test `test_hit_mask_basic`; groundwork for upcoming pixel-based collision path alongside existing capsule sweep.
* Tool Update: Reworked `weapon_pose_tool` to use single weapon image overlaid on 8 sliced frames from a player slash animation sheet (PNG via WIC or BMP fallback), adding direction/frame_size metadata to exported JSON.
* Hit Detection Pixel Masks Slice B: Runtime toggle-integrated pixel broadphase (AABB) + narrowphase sampling path in `rogue_combat_weapon_sweep_apply`, helper APIs, integration test, fallback safety retained.
* Hit Detection Pixel Masks Slice C: Dual-path (capsule + pixel) evaluation with mismatch logging, cumulative counters, extended debug overlay (mask AABB placeholder + per-hit coloring), and mismatch unit test.
* Enemy Difficulty Phase 0: Introduced taxonomy (archetypes + canonical budgets) & tier multipliers (Normal→Nemesis) with monotonic HP growth and test coverage.
* Enemy Difficulty Phase 1 (core slice): Added sublinear base scaling curves, relative level differential (ΔL) penalty/buff model with dominance + trivial thresholds, reward scalar function, parameter file loader, and unit tests (monotonicity, multiplier invariants, reward thresholds). Roadmap updated (remaining Phase 1 items deferred: attribute rating curves, biome param variants, UI indicator, full TTK heatmap harness).
* Enemy Difficulty Phase 1 (completed): Added attribute curves (crit/resists), biome parameter registry, ΔL severity classifier, TTK estimation helper, extended tests validating attributes, biome overrides, severity & TTK ordering.
* Enemy Difficulty Phase 2 (initial modifiers slice): Added procedural modifier system core: definition parser (id/name/weight/tier mask/budget costs/incompat/telegraph), deterministic weighted selection respecting per-dimension (dps/control/mobility) budget caps, incompatibility mask enforcement, telegraph token stored for future UI, and unit tests covering load, determinism, incompat exclusion, and budget compliance.
* Enemy Difficulty Phase 1.6 UI indicator: Added HUD ΔL severity indicator adjacent to player level text with color-coded severity categories (advantage, dominance, trivial, threat, major threat, fatal) sourced from classifier; roadmap updated marking 1.6 Done.
* Enemy Difficulty Phase 3 (initial encounter composition slice): Added encounter template loader (`encounters.cfg`), deterministic spawn composer (counts, elite spacing & chance, boss + support units), unit tests verifying template load, count bounds, boss/support presence & level assignment; roadmap updated (3.1–3.3,3.5 Done; 3.4 partial telegraph/env props deferred).
* Enemy Difficulty Phase 4 (adaptive feedback loop): Performance KPIs (avg TTK, damage intake rate, potion usage, death frequency) with EMA smoothing, kill-event gated upward pressure, decay & idle relaxation, bounded scalar (0.88–1.12) applied after relative ΔL & tier/modifier adjustments, opt-out flag, and unit test covering disable, upward, downward, relaxation convergence.

For exhaustive historical details, refer to Appendix A (original full logs) and roadmap files.

---

## 12. Contributing
Standards remain unchanged (tests required, deterministic behavior, no unrelated formatting). See `CONTRIBUTING.md` and tooling guidelines in Appendix A.

---

## 13. License
MIT – see `LICENSE`.

---

## 14. Screenshots / Media (Placeholders)
Table retained as in original (see Appendix A Section 14) – diagrams pending.

---

## 15. Quick Reference Cheatsheet
Most used commands (full table duplicated in Appendix A end):

| Task | Command |
|------|---------|
| Configure Debug (SDL) | `cmake -S . -B build -DROGUE_ENABLE_SDL=ON -DCMAKE_BUILD_TYPE=Debug` |
| Build (Win) | `cmake --build build --config Debug` |
| Run Tests | `ctest --test-dir build -C Debug --output-on-failure` |
| Specific Test Regex | `ctest -C Release -R loot_phase8_generation_basic -V` |
| Format | `cmake --build build --target format` |
| Static Analysis | `cmake --build build --target tidy` |

---

## Appendix A – Full Phase Logs (Original Detailed Content)
The remainder of this document is the unedited original README content (phases, deep dives, exhaustive logs) preserved verbatim for historical and technical reference.

---

### Maintainability & Module Boundaries (Phase M1–M2 Complete)

Foundational documentation and automated audit introduced:

* `docs/OWNERSHIP.md` – clarifies review/maintenance responsibility per top-level module.
* `docs/DEPENDENCY_BOUNDARIES.md` – enforced layering & private header visibility rules.
* `docs/INTERNAL_HEADERS.md` – inventory of private `_internal.h` headers (kept in sync with refactors).
* `tools/maint_audit.py` – lightweight static audit (run manually or wire into CI) checking:
  - util/ isolation (no cross-module includes)
  - no external inclusion of another module's `_internal.h`
  - public header function name prefix (`rogue_`)
  - simple include cycle detection

Run locally:
```
python tools/maint_audit.py
```
Exit code !=0 flags a violation (see first printed AUDIT FAIL line).

Phase M2 added:
* `core/features.h` capability macros (`ROGUE_FEATURE_*`) for conditional tests/tools.
* Combat damage observer interface (register callbacks for `RogueDamageEvent` emission) with unit test `test_combat_damage_observer`.
* Deprecation annotation macro (`ROGUE_DEPRECATED(msg)`).
* Public header pass pruning & observer logic isolated (no leak of internal state).

Phase M3 (data-driven pipeline) progress:
* Unified key/value parser utility (`util/kv_parser.c,h`) with dedicated unit test `test_kv_parser`.
* Schema + validation layer (`util/kv_schema.c,h`) with unit test `test_kv_schema` surfacing unknown keys, type errors, and missing required fields.
* Hot reload system (`util/hot_reload.c,h`) with registry, manual force trigger, and automatic content hash change detection (`test_hot_reload`).
* NEW: Asset dependency graph (`util/asset_dep.c,h`) providing cycle detection + recursive FNV-1a combination hashing; unit test `test_asset_dep` validates hash change on dependency modification and cycle rejection.
* NEW: Externalized projectile & impact tuning (`core/projectiles_config.c,h` + `assets/projectiles.cfg`) driving shard counts, speeds, lifetimes, sizes, gravity, and impact lifetime; integrated hot reload (`test_projectiles_config`).
* NEW: Hitbox directory loading (Phase M3.6) via `rogue_hitbox_load_directory` allowing aggregation of multiple `.hitbox` / `.json` primitive definition files (`test_hitbox_directory_load`).
* NEW: Persistence VERSION tags (Phase M3.7) for player stats & world gen parameter files (backward compatible; legacy files default to v1) with accessor APIs (`test_persistence_versions`).
* NEW: Phase M4.1 test expansion – projectile & persistence edge cases (`test_projectiles_config_edge`, `test_persistence_edge_cases`) covering partial config overrides, missing file load invariant, negative VERSION clamping, and dirty save gating.
* NEW: Phase M4.2 deterministic hashing + M4.3 golden master replay harness (`util/determinism.c,h`, `test_determinism_damage_events`) providing FNV-1a hash of `RogueDamageEvent` sequences (stable for identical sequences, differing for variants) and text round-trip serialization for future combat replay validation.
* NEW: Phase M4.4 parser fuzz tests (`test_fuzz_parsers`) exercising affix CSV loader, player persistence key/value loader, and unified kv parser with randomized malformed/partial lines (ensures graceful ignore, clamps VERSION >0, no crashes, and bounded iteration) establishing baseline robustness harness.
* NEW: Equipment Phase 2 (advanced stat aggregation) initial slice completed:
  - Layered stat cache fields (base / implicit / affix / buff) with deterministic fingerprint hashing.
  - Affix system extended (strength/dexterity/vitality/intelligence/armor flat) feeding `g_player_stat_cache.affix_*` via non‑mutating equipment aggregation pass.
  - Derived metrics populated (DPS, EHP, toughness, mobility, sustain placeholder).
  - Soft cap helper (`rogue_soft_cap_apply`) + continuity & diminishing returns unit tests.
  - Ordering invariance & fingerprint mutation tests (`test_equipment_phase2_affix_layers`, `test_equipment_phase2_stat_cache`).
  - Resist breakdown (physical, fire, cold, lightning, poison, status) with affix-driven aggregation, soft (75%) + hard (90%) caps and test (`test_equipment_phase2_resists`).

Next phases (M3+) will introduce unified config schema, hot reload, and expanded deterministic replay/coverage gates.

### Equipment System Phase 7.2–7.5 (Defensive Layer Extensions Continued)

Implemented in this slice:
* Damage Conversion (7.2): Added affix stats `phys_conv_fire_pct`, `phys_conv_frost_pct`, `phys_conv_arcane_pct` (enum + parser + aggregation). Incoming physical melee damage now partitions into elemental components with a 95% aggregate cap (always at least 5% remains physical). Conservation enforced (post‑conversion sum == original raw) – validated by new unit test `test_equipment_phase7_conversion_reflect`.
* Guard Recovery Modifiers (7.3): New `guard_recovery_pct` affix scales guard meter regeneration (multiplicative, clamped 0.1x–3.0x) and inversely scales hold drain (floor 0.25x) inside `rogue_player_update_guard`.
* Thorns / Reflect (7.5): Added `thorns_percent` and `thorns_cap` affix stats; reflect computed after conversion & mitigation ordering inside `combat_guard` (currently attacker application deferred until enemy context wire-up). Cap enforces upper bound per hit.
* Tests (7.6 partial): `test_equipment_phase7_conversion_reflect` covers conversion conservation, cap behavior when total attempted conversion exceeds 95%, guard recovery positive scaling math, and thorns integration placeholder path (ensuring no crash and ordering stable). Existing block tests (`test_equipment_phase7_defensive`, `test_equipment_phase7_block_affixes`) remain green.

Reactive shield procs (7.4) now implemented: proc system exposes absorb pool helpers consumed in melee pipeline prior to reflect. Test `test_equipment_phase7_reactive_shield` validates shield absorption ordering (block → conversion → absorb → reflect) and depletion.

---

### Equipment System Phase 8.1 (Durability Model)

Implemented non-linear durability decay (logarithmic severity scaling with rarity mitigation) in `core/durability.c`:
* Formula: `loss = ceil(base * log2(1 + severity/25) * (1/(1+0.35*rarity)))`, with `base=2` for severe events (>=50 severity).
* Diminishing returns on extremely large severity values while retaining meaningful chip damage for small hits (minimum scaled floor).
* Higher rarity items degrade more slowly (rarity factor divisor), reinforcing acquisition value without granting infinite endurance.
* API: `rogue_item_instance_apply_durability_event(inst,severity)` – future integration slice will replace fixed per-hit decrement sites.
* Unit test `test_equipment_phase8_durability_model` validates: (a) monotonic non‑decreasing loss vs severity for a single item, (b) higher rarity never exceeds common loss for identical severity events.
* Roadmap updated (Phase 8.1 Done; 8.6 partial until repair cost & salvage coupling tests land).

### Equipment System Phase 8.2 (Repair Cost Scaling)

Added `rogue_econ_repair_cost_ex(missing, rarity, item_level)` implementing multi-factor repair cost:
* Unit cost = `(6 + rarity*6) * (1 + sqrt(item_level)/45)` producing gentle early scaling and higher late-game sink without explosive growth.
* Legacy `rogue_econ_repair_cost` kept as wrapper (item_level=1) for existing call sites; equipment repair updated to use new API with item instance level.
* Unit test `test_equipment_phase8_repair_cost` validates monotonic increase with missing durability, higher cost for higher rarity, and level-driven growth (1 < 50 < 200).
* Roadmap Phase 8.2 marked Done; Phase 8.6 test coverage extended (cost monotonicity).

### Equipment System Phase 8.3 (Auto-warn Thresholds & Notifications)
Centralized durability bucket helper (good >=60%, warn >=30%, critical <30%) moved to `durability.c` and exposed to both UI and systems. Added transition notification state (`rogue_durability_last_transition`) allowing tests or future HUD flashes to react exactly once when dropping into warn or critical without polling each frame.

### Equipment System Phase 8.4 (Instance-aware Salvage Yield)
Salvage yields now scale with remaining durability for specific item instances: factor = `0.4 + 0.6 * (cur/max)` (40% floor for broken gear). Implemented via `rogue_salvage_item_instance` used preferentially by inventory UI when an active instance exists; falls back to definition-based salvage otherwise.

### Equipment System Phase 8.5 (Fracture Mechanic)
Items that reach 0 durability become `fractured` imposing a 40% damage penalty (current min/max damage multiplied by 0.6). Full repair clears the flag, restoring baseline performance. This introduces tangible gameplay pressure to repair rather than ignoring durability until salvage.

### Equipment System Phase 13 (Persistence & Migration Complete)

Introduces a versioned, forward-compatible equipment serialization layer with deterministic integrity hashing and legacy slot migration:

* Versioned Schema (13.1): `EQUIP_V1` header followed by one line per occupied slot. Each line encodes slot index and key/value pairs: base def, item_level, rarity, prefix/suffix indices & rolled values, durability (cur/max), enchant level, quality, socket count + up to 6 gem ids, affix lock flags, fracture flag, plus `SET <id>`, unique id token `UNQ <string>` ("-" if none), and synthetic runeword pattern token `RW <pattern>` (derived from gem ordering or '-' if none). Unknown future tokens are skipped safely.
* Slot Migration (13.2): Legacy (pre-header) format without `EQUIP_V1` is treated as version 0 and remapped: indices 0..5 (WEAPON, HEAD, CHEST, LEGS, HANDS, FEET) translated to current enum; other indices skipped. New test `test_equipment_phase13_slot_migration` validates remap & round-trip forward serialization.
* Unique/Set/Runeword (13.3): Set id, unique id (string from unique registry), and runeword pattern tokens serialized.
* Integrity Hash (13.4): 64-bit FNV-1a over canonical serialized buffer via `rogue_equipment_state_hash` for tamper detection / analytics fingerprinting. Deterministic across identical equip states regardless of equip order.
* Optional Fields (13.5): Loader tolerates missing (legacy) fields like `SOCKS`, `ILVL`, `ENCH`, `QC`; defaults applied (count=0, level=1, enchant=0, quality=0, gems=-1).
* Tests (13.6): `test_equipment_phase13_persistence` (round-trip, omission of new tokens, tamper hash divergence) and `test_equipment_phase13_slot_migration` (legacy remap) ensure backward + forward compatibility and integrity detection.

### Equipment System Phase 14 (Performance & Memory – Expanded)

Performance layer now covers memory pooling, aggregation variants, micro-profiling, and parallel loadout optimization:
* SoA Slot Arrays (14.1): `equipment_perf.c,h` maintain per-slot primary stat & armor contributions plus cached totals.
* Frame Arena (14.2 Complete): Linear arena (`rogue_equip_frame_alloc/reset/high_water/capacity`) integrated into loadout optimizer candidate enumeration eliminating per-iteration stack/heap pressure; tests assert high-water stability across repeated invocations.
* Batch Aggregation (14.3 Conceptual SIMD): 4-slot batch loop produces identical totals to scalar baseline; pluggable path allows future SSE/AVX implementation guarded by feature detection.
* Parallel Affinity (14.4): Async optimizer API (`rogue_loadout_optimize_async/join/async_running`) dispatches optimization on a worker thread (Win32 CreateThread) with deterministic fallback synchronous path when threading unavailable; preserves deterministic results (fixed slot & instance ordering) and instruments launch + body with profiler zones.
* Micro-Profiler (14.5): Zone profiler covers aggregation modes, synchronous optimize, and async launch; JSON dump helper for potential CI perf diffing.
* Perf & Parallel Tests (14.6 Extended): `test_equipment_phase14_perf` (aggregation parity & profiler), and new `test_equipment_phase14_parallel` validating async improvement non-decrease, join semantics, and arena reuse (no capacity overflow across successive optimizer runs).

### Equipment System Phase 15 (Integrity & Anti-Cheat Foundations – Completed)

Multiplayer integrity layer now includes proc anomaly auditing, banned affix blacklist, hash chain & GUID tamper detection:
* GUIDs (15.3): Every spawned item instance receives a 64-bit pseudo-random GUID (definition id, instance index entropy, quantity). `test_equipment_phase15_integrity` validates uniqueness.
* Equip Hash Chain (15.2): Rolling 64-bit chain per item updated on equip/unequip (slot id + GUID + action salt) forming a tamper-evident transcript of lifecycle events.
* Server Validation Scaffolding (15.1): Equip path maintains strict slot category & two-hand gating prior to state mutation; with GUID + chain available, an authoritative server can replay events and compare chains.
* Proc Replay Auditor (15.4): `rogue_integrity_scan_proc_anomalies` scans registered procs and returns any exceeding a configurable triggers-per-minute threshold (simple absolute check suitable for first-line heuristic flagging). Exercised in `test_equipment_phase15_replay_auditor` using fast vs slow proc definitions.
* Banned Affix Blacklist (15.5): Lightweight unordered pair list of forbidden affix combinations (`rogue_integrity_add_banned_affix_pair`, `rogue_integrity_is_item_banned`). Unit test injects an item with both affixes and asserts ban detection.
* Hash Chain / GUID Tamper Tests (15.6): Replay auditor test mutates an item's stored chain and GUID directly; mismatch & duplicate detection surfaces anomalies via `rogue_integrity_scan_equip_chain_mismatches` and `rogue_integrity_scan_duplicate_guids`.
* Public Integrity APIs (`equipment_integrity.h`): Proc anomaly scan, affix blacklist management, expected equip hash recomputation, mismatch & duplicate GUID scanners.

Forward Work: Statistical proc distribution validation (Phase 18), authoritative network transport & signed state diffs (later multiplayer phases), and deeper anomaly heuristics (rolling Z-scores / EWMA) once broader telemetry (latency jitter, server tick alignment) is present.

### Equipment System Phase 16.4 (Runeword Recipe Validator)
Introduced validation for externally-authored runeword patterns to eliminate malformed or impossible recipes entering the registry:
* Pattern Rules: lowercase letters/digits separated by single underscores, max 5 segments, max stored length 11 (fits internal buffer with NUL), no double underscores, no uppercase or symbols.
* API: `rogue_runeword_validate_pattern` (returns 0 OK or negative error code) invoked by `rogue_runeword_register` (now rejects invalid with -3).
* Test: `test_equipment_phase16_runeword_validator` exercises failure (null, empty, invalid char, segment overflow, double underscore) and success (single + multi-segment, max segment) cases plus registration path.
* Roadmap updated marking Phase 16.4 Done.

## Phase 16.5 – Budget Analyzer Report Generator
*(Tooling / Analytics)* Introduced `equipment_budget_analyzer` module providing aggregate affix budget utilization metrics over all active item instances.

Exposed APIs:
- `rogue_budget_analyzer_run(&report)` – scans active instances computing:
  - `item_count`
  - `over_budget_count`
  - `avg_utilization` (mean ratio of total affix weight / cap)
  - `max_utilization` + `max_item_index`
  - histogram buckets: `<25%`, `<50%`, `<75%`, `<90%`, `<=100%`, `>100%`
- `rogue_budget_analyzer_export_json(buf,cap)` – deterministic JSON suitable for CI dashboards.

Test `test_equipment_phase16_budget_analyzer` fabricates synthetic items spanning utilization brackets (including an intentional over‑budget item) asserting bucket population, over-budget detection, and JSON field presence.

## Phase 16.6 – Analyzer Boundary & Validator Edge Tests
Added `test_equipment_phase16_analyzer_boundaries` exercising:
- Empty inventory: analyzer run & JSON export (item_count field present, no crash)
- Extreme item_level + rarity producing large cap (no divide-by-zero / overflow path covered)
- Runeword validator edge patterns (max length accepted, over-length rejected, max segments accepted, overflow segments rejected)
Closes tooling hardening for Phase 16 block.

## Phase 17.1 – Public Schema Docs
Introduced `equipment_schema_docs` module exposing `rogue_equipment_schema_docs_export` to emit a consolidated JSON document describing equipment authoring schemas:
- item_def field list (id, name, category, rarity, base damage, sockets)
- set_def structure (set_id, bonuses array with stat fields)
- proc_def fields (name, trigger enum, icd_ms, duration_ms, magnitude, max_stacks, stack_rule, param)
- runeword pattern rules (lowercase alnum, single underscores, max 5 segments, max length 11, no double underscores)
Test `test_equipment_phase17_schema_docs` asserts all sections and version tag present.

## Phase 17.2 – Hot Reload of Equipment Set Definitions
Added lightweight hot reload integration for external set definition JSON authoring:
- New API: `rogue_equipment_sets_register_hot_reload(id, path)` registers a watcher using the shared `hot_reload` infrastructure.
- Reload Strategy: on change detection (content hash diff) the callback clears the in‑memory set registry (`rogue_sets_reset`) then re-imports the JSON via `rogue_sets_load_from_json`, providing an atomic swap style refresh for tooling / iterative tuning.
- Determinism: explicit `rogue_hot_reload_force(id)` enables controlled reloads in tests & editor; forcing after no file change leaves registry untouched.
- Unit Test: `test_equipment_phase17_hot_reload` authoring flow simulation writing initial file (1 set), forcing load, modifying file to add second set, invoking `rogue_hot_reload_tick` and asserting updated count & presence; verifies subsequent force does not duplicate or alter counts.

Determinism Guard (Phase 17.5 early): Added `rogue_sets_state_hash` (FNV-1a over registry) and test `test_equipment_phase17_hot_reload_hash` verifying stable hash across identical reloads and change after file modification.

This establishes the minimal live-edit feedback loop ahead of Phase 17.3 sandbox scripting hooks. Future extensions may add: granular diff application (preserve unaffected sets), runeword & proc definition reload symmetry, error surfacing (retain last good on parse failure), and sandboxed scripting integration.

## Phase 17.3 – Sandboxed Scripting Hooks (Minimal)
Introduced a deterministic, restricted scripting format for unique behavior stat adjustments:
- Script Lines: `add <stat> <int>` or `mul <stat> <percent>`; up to 16 instructions; comments with `#` and blanks ignored. Enforcement: add values must be within [-1000,1000]; mul percents within [-90,500] (guarding against runaway scaling / negative overflow while still allowing debuffs).
- Supported Stats: strength, dexterity, vitality, intelligence, armor_flat, all six resist variants.
- Application Order: All add operations applied first (aggregating base deltas) followed by all mul operations (percentage increases) to ensure deterministic stacking independent of file line order for identical op ordering.
- APIs: `rogue_script_load`, `rogue_script_hash`, `rogue_script_apply` defined in new `equipment_modding` module.
- Test `test_equipment_phase17_sandbox_script` validates successful parse, math correctness (10 add +20% => 12), non-zero hash, and rejection of invalid opcode line.

## Phase 17.4 – Set Definition Diff Tool
Implemented mod vs base diff utility for set definitions:
- API `rogue_sets_diff(base_path, mod_path, buf, cap)` emits JSON object with arrays: `added`, `removed`, `changed` (changed determined by FNV hash of bonuses array per set id).
- Use Case: CI tooling & mod review to surface drift in set catalogs (e.g., tuning changes vs new content removal/additions).
- Unit test `test_equipment_phase17_sets_diff` constructs base & mod JSON: detects removed id 20, added id 30, changed id 10.

Public APIs (`equipment_persist.h`):
* `rogue_equipment_serialize(buf, cap)` – emits versioned block; returns bytes written or -1.
* `rogue_equipment_deserialize(text)` – idempotently reconstructs equipped items (spawning instances) from text; tolerates legacy headerless data.
* `rogue_equipment_state_hash()` – stable 64-bit fingerprint for auditing & future multiplayer anti-cheat chain.

Follow-up targets: serialize explicit unique/set/runeword identifiers, negative test harness (malformed tokens), hash mismatch consumer path (alert & reject), and slot expansion remap table when new cosmetic/aux slots are introduced.

## Phase 18.1 – Golden Master Snapshot
Introduced deterministic single-line snapshot of combined equipment serialization hash and aggregated stat fingerprint enabling lightweight golden master regression guards in CI.

Key points:
* Format: `EQSNAP v1 EQUIP_HASH=<16hex> STAT_FP=<16hex>` produced by `rogue_equipment_snapshot_export`.
* Comparison: `rogue_equipment_snapshot_compare(text)` parses expected hashes (ignoring unknown future tokens) returning 0 match, 1 mismatch, -1 error.
* Internals: Reuses Phase 13 equipment state hash and Phase 2.5 stat cache fingerprint (stable across equip ordering) to detect unintended drift from serialization or aggregation logic changes.
* Test `test_equipment_phase18_snapshot` captures baseline snapshot, asserts match, mutates state (equip new item) and validates mismatch against prior baseline.

This forms the foundation for upcoming Phase 18 QA expansions (fuzzing, statistical proc rate validation, stress & mutation tests) by providing a concise golden reference artifact.

## Phase 18.2 – Equip Sequence Fuzz Harness
Established a deterministic fuzz harness `rogue_equipment_fuzz_sequences(iterations, seed)` performing randomized equip, swap, and unequip operations across all equipment slots using a xorshift64* PRNG. The baseline Phase 18.2 implementation enforces a minimal invariant: no duplicate item instance index should remain equipped in multiple slots simultaneously—duplicates are auto-healed by unequipping the later slot to keep runs noise-free (not counted as violations). This minimalist scaffold keeps CI cost trivial while laying groundwork for layering deeper invariants (stat fingerprint drift checks, proc rate statistical bounds, mutation rejection) in subsequent 18.x phases.

## Phase 18.3 – Statistical Proc Rate Tests
Added deterministic simulation harness validating proc trigger counts against analytical expectations derived from internal cooldown (ICD) and event frequency. Because current proc triggering model is purely deterministic (no random per-event chance), statistical validation reduces to verifying:
  expected_hits_proc_triggers ≈ floor(total_sim_ms / icd_ms) + 1 (first immediate trigger) bounded by available triggering events (for sparse triggers like crits). The test registers three procs (fast & slow ON_HIT, plus ON_CRIT) and runs a 100s simulated combat loop (20 ms ticks, periodic crits). It asserts individual counts within ±1 tolerance and validates the fast/slow ratio matches ICD ratio (±1%). This codifies intended deterministic scaling, protecting against future regressions (e.g., if trigger ordering or global rate cap logic changes) while remaining tolerant to small adjustments (start-of-sim trigger timing). Telemetry APIs using rolling one-second windows are intentionally bypassed for stability; only raw trigger counters are used. Future phases (18.4–18.5) will layer more complex mixed-content stress and mutation fuzzing atop this foundation.

Test `test_equipment_phase18_fuzz` executes 500 iterations with seed 1337 and expects zero violations, printing a concise success marker. Future phases will extend the harness rather than creating new executables to retain historical coverage continuity.

### Equipment System Phase 8.6 (Expanded Tests)
Added `test_equipment_phase8_salvage_fracture` validating: (a) salvage yield decreases after durability loss, (b) fracture penalty reduces damage output versus an otherwise identical non-fractured item. Existing repair cost test covers cost monotonicity. Durability model smoke persists pending broader integration hooks.

### Equipment System Phase 9 (Loadout Optimization & Comparison)

Implemented a foundational yet deterministic loadout optimization pipeline:
* Snapshot & Diff (9.1): `rogue_loadout_snapshot` captures equipped instance + definition indices and derived heuristic stats (DPS/EHP/Mobility). `rogue_loadout_compare` returns per-slot change mask and diff count enabling UI deltas and regression tests.
* Heuristic Optimizer (9.2): `rogue_loadout_optimize(min_mobility, min_ehp)` performs constraint‑aware search maximizing DPS while maintaining minimum mobility and EHP thresholds. Constraints are evaluated after each candidate swap; violating candidates are discarded.
* Hill-Climb Search (9.3): Iterative improvement loop scans each slot, enumerates viable candidate instances of matching category, simulates equip, recomputes stat cache, and tracks the best improvement. Applies improvements greedily per pass until convergence or safety guard trips.
* Combination Caching (9.4): Open‑addressed hash set (256 entries) over snapshot hash (FNV‑1a of slot def indices + heuristic stats) prunes already evaluated configurations, tracking hit/insert counts via `rogue_loadout_cache_stats` for test instrumentation.
* Tests (9.5): `test_equipment_phase9_loadout_opt` covers snapshot diff correctness, non‑decreasing DPS after optimization under constraints, and cache insert accounting (capacity & basic hit path). Determinism is ensured by absence of RNG in the search ordering (stable iteration over instance indices + slot order).

Future extensions (later phases) can layer weight/encumbrance penalties, multi-objective scoring, and parallel search (Phase 14.4) atop this deterministic core.

### Equipment System Phase 10.4 (Quality Metric)
Added per-item instance quality (0–20) influencing base weapon damage (and future armor) via up to +10% linear scaling. APIs: `rogue_item_instance_get_quality`, `rogue_item_instance_set_quality`, `rogue_item_instance_improve_quality`. Unit test `test_equipment_phase10_quality` validates monotonic damage increase and clamped maximum.

### Equipment System Phase 10.1–10.3 (Crafting & Upgrade Pipelines)
Implemented foundational crafting upgrade mechanics:
* Upgrade Stones (10.1): `rogue_item_instance_apply_upgrade_stone` elevates item level by N tiers leveraging existing deterministic affix elevation logic (`rogue_item_instance_upgrade_level`) expanding budget headroom without exceeding caps.
* Affix Transfer (10.2): Added transient affix orb storage fields to item instances (`stored_affix_index/value/used`). Extraction API `rogue_item_instance_affix_extract` moves a chosen prefix or suffix into an empty orb (generic container item) clearing the source slot. Application API `rogue_item_instance_affix_orb_apply` validates slot vacancy, one-time use, and post-application budget compliance (rejects over‑cap insertions) before consuming the orb charge.
* Fusion (10.3): `rogue_item_instance_fusion` sacrifices a donor item, transferring its single highest-value affix (prefix or suffix) into a vacant corresponding slot on the target if budget allows; donor instance is deactivated atomically.
* Tests (10.6 completion): `test_equipment_phase10_crafting` covers upgrade level increase + non‑decreasing affix total, successful extraction → application (including one-time consumption flag), and fusion increasing affix weight while respecting budget & deactivating donor.
* Roadmap updated: 10.1–10.3 Done; 10.6 tests marked Done (quality monotonicity + fusion budget + transfer one-time use).

### Equipment System Phase 10.5 (Crafting Success Chance – Optional)
Introduced scalable crafting success mechanic:
* Skill-Based Chance: `rogue_craft_set_skill` / `rogue_craft_get_skill` manage crafting skill; success chance = `35% + skill*4 - rarity*5 - difficulty*3` (clamped 5%..95%).
* Attempt API: `rogue_craft_success_attempt(rarity,difficulty,rng)` deterministic w/ supplied RNG; upgrade helper `rogue_craft_attempt_upgrade` gates upgrade stone application on success.
* Test `test_equipment_phase10_crafting_success` validates skill improves success rate distribution and that upgrade attempts eventually succeed under repeated tries even at low skill (probabilistic guard). Roadmap optional item 10.5 marked Done.

### Equipment System Phase 11.1–11.6 / 11.7 (Analytics & Balancing Foundations)
Implemented core analytics instrumentation:
* Snapshot Export (11.1): `rogue_equipment_stats_export_json` emits compact JSON of key aggregated metrics (DPS/EHP/Mobility + primary stats) for tooling ingestion.
* Rarity/Slot Histograms (11.2): `rogue_equipment_histogram_record` accumulates DPS/EHP sums per rarity/slot with JSON export via `rogue_equipment_histograms_export_json` (average derivation at export time).
* Set/Unique Usage (11.3): `rogue_equipment_usage_record` tallies equipped set id counts and unique base item occurrences with JSON export via `rogue_equipment_usage_export_json` enabling popularity & penetration analytics.
* Outlier Detection (11.4): Median + MAD based detector `rogue_equipment_dps_outlier_flag` flags extreme DPS spikes (>5 MAD) enabling future automated balance telemetry.
* Proc / DR Oversaturation Flags (11.5): Balance analytics module (`equipment_balance.c,h`) tracks burst proc trigger counts and stacked damage reduction sources; `rogue_equipment_analytics_analyze` raises `proc_oversaturation` when triggers exceed threshold and `dr_chain` when cumulative remaining damage fraction drops below configurable floor.
* A/B Balance Harness (11.6): Lightweight variant registry (`rogue_balance_register`, `rogue_balance_select_deterministic(seed)`) allowing deterministic selection of tuned thresholds (MAD multiplier, proc oversat threshold, DR chain floor) for comparative testing; default variant auto-created if none registered.
* Tests (11.7 initial): `test_equipment_phase11_analytics` covers snapshot, histograms, usage, outlier detection; `test_equipment_phase11_balance` validates proc oversaturation flagging, DR chain detection, and deterministic variant selection / threshold enforcement. Roadmap updated (11.1–11.6 Done; 11.7 basic Done).

### Equipment System Phase 12 (UI / Visualization Complete)
Deterministic, headless UI helpers (text-mode) enabling regression tests and golden hashing:
* Panel Grouping (12.1): `rogue_equipment_panel_build` grouped slot listing + set progress counters.
* Full Layered Tooltip (12.2): `rogue_item_tooltip_build_layered` now includes implicit armor, gems, set tag and active runeword indicator.
* Comparative Deltas (12.3): Expanded `rogue_equipment_compare_deltas` covers damage, primary stats (Str/Dex/Vit/Int), armor, and physical resist deltas via temporary simulation swap; soft-cap saturation helper `rogue_equipment_softcap_saturation` exposed for prospective color coding (tests can assert numeric saturation rather than colors).
* Proc Preview (12.4): `rogue_equipment_proc_preview_dps` naive expected DPS contribution (triggers/min -> per second) for quick evaluation.
* Socket Drag & Gem Inventory (12.5): Selection (`rogue_equipment_socket_select`), placement (`_place_gem`), clear, plus textual gem inventory panel `rogue_equipment_gem_inventory_panel` enumerating selected socket and discovered gem defs across equipped items.
* Transmog UI (12.6): Selection state wrappers (`rogue_equipment_transmog_select` / `_last_selected`).
* Determinism & Hash (12.7): `rogue_item_tooltip_hash` (FNV-1a) + stable ordering guaranteeing identical hashes across runs; test `test_equipment_phase12_ui` validates panel presence, hash determinism, and non-negative proc preview.

All Phase 12 roadmap items now marked Done; subsequent visual polish (actual color rendering) deferred to higher-level renderer integration but logically represented via saturation helper.

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
| Dialogue | Ph0 data model+loader; Ph1 playback & UI; Ph2 tokens (${player_name}, ${run_seed}); Ph3 effects (SET_FLAG/GIVE_ITEM) idempotent; Ph4 persistence; Ph5 localization; Ph6 typewriter + skip; Ph7 analytics (line counters + digest) | Performance & Memory |

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

#### AI & Behaviour (Phases 1.1–1.7, 2, 3 Implemented)
Phase 1 core architecture established:
* `ai/core/behavior_tree.[ch]` – minimal BT engine + tick accounting & active path serialization.
* `ai/core/blackboard.[ch]` – fixed-cap typed store (int/float/bool/ptr).
* `ai/nodes/basic_nodes.[ch]` – Selector / Sequence + simple condition & stub leaves.
* `ai/util/utility_scorer.h` – scoring interface (utility composite deferred to later phase).
* NEW (1.4–1.7): scheduler counters (tick_count), deterministic per-agent RNG (`ai_rng.h` xorshift64*), trace ring buffer (`ai_trace.{c,h}`) capturing hashed active path, preorder path serialization for future save/replay.
* Tests: `test_ai_behavior_tree` (foundational) and `test_ai_phase1_scheduler_trace` (trace ring, RNG determinism, path hashing).

Phase 2 (Blackboard & Memory): Added vec2 & timer types, per-entry TTL decay, write policies (Set/Max/Min/Accumulate) for int/float writes, dirty flag API for reactive nodes, and deterministic tick-based decay (`rogue_bb_tick`). Test `test_ai_phase2_blackboard` validates vec2/timer storage, policies, TTL expiry, and dirty flag lifecycle.

Phase 3 (Perception System): New `ai/perception/perception.{c,h}` implements:
* LOS raycast (Bresenham tile walk) with overridable blocking predicate (used in tests)
* Vision cone + distance check (FOV dot vs cos limit + range squared) integrated with LOS
* Hearing event ring buffer (attack / footstep) with loudness radius queries
* Threat accumulation (visible gain/sec) and linear decay + last seen position memory with TTL
* Group alert broadcast elevating nearby agents' threat & seeding memory
* Agent struct (`RoguePerceptionAgent`) tracks facing vector, threat, last seen position & TTL
Unit test `test_ai_phase3_perception` covers LOS blocking/unblocking, FOV inclusion/exclusion, threat gain & decay over time, hearing-based memory seeding, and broadcast radius filtering.

Phase 4 (Behavior Node Library) progress:
* Composites: Parallel added (joins existing Selector/Sequence) + decorators (Cooldown, Retry).
* Conditions: IsPlayerVisible, TimerElapsed, HealthBelow%.
* Actions: MoveTo, FleeFrom, AttackMelee, AttackRanged stub, Strafe (perpendicular lateral movement with alternating direction flag).
* Tactical: FlankAttempt (computes perpendicular flank point), Regroup (movement toward regroup point), CoverSeek (computes cover point opposite player relative to obstacle; moves agent and validates line-of-sight occlusion via projection inside obstacle radius before success).
* Utility Selector: dynamic highest-score child evaluation via per-child scorer callbacks.
Tests: `test_ai_phase4_nodes` (utility, movement, cooldown), `test_ai_phase4_tactical` (strafe duration + direction flip, flank point generation, regroup completion, basic cover flag), and `test_ai_phase4_cover` (full cover seek movement & occlusion validation).
Phase 5 (Initial Enemy Integration): Added per-enemy behavior tree enable/disable API and simple MoveToPlayer tree gated by a feature flag on the enemy struct; unit test `test_ai_phase5_enemy_integration` validates movement toward player when enabled. Performance probe instrumentation pending (Phase 9.1).
Enemy Integration Phase 0 (Data & Runtime Structure Alignment): Introduced JSON-based enemy type definitions (`../assets/enemies/*.json`) with fields for archetype id, tier id, base level offset, and type id; added mapping builder (`rogue_enemy_integration_build_mappings`) producing a validated type table (uniqueness enforced). Extended `RogueEnemyTypeDef` and `RogueEnemy` with integration fields (tier_id, base_level_offset, encounter_id, replay_hash_fragment, modifier id slots, flags) plus a cached final stats block. Added spawn apply helper (`rogue_enemy_integration_apply_spawn`) invoking existing difficulty scaling once at instantiation and storing results. New tests: `test_enemy_json_loader` (JSON loader & field assertions) and `test_enemy_integration_phase0` (mapping uniqueness, stat cache population). Roadmap Phase 0 tasks (0.1–0.5) marked Done; sets foundation for deterministic seed & modifier phases.
Enemy Integration Phase 1 (Deterministic Seeds & Replay Hash): Added encounter seed derivation API (`rogue_enemy_integration_encounter_seed`) implementing world_seed ^ region_id ^ room_id ^ encounter_index, replay hash generator (`rogue_enemy_integration_replay_hash`) summarizing template id + unit levels (modifier ids placeholder for Phase 3), and a debug ring buffer with dump API for last 32 encounters. New test `test_enemy_integration_phase1` validates seed reproducibility (two passes identical) and divergence under different world seed. Roadmap Phase 1 (1.1–1.5) marked Done.
Phase 9.1 (AI Time Budget Profiler): Introduced lightweight per-frame AI timing instrumentation (`ai/core/ai_profiler.{c,h}`) with API to set a millisecond budget, begin a frame, record per-agent tick durations, and snapshot cumulative totals (frame total, max agent, agent count, exceeded flag). New unit test `test_ai_phase9_budget` covers non-exceeded and exceeded budget paths; groundwork for later incremental evaluation (9.2) and LOD behavior (9.3).
Phase 9.2 / 9.3 (Incremental Evaluation + LOD Behaviour): Added `ai/core/ai_scheduler.{c,h}` providing a bucketed frame scheduler and distance-based Level of Detail gating. Enemies only execute their behavior tree when (a) they are within the active LOD radius and (b) their index falls into the current frame's bucket (spreading expensive AI work across frames). Outside the radius they receive a cheap maintenance tick (hook for future status decay). Radius & bucket count are runtime tunables (`rogue_ai_lod_set_radius`, `rogue_ai_scheduler_set_buckets`). Unit test `test_ai_phase9_incremental_lod` validates: near agent movement under bucketed ticking, far agent exclusion, and subsequent movement after expanding the LOD radius.
Phase 9.4 (Agent Pooling & Reinit): Introduced `ai/core/ai_agent_pool.{c,h}` — a slab-based reusable pool for per-enemy AI blackboard wrapper blocks used by behavior tree enabled enemies. Eliminates malloc/free churn when large waves of enemies spawn/despawn or toggle AI feature flags. Integrated into `rogue_enemy_ai_bt_enable/disable` replacing direct allocation. Unit test `test_ai_phase9_agent_pool` exercises repeated acquire/release cycles, verifies peak allocation does not grow after reuse, and asserts absence of leaks (in_use returns to 0 with free list >= peak). Diagnostic APIs: `rogue_ai_agent_pool_in_use`, `rogue_ai_agent_pool_free`, `rogue_ai_agent_pool_peak`.

Phase 9.5 (Stress Test – 200 Agents Pathing): Added large-scale scalability validation via `test_ai_phase9_stress` which spawns 200 enemies, enables their behavior trees, and advances the scheduler in two phases: (1) constrained LOD radius (30 units) verifying only near agents move while far agents remain static, then (2) expanded LOD radius (400 units) confirming deferred far agents begin moving. Test asserts minimum movement counts (>=25 near, >=100 far after expansion), validates scheduler bucket distribution (8 buckets over 128 frames), and ensures the agent pool peak equals the active enemy count (200) without leaks. Provides a regression guard for future pathing / scheduler / LOD changes impacting large waves.

Phase 10.1–10.5 (AI Debug & Tooling): Introduced consolidated debug helpers in `ai/core/ai_debug.{c,h}`:
* Behavior Tree Visualizer `rogue_ai_bt_visualize` outputs an indented ASCII tree for quick inspection in logs or overlays.
* Perception Overlay Primitive Collector `rogue_ai_perception_collect_debug` emits simple line primitives (facing vector + LOS ray) for future on-screen rendering.
* Blackboard Dump `rogue_ai_blackboard_dump` serializes current key/value pairs (ints, floats, vec2, timers) with consistent formatting.
* Trace JSON Export `rogue_ai_trace_export_json` converts the ring buffer of path hashes into chronological JSON array for tooling ingestion.
* Determinism Verifier `rogue_ai_determinism_verify` constructs two trees from a supplied factory, runs them in lockstep, and aggregates FNV-1a path hashes—failing on any divergence. New unit test `test_ai_phase10_debug` validates all helpers.

#### World Generation Phase 1 (Foundations)
* Extended `RogueWorldGenConfig` with macro layout fields (`continent_count`, `biome_seed_offset`).
* Added deterministic multi-channel RNG (macro/biome/micro) via `RogueWorldGenContext` for decorrelated generation layers.
* Introduced chunk coordinate utilities (`ROGUE_WORLD_CHUNK_SIZE=32`, mapping/origin/index helpers) laying groundwork for streaming & caching phases.
* Implemented FNV-1a tilemap hashing (`rogue_world_hash_tilemap`) for future golden snapshot regression tests.
* New unit test `test_worldgen_phase1_foundation` validates chunk mapping edge cases, RNG channel independence & reproducibility, and hash stability/difference on mutation.

#### World Generation Phase 2 (Macro Layout & Biomes)
* Implemented macro continent mask (fbm + radial falloff) producing landmass count (size-filtered) and water/land ratio within bounded range.
* Added elevation normalization (land only) combining continental mask + secondary fbm shaping for smoother gradients.
* Introduced latitude + altitude temperature model and fbm-based moisture field for rudimentary climate approximation.
* Implemented deterministic river carving: peak selection (elev>0.55) with greedy downhill descent marking river tiles until sea level reached.
* Added threshold biome classification (mountain / snow / swamp / forest / grass / water) assigning tile types used later for resource & spawn gating.
* New unit test `test_worldgen_phase2_macro` validates: water ratio bounds, ≥1 continent, river adjacency to ocean, and full determinism (tilemap hash + biome histogram reproducibility for identical seed).

#### World Generation Phase 3 (Biome Descriptors & Registry)
* Introduced data-driven biome descriptor format (`*.biome.cfg`) parsed into `RogueBiomeDescriptor` containing name, music track, vegetation & decoration densities, ambient color, structure & weather flags, and normalized tile palette weights (`tile_*` entries for grass/forest/water/mountain/swamp/snow/river).
* Added dynamic biome registry with directory loader and palette blending utility (`rogue_biome_blend_palettes`) for boundary transitions.
* Implemented secure, deterministic parsing (normalizes weights sum->1, clamps densities 0..1, rejects descriptors with no tile entries).
* Unit test `test_worldgen_phase3_biome_descriptors` covers: successful parse, failure on missing tiles, palette normalization, registry add, and blend normalization.
* Lays groundwork for Phase 4 local terrain perturbation & Phase 5 advanced river/erosion passes by externalizing biome metadata for future modding & hot reload.

### World Generation Phase 4 (Local Terrain & Caves)
Adds micro-scale detailing atop the macro biome layout:
* 4.1 Local Terrain Perturbation: Applied secondary fbm noise using micro RNG channel to subtly convert boundary grass<->forest tiles and erode select mountain tiles, improving natural transitions without altering macro shape.
* 4.2 Cave Cellular Automata: Independent CA seeded only under mountain tiles (respecting configured fill chance & iterations) producing CAVE_WALL / CAVE_FLOOR layering separate from initial macro pass, enabling deterministic interior spaces.
* 4.3 Lava Pockets: Deterministic pocket flood around sampled cave floor centers (radius 1–3) marking `ROGUE_TILE_LAVA` creating environmental hazards for future gameplay interactions.
* 4.4 Ore Veins: Random-walk ("perlin worm" style) vein lines over cave wall cells converting them to `ROGUE_TILE_ORE_VEIN`; direction perturbs with 30% turn chance for organic branching.
* 4.5 Passability Map: Introduced `RoguePassabilityMap` derivation marking walkable tiles (grass/forest/swamp/snow/cave_floor/delta) to support downstream pathing & spawn validation.
* 4.6 Unit Test: `test_worldgen_phase4_local_caves` validates: cave openness ratio within 25–75%, presence of lava pockets & ore veins, passability map deterministic rebuild, and full multi-phase determinism via hash comparison after regeneration.
New tile types: `ROGUE_TILE_LAVA`, `ROGUE_TILE_ORE_VEIN`. Implementation in `world_gen_local.c` using micro RNG channel; roadmap Phase 4 items marked Done.

### World Generation Phase 5 (Rivers & Erosion Detailing)
* 5.1 River Refinement: Noise-guided widening around existing river tiles introducing `ROGUE_TILE_RIVER_WIDE` clusters and converting highly aquatic-adjacent wide tiles into deltas (`ROGUE_TILE_RIVER_DELTA`).
* 5.2 Simplified Erosion: Thermal creep (>=3 lower neighbors) and hydraulic lowering of steep differences; mountain tiles reduced to forest/grass as elevation proxy drops.
* 5.3 Deltas & Estuaries: Post-widen pass marks delta tiles where widened rivers meet large water bodies (>=4 adjacent water tiles).
* 5.4 Bridge Hints: Gap scan function counts candidate short crossings (water span 3–6) for future structure placement; placeholder tile enum `ROGUE_TILE_BRIDGE_HINT` added (not yet painted onto base map, counted logically).
* 5.5 Unit Test: `test_worldgen_phase5_rivers_erosion` ensures deterministic refinement (hash match), presence of widened/delta river tiles, and non-zero bridge hint count.

### World Generation Phase 6 (Structures & Points of Interest)
Introduces overland structure placement and dungeon entrance markers.
* 6.1 Structure Descriptors: Static registry (`world_gen_structures.c`) with id, footprint (w,h), biome bitmask, rarity weight, elevation bounds (0–3 heuristic), and rotation flag. Example baseline set: hut, watchtower, shrine.
* 6.2 Placement Solver: Stochastic sampling (attempt budget = 20x target) using micro RNG channel; per candidate applies: bounds check, min spacing (Poisson-esque), biome mask match, elevation clamp, occupancy rejection (no water/mountain/river in footprint). Successful placements carve border walls (`ROGUE_TILE_STRUCTURE_WALL`) plus interior floors (`ROGUE_TILE_STRUCTURE_FLOOR`) ensuring deterministic footprint tiles.
* 6.3 Multi-piece Assembly: Deferred (single-piece only in this slice); design leaves room for later prefab stitching.
* 6.4 Dungeon Entrances: Deterministic subset selection of placed structures; converts a floor tile near structure base into `ROGUE_TILE_DUNGEON_ENTRANCE` (future linkage to subterranean graph in Phase 7).
* 6.5 POI Index: Deferred – placements array returned; spatial acceleration structure slated for a later optimization phase (Phase 11 or 14). 
* 6.6 Unit Test: `test_worldgen_phase6_structures` validates non-zero placement count, absence of overlap (AABB test), deterministic regeneration (placements & hash), and successful entrance placement count (>=0).
New tile types: `ROGUE_TILE_STRUCTURE_WALL`, `ROGUE_TILE_STRUCTURE_FLOOR`, `ROGUE_TILE_DUNGEON_ENTRANCE`.

### World Generation Phase 7 (Dungeon Generator)
Adds an underground dungeon layer built from a deterministic room graph.
* 7.1 Room Sampling: Non-overlapping rectangular rooms stochastically sampled (size 4–10x4–9) with overlap rejection producing node set.
* 7.2 Corridors & Loops: Nearest-neighbor MST plus percentage-based extra loop edges; L-shaped corridor carving guarantees connectivity while enabling cycles for navigation variety.
* 7.3 Thematic Tagging: Deterministic tagging assigns largest room as TREASURE, the two farthest from the start as ELITE, and small leaf rooms (degree 1, area below average) as PUZZLE. Tags aid future spawn & loot weighting.
* 7.4 Secret Rooms: Probability-driven (configurable chance) selection flags suitable larger rooms as secret; converts a wall to SECRET_DOOR tile for discovery gameplay.
* 7.5 Key/Lock Progression: Subset of rooms gated by LOCKED_DOOR tiles with placed keys guaranteed to appear in earlier reachable rooms ensuring linearized progression without softlocks.
* 7.6 Traps & Hazards: Deterministic interior trap placement up to target count, leveraging micro RNG channel; future safety radius tuning planned.
* 7.7 Validation: Unit test asserts full reachability, minimum loop ratio threshold, non-zero carving, deterministic regeneration (room centers), secret/lock/trap counts non-negative, and tagging invariants (exactly one treasure, at least one elite).
New tile types: `ROGUE_TILE_DUNGEON_WALL`, `ROGUE_TILE_DUNGEON_FLOOR`, `ROGUE_TILE_DUNGEON_DOOR`, `ROGUE_TILE_DUNGEON_LOCKED_DOOR`, `ROGUE_TILE_DUNGEON_SECRET_DOOR`, `ROGUE_TILE_DUNGEON_TRAP`, `ROGUE_TILE_DUNGEON_KEY`.

### Equipment System Phase 7.1 (Defensive Layer – Block Stats)
Introduced first defensive extensions toward Phase 7 goals:
* New affix stats: `block_chance`, `block_value` (bulwark / of_guarding) parsed & rollable.
* Stat cache aggregation wiring plus passive block application when not actively guarding.
* Unit tests: `test_equipment_phase7_defensive` (runtime block reduction) and `test_equipment_phase7_block_affixes` (affix parsing + cache injection).
Forms baseline for upcoming damage conversion, guard recovery, reactive shields, and thorns reflect phases.

### Equipment System Phase 1 (Completed)
* Added expanded equipment slot enum: weapon, offhand, head, chest, legs, hands, feet, ring1, ring2, amulet, belt, cloak, charm1, charm2.
* Implemented data-driven two-handed weapon flag (item definition `flags` bitmask) powering automatic offhand clearing and offhand equip blocking.
* Persisting full equipment slot state in player save component (count + per-slot instance indices) with backward-compatible loader.
* Added cosmetic transmog layer (per-slot override definition index) with API and basic validation (same category requirement).
* Basic smoke test (`test_equipment_phase1_slots`) exercises new API and validates two-hand/offhand guard if a flagged weapon exists in definitions.
* New test `test_equipment_phase1_transmog` validates transmog set/clear and category mismatch rejection.
* Added persistence + atomicity test `test_equipment_phase1_persistence` with dedicated test items (`test_equipment_items.cfg`) ensuring two-hand offhand clearing, swap behavior, and save/load roundtrip.
* Roadmap updated (Phase 1.1–1.7 Done). Note: broader suite has pre-existing unrelated failures; new equipment tests pass. Next: begin Phase 2 stat layering refactor.

### World Generation Phase 8 (Fauna & Spawn Ecology)
Introduces deterministic wildlife/creature spawn sampling decoupled from runtime AI specifics.
* 8.1 Spawn Table Registry: Lightweight in-memory registry keyed by representative biome/dungeon tile (GRASS, FOREST, DUNGEON_FLOOR, etc.) with normal & rare weights per entry.
* 8.2 Density Map: Per-tile float density derives from tile type plus water adjacency dampening (coastal & wetland scarcity), leveraging existing tile categories as elevation proxy.
* 8.3 Hub Suppression: API `rogue_spawn_apply_hub_suppression` clears density inside configurable hub radius (player settlements) and smoothly attenuates outward ring.
* 8.4 Rare Encounters: Basis-point rare roll switches to alternative weight column enabling rarities without separate tables; deterministic via micro RNG channel.
* 8.5 Validation Test: `test_worldgen_phase8_spawns` builds macro map, registers grass/forest tables, builds density, applies hub suppression, samples 200 deterministic spawn queries (collecting rare hits), and replays seed to confirm reproducible first 20 spawn ids.
APIs cover table registration, density build/free, suppression, and position-based spawn sampling with rare flag output.

### World Generation Phase 9 (Resource Nodes)
Implements clustered placement of lootable resource nodes (ores/crystals) with rarity & tool tier metadata.
* 9.1 Descriptors: In-memory registry (id, rarity tier, tool tier requirement, yield range, biome mask) with validation.
* 9.2 Cluster Placement: Deterministic seeding of cluster centers (attempt-constrained sampling) then local jitter within a square radius while enforcing homogeneous biome membership.
* 9.3 Respawn Scheduling: Deferred (runtime system to be added in streaming/runtime phases; current slice static).
* 9.4 Upgrades: Rarity-tier based upgrade roll (5%/10%/18%) applying 1.5x yield and marking upgraded flag for telemetry/balance.
* 9.5 Test: `test_worldgen_phase9_resources` registers copper/iron/crystal descriptors, generates nodes, asserts non-zero count, positive yields, deterministic regeneration (positions & descriptor indices), and upgrade count non-negative.
Public APIs allow descriptor registration, generation with clustering parameters, and upgrade counting for metrics.

### World Generation Phase 10 (Weather & Environmental Simulation)
Adds a deterministic, biome-weighted weather scheduling layer providing foundation for visual/audio/environmental modulation.
* 10.1 Pattern Descriptors: `RogueWeatherPatternDesc` captures id, duration bounds, intensity range, biome mask, and base probability weight. In-memory registry (32 max) with simple append semantics.
* 10.2 Scheduler: `rogue_weather_update` advances a `RogueActiveWeather` state machine tick-by-tick. On expiry or uninitialized state it selects the next pattern via macro RNG weighted choice restricted to biome mask. Each activation seeds a target intensity inside the descriptor range using micro RNG. Intensity eases 5% per tick toward target; fade-out triggers when duration elapses (target set to 0) producing smooth transitions.
* 10.3 Biome Weighting: Patterns whose `biome_mask` excludes the current biome contribute zero weight and are never selected. Weighted roulette wheel across remaining patterns ensures relative proportions loosely reflect configured `base_weight` values.
* 10.4 Environmental Hooks: Initial effects include lighting tint sampling (`rogue_weather_sample_lighting`) applying slight darkening & blue shift proportional to intensity, plus movement speed debuff factor (`rogue_weather_movement_factor`) clamped to 0.5–1.0 range. Particle & audio hooks intentionally deferred until rendering/audio subsystems integrate runtime callbacks.
* 10.5 Validation Test: `test_worldgen_phase10_weather` registers three patterns (clear, rain, storm), simulates 2000 ticks, asserts multiple transitions, verifies probability weighting (rain observations >= clear due to weight 10 vs 5), and confirms determinism by re-running with identical seed & registry and comparing per-pattern observation counts.
APIs added: registry management (`rogue_weather_register`, `rogue_weather_clear_registry`, `rogue_weather_registry_count`), scheduler init (`rogue_weather_init`), update (`rogue_weather_update`), and effect sampling helpers.

### World Generation Phase 11 (Runtime Streaming & Caching)
Introduces a lightweight deterministic chunk streaming manager for progressive world materialization.
* 11.1 Async Queue: Ring-buffer queue (capacity 512) with duplicate suppression storing chunk coordinates awaiting generation.
* 11.2 Time-Sliced Budget: `rogue_chunk_stream_update` consumes up to `budget_per_tick` queued chunks per call, enabling the caller to distribute generation work across frames.
* 11.3 LRU Eviction: Fixed-capacity in-memory cache (configurable) maintains last-access tick; generator evicts least recently accessed chunk upon needing space (records eviction stat).
* 11.4 Persistent Cache: Flag & directory path plumbed but actual serialization deferred (future persistence phase). Current implementation purely in-memory; hashes facilitate future disk naming.
* 11.5 Validation Test: `test_worldgen_phase11_streaming` requests more chunks than capacity, advances updates until cache full, accesses subset to refresh LRU, forces eviction via new request, asserts cache misses > 0, and validates deterministic regeneration by comparing hash of a chunk across a manager tear-down/recreate cycle.
APIs: creation/destruction (`rogue_chunk_stream_create/destroy`), enqueue/request (`rogue_chunk_stream_enqueue/request`), update, retrieval (`rogue_chunk_stream_get`), stats, loaded count, and per-chunk hash exposure.

### World Generation Phase 12 (Telemetry & Analytics)
Initial analytics layer for world generation providing snapshot metrics and anomaly flags.
* 12.1 Metrics Snapshot: `RogueWorldGenMetrics` captures placeholder timing buckets (future instrumentation), counts (continents, river tiles, etc.), and anomalies bitmask.
* 12.2 Anomaly Detection: Current flags include land ratio out-of-bounds (<30% or >55%) and absence of rivers; detection executed in `rogue_world_metrics_collect`.
* 12.3 Heatmap Export: `rogue_world_export_biome_heatmap` writes a flat byte array of tile biome ids for tooling (structure density overlay deferred until POI index implemented).
* 12.4 Validation Test: `test_worldgen_phase12_telemetry` generates macro layout, collects metrics, asserts anomaly correctness (missing rivers triggers flag), exports heatmap, and verifies deterministic equality across regenerated run.
Helpers provided to stringify anomaly list for logging (`rogue_world_metrics_anomaly_list`).

### World Generation Phase 13 (Modding & Data Extensibility)
Introduces the first slice of data-driven hot reload infrastructure focused on biome descriptors.
* 13.1 Hot-Reloadable Descriptor Packs: A pack directory contains a `pack.meta` (declaring `schema_version=`) plus one or more `*.biome.cfg` files. Loading builds a temporary biome registry which, on successful parse & validation, atomically swaps with the active registry (all-or-nothing) ensuring generators never observe partial state.
* 13.2 Versioned Schema & Migration Hooks: Pack loader parses `schema_version` and exposes migration registration API (`rogue_pack_register_migration(old,target,fn)`). Current slice scaffolds callback registry (no multi-step migrations yet) establishing forward compatibility path.
* 13.3 Sandbox Validation / CLI: `rogue_pack_cli_validate(path)` loads a pack and prints success or failure (with error). Lightweight summary API (`rogue_pack_summary`) yields counts for diagnostics & analytics overlays.
* 13.4 Unit Tests: `test_worldgen_phase13_modding` exercises valid load, invalid pack rejection (missing `pack.meta`), hot reload with an additional biome file (verifying atomic swap), summary content, and clearing the active pack (`rogue_pack_clear`).
Implementation Notes: Uses safe file I/O (fopen_s on Windows), platform-specific directory enumeration (Win32 / POSIX), removes unsafe `strcpy`, and avoids compiler-specific constructor attributes in favor of lazy initialization. Roadmap marks Phase 13 items Done (future extension will incorporate structures/resources & multi-step migration application).

### World Generation Phase 14 (Optimization & Memory – Progress)
Performance scaffolding, memory pooling, and first parallel pass:
* 14.1 Transient Arena Allocator: Linear bump arena (`RogueWorldGenArena`) with create/destroy/alloc/reset/usage APIs integrated into macro temp buffers (continent/elevation/temperature/moisture) when a global arena is provided (`rogue_worldgen_set_arena`). Fallback malloc path retained for isolated calls.
* 14.2 SIMD Toggle & Batched Noise Path: Optimization toggle (`rogue_worldgen_enable_optimizations(simd,parallel)`) + batched value/fbm sampling (4-lane). Current stub still invokes scalar per lane to guarantee deterministic hashes; vector kernel insertion point (SSE2) prepared along with accessor APIs. Future: real vector math + erosion SIMD.
* 14.3 Parallel River Carving (Initial): Implemented deterministic multi-threaded river carving on Windows when parallel flag enabled. Collects river start candidates (ordered selection), partitions the list into contiguous chunks, and launches a thread per chunk (CreateThread). Each worker uses identical carve logic without shared mutable global state aside from the destination map (tile writes are disjoint per river path except benign overlaps). Non-Windows or disabled flag falls back to sequential path. Biome classification remains single-thread (planned extension).
* 14.4 Benchmark Harness: `rogue_worldgen_run_noise_benchmark(w,h,&bench)` measures scalar vs SIMD FBM sampling and (MSVC) writes a baseline file for upcoming regression guard. Tolerance comparison & CI gating logic not yet implemented.
* 14.5 Unit Test: `test_worldgen_phase14_optimization` validates benchmark invariants, SIMD toggle path, and keeps output deterministic (SIMD still scalar-lane). Additional determinism test for parallel vs sequential generation planned.
Next steps: broaden arena adoption (caves/erosion scratch), true vectorized noise + erosion kernels, optional parallel biome classification (thread pool abstraction), performance regression guard (baseline load + % tolerance + failure), and hash-based determinism test comparing parallel enabled/disabled outputs.

Phase 11.1–11.5 (AI Testing & QA Expansion): Added comprehensive quality gates around core AI behaviours.
* Core Node Edge Tests: `test_ai_phase11_core_nodes` exercises Selector/Sequence short‑circuiting, Parallel mixed status aggregation, Utility selector tie‑break determinism, cooldown boundary reset, and retry decorator exhaustion/reset semantics.
* Blackboard Fuzz: `test_ai_phase11_blackboard_fuzz` performs 5k deterministic pseudo‑random operations (Set/Max/Min/Accumulate/int+float/timer/vec2) mirrored against an in‑memory model to ensure policy invariants and capacity bounds (no overflow of 32 entries).
* Scenario Script: `test_ai_phase11_scenario` simulates a patrol→detect(chase)→lose track→resume patrol loop using a handcrafted patrol node + visibility toggle, validating state transitions and movement deltas across phases.
* Repro Trace: `test_ai_phase11_repro_trace` constructs dual RNG‑branch trees verifying identical seeds reproduce byte‑identical serialized active paths & FNV hashes for 40 ticks, then confirms divergence under a different seed.
* Performance Guard: `test_ai_phase11_perf_guard` simulates profiler frames under and over a configured budget ensuring `budget_exceeded` flag accuracy and correct agent count accumulation. Collectively these tests harden future refactors (pathing, combat hooks) against silent behavioural drift or hidden perf regressions.


#### Combat & Skills
* Skill system supports active & passive examples (fireball, dash, passives) with forced short cooldowns under test macro.
* Combat Attack Registry (Phase 1.1–1.3 partial): classless weapon archetypes (light, heavy, thrust, ranged, spell_focus) with static attack chains + branch queue logic (1.4/1.6 partial), scaling coefficients & damage type fields (1.5 partial), extended AttackDef fields (poise_cost, cancel_flags, whiff_cancel_pct, status buildup scaffolds) and validation tests (`test_combat_attack_registry`, `test_combat_chain_branch`, `test_combat_scaling_coeffs`, `test_combat_whiff_cancel`, `test_combat_block_cancel`, `test_combat_drift_timing`). Implements hit / whiff / block early cancel logic, late-chain grace window (<130ms idle chain continuation), and a drift-resistant double accumulator for precise phase timing.
* Phase 2 (Damage & Mitigation – initial slice): Added `RogueDamageType` enum (physical, bleed, fire, frost, arcane, poison, true). Player & enemy structs extended with armor + elemental resist fields (fire/frost/arcane) plus player penetration stats (flat & percent placeholders). Implemented `rogue_apply_mitigation_enemy()` applying order: flat armor -> percent resist -> minimum damage floor (>=1) and reporting overkill. Integrated into strike pipeline (damage numbers now show post‑mitigation values). Introduced `RogueDamageEvent` struct (not yet emitted) and unit test `test_combat_mitigation` validating flat reduction precedes percent, fire resist 50% behavior, and minimum floor under extreme armor.
* Multi-hit active windows (Phase 1A.2 partial): data-driven window array per attack with per-window cancel flag override, per-window damage_mult scalar, status buildup scaffold fields, duplicate hit prevention mask; examples: light_3 (two pulses) + heavy_2 (three pulses with intentional 0‑50 / 40‑80 overlap) validated by `test_combat_multi_hit`, `test_combat_heavy_multi_window`, and overlap edge coverage in `test_combat_window_boundary`. Per-window begin/end animation events emitted (`test_combat_events`) and consumable via `rogue_combat_consume_events` (non-destructive to duplicate prevention masks). Helper `rogue_attack_window_frame_span` now backed by a static frame cache built once at startup for O(1) subsequent queries (preps for external frame data tooling). 
* Damage numbers decoupled from combat logic; spawn & lifetime tested for determinism.
* Phase 2 Damage Pipeline Advancements: Added dedicated physical percent resist (`resist_physical`) applied after flat armor with a diminishing returns curve (`eff = p - (p*p)/300`, capped) to curb extreme stacking; implemented player armor penetration (flat then percent of original armor) applied pre-mitigation; introduced global `RogueDamageEvent` ring buffer emitting an event per hit (tracks raw, mitigated, overkill, crit & execution flags) with consumer APIs (`rogue_damage_events_snapshot`, `rogue_damage_events_clear`); crit layering refactor with toggle (`g_crit_layering_mode`) supporting pre- (legacy) vs post-mitigation application; execution triggers (low health ≤15% or overkill ≥25% max health) mark finishing blows. New tests: `test_combat_mitigation` (armor + elemental + floor), `test_combat_penetration` (penetration ordering), `test_combat_damage_events` (event emission & invariants), `test_combat_crit_layering` (crit layering invariants), `test_combat_execution` (execution trigger flags), `test_combat_phys_resist_curve` (diminishing returns monotonicity & slope taper).
* Phase 3.1 Foundations: Introduced separate Guard and Poise meters (player + enemy structs) laying groundwork for directional blocking, chip damage, stagger & hyper armor logic in later Phase 3.x/4.x; initialization & clamping added, with unit test `test_combat_phase3_guard_poise` validating presence and basic bounds.
* Phase 3.2 / 3.5 Encumbrance & Stamina Scaling: Added encumbrance (weight) tracking with capacity and tier calculation (light/medium/heavy/overloaded) influencing movement speed and stamina regeneration multipliers (1.0 / 0.92 / 0.80 / 0.60). Unit test `test_combat_phase3_encumbrance` verifies monotonic decrease in regen across tiers. Future work will extend to dodge i-frame and roll distance adjustments.
* Phase 3.3 Poise & Stagger: Implemented per-attack `poise_damage` application to enemy poise; depletion triggers a simple stagger state (flag + timer) that auto-recovers to 50% poise and blocks repeated immediate staggers. Poise passively regenerates while not staggered. Unit test `test_combat_phase3_poise_stagger` validates depletion, stagger flag set, timed recovery & partial poise refill.
* Phase 3.4 Hyper Armor (Plumbing): Added `ROGUE_WINDOW_HYPER_ARMOR` window flag and combat state exposure (`current_window_flags`). Data authoring not yet assigning the flag, but pipeline supports future immunity to incoming poise damage during flagged frames. Unit test `test_combat_phase3_hyper_armor` confirms flag plumbing path.
* Phase 3.6 Defensive Weight Soft Cap: Implemented post-mitigation soft cap on stacked physical reduction (armor + physical resist) using threshold 65%, excess scaled by 0.45 slope, absolute cap 85%, ignoring trivial hits (raw<100). Ensures extreme stacking yields diminishing returns. Unit test `test_combat_phase3_def_softcap` validates bounded reduction.
* Phase 3.8 Guard & Chip: Added directional guard system (frontal cone dot>=0.25) with guard meter drain (passive + on block) and chip damage (20% with min 1) when block not perfect. Integrated into enemy melee pipeline; test `test_combat_phase3_guard_block` validates perfect guard (0 dmg), regular chip, and rear attack bypass.
* Phase 3.9 Perfect Guard: First 140ms of guard counts as perfect window; successful perfect guard negates damage, refunds guard meter, restores bonus poise. (Attacker posture break hook reserved.)
* Phase 3.10 Poise Regen Curve: Added delayed (650ms) non-linear poise regeneration (quadratic acceleration when low) via `rogue_player_poise_regen_tick`; test `test_combat_phase3_poise_regen_curve` confirms stronger early recovery vs high-poise state.
* Phase 3.11 Test Expansion (Complete): Added hyper armor poise immunity test (`test_combat_phase3_hyper_armor_immunity`); guard block test covers cone + perfect window; guard cone edge fuzz test (`test_combat_phase3_guard_cone_edge`) ensures inclusive threshold handling; stagger threshold precision (`test_combat_phase3_poise_stagger_precision`) validates exact depletion trigger; authored hyper armor window on `heavy_2` mid window (flag `ROGUE_WINDOW_HYPER_ARMOR`) granting transient poise immunity only during flagged frames.
* Phase 4.1–4.3 Reactions & I-Frames: Introduced unified player reaction system (light flinch, stagger, knockdown, launch) with duration table and automatic selection: poise break => stagger; damage >=80 => knockdown; damage >=25 => flinch. Added `iframes_ms` granting complete damage + poise immunity while active. Test `test_combat_phase4_reactions` validates reaction selection determinism, timer expiry, stagger via poise depletion, knockdown via high damage, and i-frame immunity.
* Phase 4.4 Crowd Control: Added stun/root/slow/disarm timers & gating; stun & disarm suppress attack buffering, root halts movement but preserves attack usage, slow applies capped (≤95%) speed reduction. Test `test_combat_phase4_cc` validates suppression and allowed actions per CC type.
* Phase 4.5 Reaction Cancel Windows & Directional Influence: Implemented early recovery windows per reaction type (light 40–75%, stagger 55–85%, knockdown 60–80%, launch 65–78%) via `rogue_player_try_reaction_cancel`, rejecting attempts outside bounds. Directional Influence (DI) during active reactions accumulates small movement intent vectors clamped by per-reaction radius caps (0.35 / 0.55 / 0.85 / 1.0). Player struct tracks original reaction duration and DI accumulators; new test `test_combat_phase4_reaction_cancel_di` verifies early cancel gating and DI clamp.
* Phase 4.6 Test Expansion: Added i-frame overlap protection helper (`rogue_player_add_iframes`) using max replacement (no additive stacking) and test `test_combat_phase4_iframe_overlap` ensuring grants don't accumulate and immunity persists until timer expiry.
* Phase 5.1 Hitbox Primitives: Introduced foundational spatial volume types (`RogueHitbox` union) for future hit detection: capsule (segment+radius), swept arc (angle sector with optional inner radius), multi-segment chain (series of capsules), and projectile spawn descriptor (count, spread, speed) with basic intersection helpers for point testing (capsule/arc/chain) and projectile spread angle helper. Unit test `test_combat_phase5_hitbox_primitives` validates geometry inclusion/exclusion and evenly distributed projectile angles. These are pure POD utilities (no allocations) ready for external authoring (5.2) and broadphase culling integration (5.3).
* Phase 5.2 Authoring & 5.3 Broadphase: Added lightweight JSON subset parser (`hitbox_load.c`) to load arrays of hitbox definitions (capsule / arc / chain / projectile_spawn). Parser ignores unknown keys for forward compatibility and exposes memory + file APIs. Implemented broadphase collection helper computing primitive AABB bounds, pruning points before precise intersection. New test `test_combat_phase5_hitbox_authoring_broadphase` validates parsing, projectile spread angle endpoints/center, and correct candidate collection (inclusion + exclusion) for a capsule.
* Phase 5.4 Friendly Fire / Team Filtering: Introduced `team_id` on `RoguePlayer` & `RogueEnemy`; strike loop now skips same-team entities (prevents ally damage). Validated via new unit test `test_combat_phase5_team_obstruction` (ally health unchanged while enemy takes damage).
* Phase 5.5 Terrain Obstruction Attenuation: Added simple tile DDA trace between attacker and target; encountering a blocking tile applies 60% damage scaling (rounded) simulating partial wall / obstacle dampening. Integrated pre-mitigation; covered by obstruction portion of `test_combat_phase5_team_obstruction`. (Future: distinct clank SFX / spark event hook.)
* Phase 5.6 Lock-On Subsystem: Added optional directional assist (soft aim) with radius-based acquisition (nearest + facing bias), angular cycling (clockwise / counter-clockwise) with 180ms switch cooldown, directional override feeding strike arc vector, and automatic invalidation (target death or leaving radius with 1.25x hysteresis). Implemented in `lock_on.c/h` and validated by unit test `test_combat_phase5_lock_on` (acquisition, forward/backward cycle, cooldown tick, direction vector, invalidation). Further multi-target damage assist + obstruction interaction tests scheduled under Phase 5.7.
* Phase 5.7: Completed lock-on test suite: acquisition/cycling/invalidation + switch cooldown gating + obstruction attenuation (55-65% ratio) + multi-target angular ordering (`test_combat_phase5_lock_on`, `_obstruction_latency`, `_multitarget`).
* Phase 6.1 / 6.3 (Initial Advanced Offense/Defense): Charged attack subsystem (hold up to 800ms for 2.5x cap, overcharge clamp) + dodge roll (400ms i-frames, stamina cost 18, slight poise restore). Test: `test_combat_phase6_charge_and_dodge`.
* Phase 6.4 / 6.5 (Positional & Reactive Windows): Backstab detection (rear cone dot<=-0.70 within 1.5 tiles, 650ms cooldown) grants 1.75x one-shot multiplier; parry (160ms window) → riposte (650ms) consumption adds 2.25x pending multiplier + i-frames on parry success. Tests: `test_combat_phase6_parry_backstab_riposte`, `test_combat_phase6_backstab_guardbreak_riposte_bonus`.
* Phase 6.2 / 6.6 / 6.7 (Aerial, Guard Break, Projectile Deflect): Aerial flag (placeholder) adds +20% damage & 120ms landing lag to recovery. Guard break primes forced crit + 1.50x one-shot multiplier & riposte-style window; consume API clears readiness. Projectile deflection reflects direction during parry/riposte/guard-break windows. Tests: `test_combat_phase6_aerial_deflect_guardbreak`, `test_combat_phase6_backstab_guardbreak_riposte_bonus`.
* Phase 7.1 – 7.7 (Weapons, Armor Weights, Stances, Infusions, Familiarity, Durability, Expanded Tests): Weapon table (`weapons.c`) with base + STR/DEX/INT scaling, stamina & poise multipliers, durability max. Armor system (`armor.c/h`) adds light/medium/heavy per-slot gear with weight → encumbrance tier aggregation, armor/resists/poise bonuses, and regen modifiers. Stances adjust damage, stamina, poise damage plus frame data (agg -5% windup/-3% recovery, def +6% windup/+8% recovery). Infusions (`infusions.c`) convert % base to elemental & add bleed/poison buildup with per-component mitigation now emitted as distinct damage events + summary event. Familiarity grants up to +10% damage (soft cap enforced). Durability reduces weapon contribution below 50% linearly to 70% at 0 (also scales poise). Tests: `test_combat_phase7_weapon_stance_familiarity`, `test_combat_phase7_infusions_durability`, `test_combat_phase7_infusion_split_events`, `test_combat_phase7_stance_frame_timing`, `test_combat_phase7_familiarity_cap`, `test_combat_phase7_encumbrance_mobility_curve`, `test_combat_phase7_damage_event_components`, `test_combat_phase7_armor_weight_classes`.
* Frame-accurate combo / crit / buffer chain tests ensure animation & timing regressions surface quickly.
* Phase 1 (in progress): Added extended skill metadata (tags, charges, recharge timers, deterministic per-activation RNG seeding) and foundational unit test (`test_skills_phase1_basic`).
* Action Point (AP) economy core (Phase 1.5): player AP pool (scales with DEX), passive regeneration, per-skill `action_point_cost` gating, soft throttle (temporary slowed regen after large spend), and unit tests (`test_skills_ap_economy`, `test_ap_soft_throttle`).
* EffectSpec (Phase 1.2 partial): minimal registry + STAT_BUFF kind applied via skill `effect_spec_id`; unit test (`test_effectspec_basic`) validates buff application.
* Channel Tick Scheduler (Phase 1A.5 partial): basic fixed 250ms quantum ticks for channel skills with unit test (`test_channel_ticks_and_buffer`); future drift correction & dynamic interval planned.
* Input Buffering (Phase 1A.1 partial): per-skill `input_buffer_ms` allows next cast/instant skill queued during an active cast to auto-fire immediately on completion; validated by `test_input_buffer_cast`.
* Adaptive Haste (Phase 1A.3 partial): temporary buff magnitude scales cast completion speed and channel tick frequency (prototype using existing POWER_STRIKE buff); verified by `test_haste_cast_speed`.
* Cast Weaving (Phase 1A.2 partial): `min_weave_ms` enforces spacing between identical cast skill activations; tested in `test_cast_weave_and_cancel`.
* Early Cancel (Phase 1A.4 partial): `rogue_skill_try_cancel` allows terminating a cast after a minimum progress threshold applying scaled effect (`partial_scalar`); covered by `test_cast_weave_and_cancel`.

#### UI System (Phases 1–3 Complete)
* Phase 1 Core: Context, deterministic RNG channel, frame begin/end, simulation snapshot pointer (read-only), text serialization + FNV-1a diff hash, per-frame arena allocator (text duplication helper).
* Phase 2 Primitives & Interaction: Panel/Text/Image/Sprite/ProgressBar plus Button/Toggle/Slider/TextInput with focus & hot/active indices; layout containers (Row/Column/Grid helper/Layer), scroll container (wheel delta), tooltip delayed hover spawn, declarative DSL macros, widget ID hashing (label->FNV1a lookup), and directional navigation graph (Phase 2.8) using spatial heuristics (nearest neighbor with axis bias + linear wrap fallback). Tests: `test_ui_phase2_primitives`, `test_ui_phase2_interactive`, `test_ui_phase2_layout_id`, `test_ui_phase2_scroll_tooltip_nav`.
* Phase 3 Input Abstraction & Interaction Model: Completed input routing & modal gating (modal index forces focus + event isolation), controller analog axis navigation (threshold + shared repeat timing), clipboard + IME stubs (paste integrated in TextInput), key repeat system (initial delay + interval for arrows/tab/activate & controller axes), chorded shortcuts (two-step Ctrl+K, X style with timeout), and deterministic input replay (record + playback) all validated by `test_ui_phase3_input_features`.
* Phase 4.1 Inventory Grid (Minimal Slice): Added `rogue_ui_inventory_grid` helper producing a container panel plus one panel per visible slot (count overlay text for occupied slots). Deterministic serialization confirmed by unit test `test_ui_phase4_inventory`.
* Phase 4.2 Drag & Drop: Added drag state (begin/end events), slot swapping (including empty target), and event queue API (`rogue_ui_poll_event`). Covered by expanded unit test `test_ui_phase4_drag_stack` (drag begin/end, swap correctness, drag to empty).
* Phase 4.3 Stack Split Modal: CTRL+click opens split modal (half default), mouse wheel adjusts quantity (clamped 1..n-1), Activate key applies creating new stack in first empty slot, mouse release outside without activation cancels. Events emitted: OPEN/APPLY/CANCEL (validated in `test_ui_phase4_drag_stack`).
* Phase 4.4 Context Menu: Right-click on occupied slot opens menu (Equip/Salvage/Compare/Cancel). Up/Down navigates, Activate selects (emits CONTEXT_SELECT) or Cancel closes (CONTEXT_CANCEL). Unit test `test_ui_phase4_context_menu` validates open, selection (Compare), and cancel via outside click.
* Phase 4.5 Inline Stat Delta Preview: Hovering an occupied slot produces a compact panel to the side showing a simple damage value and delta (colored green/red/gray). Emits STAT_PREVIEW_SHOW when a new slot hover begins and STAT_PREVIEW_HIDE when leaving inventory items. Covered by new unit test `test_ui_phase4_stat_preview`.
* Phase 4.6 Rarity Theming: Inventory grid now applies rarity-colored outer borders and count text (mapping item_id % 5 -> rarity tiers) to visualize item quality; unit test `test_ui_phase4_rarity_colors` validates presence of all tier colors.
* Phase 4.7 Vendor Restock Timer: Vendor panel displays progress bar with remaining seconds (ETA) until automatic restock; helper `rogue_vendor_restock_fraction()` (unit test `test_ui_phase4_vendor_restock`) ensures fraction calculation correctness.
* Phase 4.8 Transaction Confirmation: Vendor purchases now require confirmation modal (ENTER to open/accept, ESC to cancel) showing item name & price with green/red affordability tint and flashing insufficient funds overlay; unit test `test_ui_phase4_vendor_transaction` validates modal lifecycle and purchase success after funding.
* Phase 4.9 Durability Thresholds: Equipment panel displays a durability bar with color buckets (green >=60%, amber 30–59%, red <30%) and flashing warning icon (!) when critical; helper `rogue_durability_bucket` and test `test_ui_phase4_durability_thresholds` validate classification logic.
* Phase 4.10 Radial Selector: Controller-friendly radial menu (up to 12 wedges) with analog stick angle -> selection mapping (index 0 at Up, clockwise ordering). Keyboard arrows cycle as fallback. Events emitted: RADIAL_OPEN, RADIAL_CHOOSE, RADIAL_CANCEL. Unit test `test_ui_phase4_radial_selector` verifies open event, axis-driven selection, and choose event consistency.
* Phase 5.1 Skill Graph (Initial): Added zoomable, pannable skill graph API with quadtree culling and node emission (base icon panel placeholder, rank pip bars, synergy glow ring). Unit test `test_ui_phase5_skillgraph` validates visible subset emission & glow presence.
* Phase 5.2 Skill Graph Layering: Implemented full node layering (background panel, synergy glow underlay 0x30307040u, rank ring, icon sprite placeholder sheet=0, per-rank pip bars with fill overlay, rank text) replacing earlier partial slice.
* Phase 5.3 Skill Graph Animations: Added rank allocation pulse (280ms scale+fade) and spend flyout animation (600ms upward fade) managed via arrays `skillgraph_pulses[32]` / `skillgraph_spends[32]` advanced each frame in `rogue_ui_begin`. Unit test `test_ui_phase5_skillgraph_anim` validates spawn + decay lifecycle.
* Phase 5.4 Synergy Aggregate Panel (Plumbing): Context flag + enable API; placeholder (non-rendering) currently tests integration path for future stat aggregation.
* Phase 5.5 Tag Filtering: Nodes carry tag bitmask (fire/movement/defense). Filter mask skips non-matching nodes during quadtree emission.
* Phase 5.6 Build Export/Import: Text format `icon:rank/max;tags` allows snapshotting & restoring rank distribution + tags (bounds-checked). Helper APIs `rogue_ui_skillgraph_export` / `rogue_ui_skillgraph_import`.
* Phase 5.7 Undo Buffer: Allocation API records previous rank (stack up to 64). `rogue_ui_skillgraph_undo` reverts last allocation and leaves animation pipeline intact.
* Phase 8 Animation & Transitions: Added easing library (cubic in/out/in-out, spring, elastic-out), entrance/exit scale+alpha transitions, button press spring pulse, keyframe timeline system (scale & alpha) with interrupt policies (REPLACE/IGNORE; APPEND reserved), global animation time dilation (time scale), and reduced-motion duration shortening.
* Phase 9 Performance & Virtualization: Added per-phase timing APIs (begin/end/query) with frame/update/render breakdown, dirty rectangle union + kind classification (structural vs content changes), lightweight glyph cache with LRU compaction + hit/miss metrics, regression guardrails (baseline + % threshold + auto-baseline averaging), virtual list helpers, and completed inventory grid row-based virtualization + scrolling (`test_ui_phase9_virtual_inventory`).
* Phase 13 Inventory UI & Management: Added `inventory_ui.c/h` module bridging core aggregated counts to UI grid. Supports slot array build with stack counts, sorting modes (name, rarity descending, category, count descending), category bitmask + min rarity filtering, equip helper (creates instance if none active), salvage & drop context actions (inventory decrement + ground spawn), and persisted sort mode (`g_app.inventory_sort_mode`) serialized in player component section (save format v9 tail-safe backward compatible). Unit test `test_inventory_ui_phase13` validates build ordering, count sort, and rarity filter behavior.
* Phase 14 Equipment & Stat Integration: Expanded equip slot model (weapon + head/chest/legs/hands/feet). Category gating enforces armor vs weapon. Stat cache pipeline now integrates weapon affix flat damage bonuses, sums armor base_armor across equipped pieces into EHP heuristic ((HP + armor*2) * vitality scalar), and retains agility->dexterity aggregation. Unit test `test_equipment_phase14_extended` validates DPS > 0 and that equipping armor increases EHP.
* Phase 16 Multiplayer / Personal Loot: Completed full feature slice. Per-item ownership (`owner_player_id`) + loot mode enum (shared/personal). Need/Greed system with session begin/choose/resolve APIs (`rogue_loot_need_greed_begin/choose/resolve/winner`) prioritizes NEED over GREED, deterministic LCG-based 400–699 (greed) / 700–999 (need) roll ranges, and locks the instance (pickup & trade gated) until resolution. Ownership assigned to winner (forcing temporary personal mode for assignment). Trade validation now rejects self-trade, non-owner attempts, and locked instances. Anti-duplication safeguard: unresolved need/greed session sets logical lock consumed by pickup loop (`rogue_loot_instance_locked`). Tests: `test_loot_phase16_personal_mode` (ownership gating + trade) and `test_loot_phase16_need_greed_trade` (need vs greed priority, pass handling, post-resolution trade, rejection paths).
* Phase 17 Performance & Memory: Added `loot_perf.c/h` module providing object pool for affix roll scratch buffers with metrics (acquires, releases, peak usage) and SIMD (SSE2) accelerated weight summation path (with scalar fallback) tracking separate counters. Expanded profiling harness (17.3) adds high-resolution nanosecond timing for weight summations and full affix roll sections (`weight_sum_time_ns`, `affix_roll_time_ns`) via `rogue_loot_perf_affix_roll_begin/end`. Implemented cache-friendly item definition indexing (17.4) using an open-addressed FNV-1a hash table rebuilt after config loads; fast lookup API `rogue_item_def_index_fast` falls back to linear scan on miss. Unit tests `test_loot_phase17_index` and `test_loot_phase17_perf` cover index correctness, pool lifecycle, and SIMD metrics. Incremental serialization Phase 17.5 now extends incremental mode with record-level inventory diff metrics: consecutive saves in incremental mode count reused vs rewritten item records (`rogue_save_inventory_diff_metrics`). Test `test_loot_phase17_5_diff` validates first-save rewrite, unchanged reuse, and single-record modification paths.
* Phase 18 Analytics: Initial slice plus advanced features. Base module provides drop event ring buffer (512 capacity) and JSON export summarizing event count & rarity distribution (APIs: `rogue_loot_analytics_record`, `rogue_loot_analytics_recent`, `rogue_loot_analytics_export_json`; test `test_loot_phase18_analytics`). New (18.3) rarity distribution drift detection with configurable baseline fractions or counts (`rogue_loot_analytics_set_baseline_counts`, `rogue_loot_analytics_set_baseline_fractions`) and relative deviation threshold (`rogue_loot_analytics_set_drift_threshold`) producing per-rarity flags via `rogue_loot_analytics_check_drift`. New (18.4) session summary struct `RogueLootSessionSummary` aggregating totals, counts, duration, drops/min, and drift OR (`rogue_loot_analytics_session_summary`). New (18.5) positional heatmap (32x32 grid) APIs: `rogue_loot_analytics_record_pos`, `rogue_loot_analytics_heat_at`, `rogue_loot_analytics_export_heatmap_csv`. Advanced test `test_loot_phase18_3_4_5_advanced` validates drift alert triggering, summary values, and heatmap export.
* Phase 19.1 QA – Loot Table Fuzz: Introduced dedicated fuzz harness `test_loot_phase20_1_fuzz_tables` generating 1200 randomized table lines (well‑formed, malformed, long ids, random noise, empty, comments) with mixed valid/invalid item ids, negative & zero weights, reversed quantity ranges, and varied rarity min/max (including negatives). Ensures loader never crashes, respects table/entry caps (`ROGUE_MAX_LOOT_TABLES`, `ROGUE_MAX_LOOT_ENTRIES`), clamps invalid fields (negative weights → 0 ignored, qmax<qmin corrected), and tolerates malformed segments gracefully. Provides early regression shield before large statistical sampling (20.2/20.3). 
* Phase 20.2–20.3 QA – Statistical & Rarity Regression Harness: Added `test_loot_phase20_2_3_stats_regression` performing 100k rolls on `ORC_BASE` for `long_sword` rarity band (0–2), asserting each rarity appears ≥1 time and empirical frequencies stay within ±0.15 absolute of uniform (≈0.33). Early warning guard against unintended sampler bias from future dynamic weighting, pity, or floor adjustments.
* Phase 21.2 Tooling – Item Definition Validation: Introduced lightweight content validation API `rogue_item_defs_validate_file` plus unit test `test_loot_tooling_phase21_2_validation` detecting malformed CSV lines (insufficient fields) and returning their line numbers for editor integration. Non‑destructive (no registry mutation) enabling external batch validation workflows.
* Phase 21.1 Tooling – Spreadsheet Converter: Added `rogue_item_defs_convert_tsv_to_csv` utility and unit test `test_loot_tooling_phase21_1_convert` converting designer-maintained TSV exports into game CSV format (ignores blank/comment lines) for streamlined content pipeline.
* Phase 21.3 Tooling – Affix Range Preview: Added `rogue_affixes_export_json` producing compact JSON array of all loaded affix ids, type, stat enum, min/max rolls, and per‑rarity weights for external editor / preview UI. Unit test `test_loot_tooling_phase21_3_affix_json` verifies JSON contains expected ids and min/max fields.
* Phase 21.4 Tooling – Rarity Rebalance: Added `rogue_rarity_rebalance_scales` (current vs target rarity counts -> multiplicative scales) and `rogue_rarity_rebalance_export_json` for editor scripting; unit test `test_loot_tooling_phase21_4_rebalance` validates scale math & JSON.
* Phase 21.5 Tooling – Auto Sort Item Defs: Added `rogue_item_defs_sort_cfg` to alphabetize item definition config lines for deterministic diffs; unit test `test_loot_tooling_phase21_5_sort` ensures ordering & header preservation.
* Phase 22.1–22.5 Security: Added `loot_security.c/h` providing `rogue_loot_roll_hash` (FNV1a-32 over table/seed/results) for verification logs, optional seed obfuscation helper (`rogue_loot_obfuscate_seed` with enable flag), lightweight config file hash snapshot + verify APIs (`rogue_loot_security_snapshot_files`, `rogue_loot_security_verify_files`), server authoritative verification stub (`rogue_loot_server_verify` with mode toggle) and rolling rarity spike anomaly detector (`rogue_loot_anomaly_config/record/flag`). Unit tests `test_loot_phase22_security` and `test_loot_phase22_server_anomaly` validate hashing, obfuscation toggle, snapshot/verify, server verification pass/fail, and spike detection window behavior.
* Phase 23.1 Documentation – Loot Architecture: Added `docs/LOOT_ARCHITECTURE.md` detailing phase layering, data flow, determinism contract, module boundaries, and extension guidelines (serves as contributor on‑ramp for loot changes).
* Phase 23.2 Documentation – Rarity & Affix Style Guide: Added `docs/LOOT_RARITY_AFFIX_STYLE.md` codifying naming, weighting, range progression, and balancing workflow for new or adjusted affixes; supports consistent content authoring & review.
* Cleanup 24.5 (incremental): Added initial bounds assert/guard pass in affix subsystem paving path for broader const-correctness tightening.
* Cleanup 24.2: Legacy adhoc debug flag scan completed; standardized structured `loot_logging` levels retained, no obsolete defines remain.
* Cleanup 24.4 (begin): Split affix gating / selection from `loot_generation.c` into `loot_generation_affix.c` reducing monolithic file size and clarifying responsibilities.
* Phase 20.4 QA – Save/Load Round‑Trip (Affix Items): New persistence integrity test `test_loot_phase20_4_persistence_roundtrip` generates a diverse inventory (90 items with prefixes/suffixes, enchant_level, durability), performs full save + reload, and asserts 100% field equality (definition id, quantity, rarity, affix indices/values, durability, enchant) proving backward‑compatible reader correctness for extended item record size.
* Phase 20.5 QA – Ground Item Merge Stress: New stress harness `test_loot_phase20_5_merge_stress` spawns large clusters of similar items near the same coordinates to force merge logic; asserts merged stack count reduction, absence of pool overflow, and deterministic final active instance tally, guarding against future spatial tolerance or regression bugs in merge radius logic.

#### Skill Graph Runtime Preview (Experimental)
Press `G` in-game to toggle a prototype overlay rendering the new UI skill graph system (separate from the legacy skill tree `K`).

Asset Placement: Add a sprite sheet at `assets/skill_icons.png` containing a uniform grid of icons (e.g., 8 columns x 4 rows = 32 icons, each 32x32). Icon IDs (0..N-1) map to frames in sheet id 0 (frame extraction hook pending). Until slicing is wired, panels + text still display (sprites may be blank if sheet not yet integrated).

Demo Layout: The runtime builds a radial distribution of placeholder nodes with sample ranks, synergy flags, and tag bits to exercise quadtree culling + layered emission (background, glow, ring, icon placeholder, pip bar, rank text, pulse/spend animation layers).

Next Steps: Bind allocation/undo to inputs, integrate real skill definitions, implement sprite sheet frame slicing, mouse hover + selection, panning/zoom interaction, and synergy aggregate panel rendering.

#### Loot Foundations (Phases 1–4)
* Item definition parser tolerant to optional later-added rarity column (backward compatibility).
* Loot tables support weighted entries + quantity range (min..max) with deterministic LCG-driven batch roll.
* Ground item pool: fixed-cap static array; O(1) spawn + simple scan for free slot; stack merge radius squared check; no dynamic allocation.
* Persistence round-trip ensures inventory & instances survive reload; affix fields appended later without format breakage.
* NEW (Persistence Phase 2 tooling): Migration runner with rollback + metrics (steps, ms) and tests (ordering determinism, chain, metrics).
* NEW (Persistence Phase 3): Binary format upgraded to v3 TLV section headers (uint16 id + uint32 size) with backward compatibility migration (tests: `test_save_v3_tlv_headers`, `test_save_v2_backward_load`). Lightweight JSON export (`rogue_save_export_json`) lists descriptor + section ids/sizes, targeted component reload API (`rogue_save_reload_component_from_slot`), and a debug toggle (`rogue_save_set_debug_json`) that emits `save_slot_X.json` on each save (`test_save_debug_json_toggle`). Phase 3.3 adds compile-time numeric width assertions + endianness guard. Phase 3.4 bumps format to v4 introducing unsigned LEB128 varint encoding for high-frequency counts (inventory items, skills, buffs) shrinking those fields for small values while maintaining backward compatibility (no data transform migration needed—reader branches on `descriptor.version`). Phase 3.5 adds a string interning table (component id 7, v5) with APIs `rogue_save_intern_string`, `rogue_save_intern_get`, `rogue_save_intern_count` to deduplicate repeated textual identifiers; section serialized with varint count and per-entry varint length + bytes. Phase 3.6 adds optional per-section RLE compression (v6) gated by `rogue_save_set_compression(enabled,min_bytes)`; compressed sections set high bit of size and store uncompressed size + RLE stream (byte,run) pairs. Phase 4.1 elevates integrity (v7) by appending a CRC32 (uncompressed payload) after every section and writing a final SHA256 footer (magic `SH32` + 32 bytes) over the entire post-header body; loader validates descriptor checksum, per-section CRCs, and footer digest (`test_save_v7_integrity`). Phase 4.2 (v8) introduces a replay hash section (component id 8) serializing deterministic gameplay-critical input events (frame, action code, value) and a SHA256 digest over the linear event buffer for future divergence diagnostics (`test_save_v8_replay_hash`). Phase 4.3 adds tamper detection flags (descriptor CRC / section CRC / SHA256) surfaced via `rogue_save_last_tamper_flags`, Phase 4.4 implements recovery loading (`rogue_save_manager_load_slot_with_recovery`) which falls back to the newest autosave on integrity failure (`test_save_v8_tamper_recovery`), and Phase 4.5 (v9) introduces an optional signature trailer (`[uint16 sig_len]["SGN0"][sig bytes]`) appended after the SHA256 footer when a provider is registered (`rogue_save_set_signature_provider`); verification failure sets tamper flag `ROGUE_TAMPER_FLAG_SIGNATURE`.
* NEW (Persistence Phase 5.1/5.2 runtime feature, no format bump): Incremental save mode with cached section payload reuse. Enabling via `rogue_save_set_incremental(1)` causes subsequent saves to reuse unchanged section bytes directly (skipping `write_fn` & compression) while recomputing global integrity (descriptor CRC, SHA256 footer, optional signature). Dirty components are marked using `rogue_save_mark_component_dirty(id)` or `rogue_save_mark_all_dirty()`. Test `test_save_incremental_basic` validates baseline reuse & dirty invalidation.
* NEW (Persistence Phase 6.1/6.3/6.4): Autosave scheduler & metrics: `rogue_save_set_autosave_interval_ms(ms)` + periodic `rogue_save_manager_update(now_ms, in_combat)` trigger ring autosaves when idle; quicksave via `rogue_save_manager_quicksave()`. Metrics (`rogue_save_last_save_rc/bytes/ms`, `rogue_save_autosave_count`) expose telemetry for future UI indicator (Phase 6.5). Test harness `test_save_autosave_scheduler` simulates elapsed time generating multiple autosaves.
* NEW (Persistence Phase 6.5): Autosave throttling + status string. API `rogue_save_set_autosave_throttle_ms(gap_ms)` enforces a minimum gap after any save; `rogue_save_status_string(buf,cap)` returns a concise snapshot for UI display. Test `test_save_autosave_indicator` covers throttled scheduling and status formatting.
* NEW (Persistence Phase 7.2/7.3): Extended skill & buff serialization. Skills now persist full extended runtime state (cast/channel progress, charge timers/counters, casting/channel active flags) appended to legacy (rank + cooldown) without format bump; loader auto-detects legacy minimal records via size heuristic. Buffs serialize relative remaining duration `(type,magnitude,remaining_ms)` (legacy absolute end time struct auto-converted). New APIs `rogue_buffs_active_count` / `rogue_buffs_get_active` decouple save layer from buff internals. Unit test `test_save_phase7_skill_buff_roundtrip` validates roundtrip integrity.
* NEW (Persistence Phase 7.1/7.7/7.8): Player progression + analytics + run metadata persistence. Player section now includes level, xp, xp_to_next, health, mana, action points, core stats, talent points, cumulative analytics counters (damage dealt, gold earned), permadeath_mode flag, equipped weapon id, current weapon infusion, and session_start_seconds (run start timestamp). Legacy minimal layouts still load. Unit test `test_save_phase7_player_analytics_roundtrip` verifies roundtrip.
* NEW (Persistence Phase 7.4): Inventory durability & equipment slots. Inventory item record extended with durability_cur/durability_max (weapons/armor only) using backward-compatible size heuristic (old records = 7 ints, new = 9). Added unit test `test_save_phase7_inventory_durability_roundtrip` validating durability preservation.
* NEW (Persistence Phase 7.5): Vendor inventory persistence including stock list (def_index, rarity) with price recomputed from formula on load plus vendor seed/restock timers. Unit test `test_save_phase7_vendor_inventory_roundtrip` ensures list roundtrip.
* NEW (Persistence Phase 7.6): Extended world meta component now serializes generation parameters (seed + water level, cave threshold, noise octaves, gain, lacunarity, river sources, river max length) with backward-compatible reader gating on payload size.
* NEW (Persistence Phase 7.5): Vendor inventory persistence (seed, restock timers, and current inventory entries (def, rarity) serialized; prices recomputed on load to honor latest pricing formula). Unit test `test_save_phase7_vendor_inventory_roundtrip` validates roundtrip.

#### Rarity System (Phase 5)
* Rarity enum centralized; color mapping utility avoids scattering switch statements across UI layers.
* NEW (Items 11.2–11.6): Crafting & Materials subsystem:
  - Material tier query (`rogue_material_tier`) classifies material definitions by rarity.
  - Recipe registry + parser (`rogue_craft_load_file`) supporting output item, quantity, up to 6 ingredients, and optional upgrade source + rarity delta.
  - Upgrade path helper (`rogue_craft_apply_upgrade`) for controlled rarity evolution (clamped 0..10).
  - Affix reroll API (`rogue_craft_reroll_affixes`) consuming materials + gold, delegating to existing affix roll generator; integrates economy & new inventory consume API.
  - Inventory removal primitive (`rogue_inventory_consume`) enabling safe resource deduction shared by crafting and future sinks.
  - Unit tests: `test_salvage_basic` (existing), `test_crafting_basic` (recipe execution + tier checks), `test_reroll_affix` (material + gold consumption, affix change).
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

#### Advanced Generation Pipeline (Phase 8) & Early Dynamic Balancing (Phase 9)
* Context object mixes enemy + player factors into seed (distinct large odd multipliers reduce collision risk).
* Multi-pass ordering (rarity→item→affixes) mirrors ARPG pipeline; easier to insert future quality adjustments pre-affix.
* Affix gating rules (initial hard-coded) enforce semantic item/affix compatibility (e.g., damage only on weapons).
* Duplicate avoidance filters candidate set before weight sampling.
* Quality scalar mapping uses diminishing returns curve `luck/(5+luck)` then linear interpolation to global min/max.
* (9.1) Global + per-category drop rate scalars layer: multiplicative global roll count adjustment + probabilistic per-category suppression enabling runtime economy/balancing hooks.
* (9.2) Adaptive weighting engine: continuously tracks observed counts per rarity & category and applies smoothed corrective multipliers (clamped 0.5x–2.0x with exponential smoothing) to gently lift under-dropped categories and damp overrepresented ones before later player preference & pity tie-ins.
* (9.3) Player preference learning: monitors pickup frequencies per category; applies mild dampening (0.75x–1.25x band) to over-picked categories to encourage build diversity without hard bans.
* (9.4) Accelerated pity: dynamic reduction of epic/legendary pity thresholds once halfway to target to smooth tail risk of long droughts.
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

#### Equipment Phase 3: Item Level & Power Budget
#### Equipment Phase 4.1: Implicit Modifier Layer

Phase 4 begins expanding beyond affixes into richer base item identity. Phase 4.1 introduces an explicit implicit modifier layer:

* Schema Extension: `RogueItemDef` gains optional implicit fields for primary attributes (strength/dexterity/vitality/intelligence), flat armor, and six resist types plus a `set_id` placeholder (future set bonuses).
* Backward Compatibility: Existing item config rows (14 core columns + optional rarity + flags) remain valid; any omitted implicit columns default to zero. Extended format appends: flags, imp_str, imp_dex, imp_vit, imp_int, imp_armor, imp_rphys, imp_rfire, imp_rcold, imp_rlight, imp_rpoison, imp_rstatus, set_id.
* Aggregation: New implicit gather pass sums definition implicit stats across equipped items, populating `implicit_*` fields in the layered stat cache and contributing defensive values (resists, flat armor) alongside affix sources.
* Layering Integrity: `compute_layers` no longer zeroes implicit fields, preserving deterministic pre-populated values prior to derived metric calculation and fingerprint hashing.
* Determinism: Fingerprint includes implicit contributions automatically (fields precede the fingerprint member). Equip ordering invariance verified in new unit test.
* Test: `test_equipment_phase4_1_implicits` covers parsing extended columns, implicit layer population, fingerprint stability under equip order swap, and mutation when adding a new implicit source.

Upcoming (remaining Phase 4 goals): unique item registry & hooks (4.2), set bonus thresholds & partial scaling (4.3/4.4), runeword recipe scaffold (4.5), and deterministic precedence rules (4.6) with comprehensive test suite (4.7).

#### Equipment Phase 4.2: Unique Item Registry & Layer

Introduces a simple data-driven unique item system layered cleanly between implicit and affix contributions:

* Registry: `equipment_uniques.c` maintains up to 64 `RogueUniqueDef` entries (id, base_item_id, fixed stat bonuses, future hook id).
* Layering: Stat cache expanded with `unique_*` fields; aggregation pass detects equipped items whose base definition matches a registered unique and sums bonuses.
* Determinism: Unique layer ordered after implicit and before affixes, ensuring predictable overrides without mutating base or implicit stats.
* Extensibility: Hook id reserved for future behavior (on-hit effects, conditional procs) while preserving pure stat aggregation now.
* Test: `test_equipment_phase4_2_uniques` validates registration, aggregation, resist contributions, and fingerprint change when a second unique (helm) is added post-equip.

#### Equipment Phase 4.3–4.7: Sets, Partial Scaling, Runewords & Precedence

These slices extend the equipment identity layers beyond uniques, introducing cumulative set bonuses, smooth interpolation between tier thresholds, an initial runeword scaffold, and a deterministic precedence contract across all stat contributors.

Key Additions:
* Set Registry (4.3): Static descriptors (id, threshold table). Each threshold specifies required piece count and a bundled stat delta (primary attributes, flat armor, resistances). Equipping pieces with matching `set_id` accumulates active thresholds and sums their bundles.
* Partial Scaling (4.4): If current equipped piece count lies between two defined thresholds, a linear interpolation per stat bridges the gap producing incremental gains (integer floor) instead of a flat plateau. Prevents abrupt jumps when approaching the next tier.
* Runeword Scaffold (4.5): Introduces a lightweight registry (id, pattern string, stat bundle). Present prototype matches pattern to base item id (placeholder until sockets/gems in Phase 5 supply ordered rune data). On match, runeword bonuses populate a dedicated `runeword_*` layer.
* Precedence Rules (4.6): Finalized deterministic aggregation order and total formula composition: implicit → unique → set → runeword → affix → buff → derived metrics. Earlier layers remain immutable input for later ones, simplifying reasoning and future conflict policies (e.g., runeword superseding an affix stat intentionally).
* Comprehensive Tests (4.7): `test_equipment_phase4_3_5_sets_runewords` validates: set counting, threshold activation, interpolation math (midpoint correctness), runeword layer population, multi-layer stacking with uniques & implicits, and fingerprint invariance under equip order permutations ensuring no regression in determinism.

Implementation Notes:
* Set & runeword registries live in `equipment_stats.c` for the initial slice to minimize plumbing overhead; a later refactor may mirror the standalone uniques module for data-driven loading / hot reload.
* Interpolation formula: `interp = prev + ( (next - prev) * (pieces - prev_req) ) / (next_req - prev_req )` applied per numeric field; resistant to division by zero because thresholds are validated strictly increasing.
* No special casing for negative deltas (future design could allow inverse scaling). Current stat bundle fields all non-negative ensuring monotonic progression.
* Fingerprint automatically encompasses new layers (structure ordering places new fields before the fingerprint member) providing regression protection without logic changes.

Planned Next (Phase 5 preview): Introduce sockets & gems feeding into or enabling real runeword pattern verification (ordered rune sequence + socket count), along with enchant & reforge mechanics. The present runeword pattern placeholder will be replaced by socket composition matching, preserving existing layer precedence.

### Equipment Phase 5.1 – Socket Count Infrastructure
Implemented foundational socketing support:
* Definition Schema: Added `socket_min` / `socket_max` optional columns (indices 28/29 after `set_id`) in item definition CSV; parser clamps negative to 0 and max to <=6, enforcing `max>=min`.
* Instance Initialization: Spawn path rolls deterministic socket counts using a lightweight LCG seeded by instance slot, definition index, and integer world position, producing a value in `[min,max]` (capped 6). Empty sockets initialized to `-1`.
* Runtime API: `rogue_item_instance_socket_count`, `rogue_item_instance_get_socket`, `rogue_item_instance_socket_insert`, `rogue_item_instance_socket_remove` for querying and mutating gem occupancy (placeholder gem id = item def index until dedicated gem registry in 5.2).
* Determinism & Bounds: Fixed-count case (`min==max`) yields invariant socket count; range case covered by unit test across multiple spawn positions asserting bounds `[min,max]`.
* Testing: New unit test `test_equipment_phase5_sockets` loads a dedicated `equipment_test_sockets.cfg`, validates parser ingestion (`2..4` and `3..3`), exercises multiple spawns to observe distribution within bounds, and verifies insert/remove + occupied slot rejection.

Upcoming (5.2+): Introduce gem definition category (`ROGUE_ITEM_GEM`) stat contributions layered before affixes, economic insertion/removal costs & safety (5.3), enchanting & reforge mechanics (5.4–5.5), and optional protective seal (5.6). Runeword placeholder matching will transition to socket rune sequence evaluation once gem typing exists.

### Equipment Phase 5.2–5.3 – Gems & Economic Socket Operations
Implemented gem system and paid socketing:
* Gem Registry (5.2): Added `equipment_gems.[ch]` with `RogueGemDef` (flat primary/resist/armor, percent primary bonuses, proc metadata placeholders). CSV loader `gems_test.cfg` demonstrates format.
* Aggregation: `rogue_gems_aggregate_equipped` integrates into equipment stat pipeline prior to sets/runewords, contributing flats and approximate percent conversions (converted to flat based on base primary stats for deterministic simplicity pending a dedicated percent layer).
* Economic Insertion (5.3): High-level APIs `rogue_item_instance_socket_insert_pay` and `rogue_item_instance_socket_remove_refund` wrap base socket operations, charging gold (`cost = 10 + 2*(sum flat + sum pct)`) and refunding 50% on removal plus optional gem return to inventory.
* Safety: Failure modes (insufficient funds, slot occupied, invalid gem) abort without mutating sockets or gold. Removal always preserves the item (never destroys base equipment) and optionally returns gem item to inventory.
* Tests: `test_equipment_phase5_gems` loads gem and item fixtures, forces socket count, inserts flat and percent gems, validates stat increases (strength >= base + flat), fire resist accumulation, and successful removal with refund path.
* Roadmap: Marked 5.2 and 5.3 Done; groundwork laid for 5.4 Enchant & 5.5 Reforge to extend economic model.

### Equipment Phase 5.4–5.5 – Enchant & Reforge
Introduced affix modification crafting operations:
* Enchant: Selectively reroll prefix and/or suffix while preserving the untouched affix, item_level, rarity, sockets & existing gems. Cost formula `50 + 5*item_level + (rarity^2)*25 + 10*sockets`. If both affixes are rerolled, a rare material `enchant_orb` is also consumed. Failure leaves item unchanged.
* Reforge: Full affix wipe + reroll (respecting rarity rules) preserving item_level, socket_count, rarity; clears gem contents (socket_count retained). Cost = 2x enchant cost and consumes `reforge_hammer`.
* Determinism: Seed basis mixes instance index and item_level for stable reroll results in tests.
* Validation: Post-operation budget check ensures no illegal stat inflation; failure aborts.
* Materials: Added `enchant_orb`, `reforge_hammer` to test item defs (category material).
* Tests: New unit test `test_equipment_phase5_enchant_reforge` covers selective reroll, dual reroll, reforge preservation (level, rarity, socket_count) and failure modes (insufficient gold, missing material). Existing gem test already validates gem stacking; roadmap Phase 5.7 updated accordingly.
* Protective Seal (5.6): Added optional crafting material `protective_seal` letting players lock prefix and/or suffix prior to an enchant. Locked affixes are skipped during reroll; attempt returns -2 if nothing remains to reroll. Single seal consumed per operation regardless of locks applied.
* Roadmap: 5.4, 5.5, 5.6, 5.7 marked Done (5.6 implemented as optional feature).

### Equipment Phase 6.1–6.3 – Proc Engine Foundations
Implemented an initial conditional effect (proc) subsystem:
* Unified Descriptor: `RogueProcDef` captures trigger (hit/crit/kill/block/dodge/low HP), internal cooldown, duration, stacking rule (refresh/stack/ignore), magnitude scalar, and max stacks.
* Runtime: Registry with up to 64 procs, per-proc cooldown + duration timers, uptime accumulation for telemetry, simple global per-second trigger rate cap to prevent spam.
* Events: APIs `rogue_procs_event_hit(was_crit)`, `rogue_procs_event_kill`, `rogue_procs_event_block`, `rogue_procs_event_dodge`, plus `rogue_procs_update(dt_ms,hp_cur,hp_max)` to tick timers.
* Stacking Rules: Refresh (keeps stacks, resets duration) and Stack (increments up to max, shared duration) implemented; Ignore (fires once until expiry) reserved for future.
* Telemetry: Query trigger counts, current stacks, uptime ratio for future balancing & analytics integration.
* Tests: `test_equipment_phase6_proc_engine` simulates mixed hit/crit sequence validating trigger counts and stacking persistence.
* Roadmap: Marked 6.1–6.3 Done; forthcoming phases (6.4–6.7) will add advanced stacking semantics variety, rate limiting refinements, and deterministic ordering tests.

### Equipment Phase 6.4–6.7: Advanced Stacking, Rate Limiting & Telemetry
Extended the proc subsystem with full stacking semantics, deterministic ordering, enhanced rate limiting, and richer analytics:
* Stacking Rules Finalized (6.4): Implemented explicit enum dispatch – Refresh (duration reset, stack count stable), Stack (increments up to max_stacks; shared duration window), Ignore (first trigger holds until expiry). Internal state machine refactored for clarity & future extension (e.g., decay stacking).
* Global Rate Limiting (6.5): Consolidated per-proc ICD with a global rolling window trigger cap to prevent burst spam; added monotonically increasing global sequence id assigned at each successful trigger to guarantee deterministic ordering across mixed trigger types even when throttled.
* Telemetry Expansion (6.6): Added per-proc triggers-per-minute calculation (scaled from total triggers & accumulated active milliseconds) plus last trigger sequence accessor for ordering assertions; existing uptime accumulation retained for later balancing dashboards.
* Deterministic Ordering (6.6 adjunct): Sequence numbers provide stable comparison for replay / audit tooling; no dependence on registration index once triggered.
* Tests (6.7): New `test_equipment_phase6_proc_engine_extended` exercises rapid hit events verifying: (a) refresh rule never increases stack count, (b) stacking rule caps at max, (c) ignore rule maintains single stack, (d) non-zero triggers-per-minute, (e) positive and ordered sequence numbers.
* Roadmap Updated: Phase 6.4–6.7 marked Done; foundation ready for later conditional affix categories and analytics phases (11.x).



* Each `RogueItemInstance` now tracks `item_level` (baseline 1 on spawn) driving a power budget.
* Budget formula: `budget = 20 + item_level * 5 + (rarity^2) * 10` (rarity clamped 0..4). Provides quadratic headroom for premium tiers while keeping early levels constrained.
* Affix roll enforcement: After prefix/suffix value rolls, aggregate weight is clamped to budget by decrementing the larger affix first (ties favor prefix) ensuring no over-budget items enter the ecosystem.
* Upgrade API: `rogue_item_instance_upgrade_level(inst, levels, &seed)` raises item_level and gently increases affix values (+1 steps with deterministic LCG selection) until reaching new cap.
* Validation: `rogue_item_instance_validate_budget` returns 0 if compliant, negative error codes if over or invalid. A lightweight safeguard for persistence or external tooling ingest.
* Test `test_equipment_phase3_budget` covers: (a) baseline under-cap item, (b) forced over-cap detection, (c) multi-level upgrade increasing affix value without exceeding new budget.

#### Loot Filter (Phase 12.1–12.2 Initial)
* Rule file parser supports lines: `rarity>=N`, `rarity<=N`, `category=...`, `name~substr`, `def=item_id`, and mode switches `MODE=ANY` / `MODE=ALL` (default AND semantics).
* Up to 64 rules stored; case-insensitive comparisons for names & substrings.
* Runtime evaluation marks each `RogueItemInstance` with `hidden_filter` flag; visible count via `rogue_items_visible_count`.
* Reapply API (`rogue_loot_filter_refresh_instances`) lets UI or console command reload filter and instantly hide/show existing ground items.
* Unit test `test_loot_filter_basic` validates loading, rule count, visibility transition (rarity>=3 hides common sword but not epic blade).

Implemented lightweight string builders for item inspection:
* `rogue_item_tooltip_build(inst, buf, sz)` lists name, quantity, damage/armor lines, affix stat rolls, and durability.
* `rogue_item_tooltip_build_compare(inst, slot, buf, sz)` appends a delta damage line vs equipped slot (currently weapon slot support).
* Supports ground or equipped instances (uses instance index lookups; future integration will feed hover selection from UI layer).
* Tests: `test_loot_tooltip_basic` (validates lines) and `test_loot_tooltip_compare` (ensures comparison delta appended).

Added passive visualization of recent ground item spawns on the minimap:
* Each item spawn registers a ping via `rogue_minimap_ping_loot(x,y,rarity)` triggered inside `rogue_items_spawn`.
* Pings persist for 5 seconds, fading out during the final 20% of lifetime.
* Rendering overlays small 3x3 colored squares (rarity color) on the minimap after the player marker.
* Module: `minimap_loot_pings.c/h` with update (`rogue_minimap_pings_update`) and render hook (`rogue_minimap_render_loot_pings`).
* Unit Test `test_equipment_phase19_2_5_vfx` validates lifecycle: spawn state, pulse activation window, alpha increase, despawn cleanup.

Unit test `test_equipment_unequip_delta` verifies stat bonus application on equip and removal on unequip, completing roadmap item 14.5.

---

## 4. Architecture & Code Layout
```
src/
  core/      Game loop, entity & world subsystems.
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
ctest -C Release -R loot_phase8_generation_basic -V
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
* `assets/items/*.cfg` – Modular category files (e.g. `swords.cfg`, `potions.cfg`, `armor.cfg`, `gems.cfg`, `materials.cfg`, `misc.cfg`). Each uses the same 15-column schema. Load them in bulk via `rogue_item_defs_load_directory("../assets/items")` for cleaner content scaling.
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
4. Update phase status / docs.
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
* Affix selection uses on-stack arrays; no heap fragmentation during generation.
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
| 10.x Economy & Vendors | Partial | Shop generation + rarity-based pricing implemented; restock & scaling pending |

Refer to `implementation_plan.txt` for granularity (over 150 line milestones).

---

## 11. Changelog & Latest Highlights

### [Unreleased]
Planned: Dynamic drop balancing (9.x), economy systems (10.x), crafting (11.x), loot filtering UI (12.x).

### UI System Phase 1 Progress
* Completed Phase 1.1–1.7 core architecture slice:
  - Module scaffolding, `RogueUIContext` lifecycle, panel & text primitives.
  - Deterministic RNG channel (xorshift32) isolated from simulation RNG.
  - Simulation snapshot separation (opaque pointer + size) enabling read-only UI consumption without coupling.
  - Per-frame transient memory arena (configurable size, default 32KB) with `rogue_ui_arena_alloc` + `rogue_ui_text_dup` helper eliminating heap churn for ephemeral strings.
  - Deterministic UI tree serializer (stable line format) + FNV-1a hashing diff API (`rogue_ui_diff_changed`) for regression tests & future golden snapshots.
  - Extended unit tests: `test_ui_phase1_basic` (init, primitives, RNG stability) and `test_ui_phase1_features_ext` (arena alloc, snapshot separation, diff detection, capacity & arena exhaustion).
* Next (Phase 2): Add additional primitives (Image/Sprite/ProgressBar) and interactive widgets (Button/Toggle/Slider/TextInput) with focus & ID plumbing.
  - Phase 2.1 completed: Added Image, Sprite, ProgressBar primitives with extended `RogueUINode` fields (aux_color, value/value_max, data_i0/i1) and test `test_ui_phase2_primitives` validating kinds & value clamping.
  - Phase 2.2 completed: Added interactive Button, Toggle, Slider, TextInput widgets (immediate-mode hit test, active/hot/focus indices) with test `test_ui_phase2_interactive` covering click, toggle flip, slider drag value update, text input character insertion & backspace.
  - Phase 2.3 completed: Added Row & Column incremental layout helpers (begin + next APIs maintaining internal cursor with padding & spacing), grid cell rect helper for uniform grids, and lightweight layer node (order stored in `data_i0`). Unit test `test_ui_phase2_layout_id` validates horizontal/vertical placement ordering, grid math, and non-overlap.
  - Phase 2.7 completed: Widget ID hashing (FNV1a over label text) automatically assigns `id_hash` for lookup via `rogue_ui_find_by_id`; stable across frames (verified in `test_ui_phase2_layout_id`).
  - Phase 2.4 completed: Scroll container (vertical) with wheel-driven offset clamped to content height; helper APIs (`rogue_ui_scroll_begin`, `rogue_ui_scroll_apply`, `rogue_ui_scroll_offset`) – foundation for future inertial easing & virtualization. Covered in `test_ui_phase2_scroll_tooltip_nav` (offset advances after wheel input).
  - Phase 2.5 completed: Tooltip system with hover intent delay; creates anchored panel + text after configurable ms threshold; validated by forcing time advance in unit test.
  - Phase 2.6 completed: Minimal declarative DSL convenience macros (`UI_PANEL`, `UI_TEXT`, `UI_BUTTON`) providing clearer callsites and reducing boilerplate.
  - Phase 2.8 partial update: Linear Tab and arrow-key navigation cycling across interactive widgets implemented (`rogue_ui_navigation_update`); spatial heuristic & controller graph still pending.
  - Phase 6.1 completed: Data-driven HUD layout spec (`assets/hud_layout.cfg`) parsed via unified kv parser, providing configurable positions/sizes for health, mana, XP bars and level text. Loader APIs: `rogue_hud_layout_load`, `rogue_hud_layout`. New test `test_ui_phase6_hud_layout` verifies parse & coordinate propagation.
* Phase 6.2 HUD Bars (NEW): Added layered smoothing health/mana/AP bars with trailing damage indicator (secondary lag bar) and new AP bar rendered beneath XP. API `hud_bars.h` exposes update + fraction accessors; unit test `test_ui_phase6_bars_ap` validates lag behavior and snap on heal.
* Phase 6.3 Buff Belt (NEW): Added snapshot API (`rogue_buffs_snapshot`) and HUD buff belt module (`hud_buff_belt.c/h`) rendering active buffs (stack magnitude text + cooldown overlay). Test `test_ui_phase6_buff_belt` validates appearance/disappearance timing.
* Phase 6.4 Damage Number Batch (NEW): Enhanced floating combat text with spatial batching (nearby merges), alpha ease-in/out, quadratic end fade, and per-number adaptive life extension on merges (no separate test yet—leverages existing combat tests implicitly).
* Phase 6.5 Minimap Toggle (NEW): Added `show_minimap` flag & 'M' key toggle gating conditional minimap render path; test `test_ui_phase6_minimap_metrics_alerts` covers toggle logic.
* Phase 6.6 Alerts System (NEW): Center-top queued alert banners (level up, low health, vendor restock) with fade-out; APIs `rogue_alert_level_up/low_health/vendor_restock`, manual test triggers via F2/F3/F4; integrated into `hud.c`.
* Phase 6.7 Metrics Overlay (NEW): Runtime FPS, frame ms, rolling average displayed bottom-left; toggled with F1 (`show_metrics_overlay`); included in same test harness.
* Phase 7.1 Theme Asset Pack (NEW): Added key/value driven `ui_theme_default.cfg` with palette (panel/bg/button/slider/tooltip/alert colors), spacing (padding_small/large), base font size, DPI scale integer for deterministic scaling.
* Phase 7.2 Theme Hot-Swap & Diff (NEW): Loader `rogue_ui_theme_load`, apply API `rogue_ui_theme_apply`, and bitmask diff `rogue_ui_theme_diff` enabling targeted re-render invalidation (unit test exercises change detection for color + spacing fields).
* Phase 7.3 Accessibility – Colorblind Modes COMPLETE: Added colorblind mode toggle with transforms for protanopia, deuteranopia, and tritanopia (`rogue_ui_colorblind_transform`). Existing unit test exercises protanopia path; transforms share approximation matrices.
* Phase 7.4 Scalable UI Helpers: Introduced DPI scaling helpers (`rogue_ui_dpi_scale_x100`, `rogue_ui_scale_px`) enabling deterministic integer layout scaling (theme supplies dpi_scale_x100). Widgets not yet scaled will be migrated in Phase 8 animation/layout pass.
* Phase 7.5 Reduced Motion Mode: Global toggle (`rogue_ui_set_reduced_motion` / `rogue_ui_reduced_motion`) for disabling or simplifying future large transitions / animations (pulse/spend already queryable).
* Phase 7.6 Narration Stub: Added lightweight narration buffer + APIs (`rogue_ui_narrate`, `rogue_ui_last_narration`) to prepare for screen reader integration; currently stores last message for tests/tooling.
* Phase 7.7 Focus Audit Tool: Developer overlay utilities (`rogue_ui_focus_audit_enable`, `rogue_ui_focus_audit_emit_overlays`) visually highlight focusable widgets and export deterministic tab/focus order (`rogue_ui_focus_order_export`) for automated auditing.
* Phase 10.1 Headless Harness: Introduced deterministic one-shot frame runner (`rogue_ui_headless_run`) and tree hash helper (`rogue_ui_tree_hash`) with unit test `test_ui_phase10_headless_harness`—foundation for golden diff, navigation traversal, and performance smoke tests.
* Phase 10.3 Navigation Traversal Test: Added `test_ui_phase10_navigation_traversal` verifying horizontal and vertical focus cycling across a grid without traps or skips (wrap correctness & uniqueness guarantees).
* Phase 11.1 Style Guide Catalog: Programmatic widget catalog builder (`rogue_ui_style_guide_build`) produces representative panel/button/toggle/slider/textinput/progress examples for documentation & visual diffing.
* Phase 11.2 Developer Inspector: Hierarchy overlay + selection & live color edit APIs (`rogue_ui_inspector_enable`, `rogue_ui_inspector_emit`, `rogue_ui_inspector_edit_color`) with automated test coverage.
* Phase 11.3 Crash Snapshot: Lightweight snapshot (`rogue_ui_snapshot`) capturing node count, last tree hash placeholder, and input state for post-mortem debugging.

