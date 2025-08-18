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

Next phases (M3+) will introduce unified config schema, hot reload, and expanded deterministic replay/coverage gates.

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
* Phase 5.1 Hitbox Primitives: Introduced foundational spatial volume types (`RogueHitbox` union) for future hit detection: capsule (segment+radius), swept arc (angle sector with optional inner radius), multi-segment chain (series of capsules), and projectile spawn descriptor (count, spread, speed). Added intersection helpers for point testing (capsule/arc/chain) and projectile spread angle helper. Unit test `test_combat_phase5_hitbox_primitives` validates geometry inclusion/exclusion and evenly distributed projectile angles. These are pure POD utilities (no allocations) ready for external authoring (5.2) and broadphase culling integration (5.3).
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
  New unit test `test_ui_phase5_skillgraph_advanced` exercises filtering, export/import, allocation + undo.
* Phase 15 Extended Persistence & Versioning: Introduced forward-compatible item instance field `enchant_level` with size-heuristic reader (no format bump). Added inventory-only partial save API `rogue_save_manager_save_slot_inventory_only` enabling lightweight persistence of inventory changes without rewriting other components, and backup rotation helper `rogue_save_manager_backup_rotate` producing timestamped `.bak` copies (pruning hook ready). Inventory section writer now emits enchant level; reader auto-populates when present while legacy saves load with default 0. New unit test `test_save_phase15_inventory_partial_backup` validates partial save path updates quantity and backup path creation.
* Phase 16 Multiplayer / Personal Loot: Completed full feature slice. Per-item ownership (`owner_player_id`) + loot mode enum (shared/personal). Need/Greed system with session begin/choose/resolve APIs (`rogue_loot_need_greed_begin/choose/resolve/winner`) prioritizes NEED over GREED, deterministic LCG-based 400–699 (greed) / 700–999 (need) roll ranges, and locks the instance (pickup & trade gated) until resolution. Ownership assigned to winner (forcing temporary personal mode for assignment). Trade validation now rejects self-trade, non-owner attempts, and locked instances. Anti-duplication safeguard: unresolved need/greed session sets logical lock consumed by pickup loop (`rogue_loot_instance_locked`). Tests: `test_loot_phase16_personal_mode` (ownership gating + trade) and `test_loot_phase16_need_greed_trade` (need vs greed priority, pass handling, post-resolution trade, rejection paths).
* Phase 17 Performance & Memory: Added `loot_perf.c/h` module providing object pool for affix roll scratch buffers with metrics (acquires, releases, peak usage) and SIMD (SSE2) accelerated weight summation path (with scalar fallback) tracking separate counters. Expanded profiling harness (17.3) adds high-resolution nanosecond timing for weight summations and full affix roll sections (`weight_sum_time_ns`, `affix_roll_time_ns`) via `rogue_loot_perf_affix_roll_begin/end`. Implemented cache-friendly item definition indexing (17.4) using an open-addressed FNV-1a hash table rebuilt after config loads; fast lookup API `rogue_item_def_index_fast` falls back to linear scan on miss. Unit tests `test_loot_phase17_index` and `test_loot_phase17_perf` cover index correctness, pool lifecycle, and SIMD metrics. Incremental serialization Phase 17.5 now extends incremental mode with record-level inventory diff metrics: consecutive saves in incremental mode count reused vs rewritten item records (`rogue_save_inventory_diff_metrics`). Test `test_loot_phase17_5_diff` validates first-save rewrite, unchanged reuse, and single-record modification paths.
* Phase 18 Analytics: Initial slice plus advanced features. Base module provides drop event ring buffer (512 capacity) and JSON export summarizing event count & rarity distribution (APIs: `rogue_loot_analytics_record`, `rogue_loot_analytics_recent`, `rogue_loot_analytics_export_json`; test `test_loot_phase18_analytics`). New (18.3) rarity distribution drift detection with configurable baseline fractions or counts (`rogue_loot_analytics_set_baseline_counts`, `rogue_loot_analytics_set_baseline_fractions`) and relative deviation threshold (`rogue_loot_analytics_set_drift_threshold`) producing per-rarity flags via `rogue_loot_analytics_check_drift`. New (18.4) session summary struct `RogueLootSessionSummary` aggregating totals, counts, duration, drops/min, and drift OR (`rogue_loot_analytics_session_summary`). New (18.5) positional heatmap (32x32 grid) APIs: `rogue_loot_analytics_record_pos`, `rogue_loot_analytics_heat_at`, `rogue_loot_analytics_export_heatmap_csv`. Advanced test `test_loot_phase18_3_4_5_advanced` validates drift alert triggering, summary values, and heatmap export.
* Phase 19.1 Audio/Visual Polish (initial): Added per-rarity pickup sound id mapping APIs (`rogue_rarity_set_pickup_sound`, `rogue_rarity_get_pickup_sound`) with integration in pickup loop logging selected sound id (placeholder for future audio backend). Unit test `test_loot_phase19_1_pickup_sounds` validates mapping usage and absence of regressions in pickup flow.
* Phase 19.2–19.5 Visual Polish: Added lightweight loot VFX state module (`loot_vfx.c/h`) tracking sparkle timer, high-rarity beam flag (rarity>=3), edge notifier distance check, and final 5s despawn pulse (alpha 0→1). APIs: `rogue_loot_vfx_on_spawn`, `rogue_loot_vfx_on_despawn`, `rogue_loot_vfx_update`, `rogue_loot_vfx_get`, `rogue_loot_vfx_edge_notifiers`. Integrated into spawn/despawn/update loops. Unit test `test_loot_phase19_2_5_vfx` validates lifecycle: spawn state, pulse activation window, alpha increase, despawn cleanup.
* Phase 20.1 QA – Loot Table Fuzz: Introduced dedicated fuzz harness `test_loot_phase20_1_fuzz_tables` generating 1200 randomized table lines (well‑formed, malformed, long ids, random noise, empty, comments) with mixed valid/invalid item ids, negative & zero weights, reversed quantity ranges, and varied rarity min/max (including negatives). Ensures loader never crashes, respects table/entry caps (`ROGUE_MAX_LOOT_TABLES`, `ROGUE_MAX_LOOT_ENTRIES`), clamps invalid fields (negative weights → 0 ignored, qmax<qmin corrected), and tolerates malformed segments gracefully. Provides early regression shield before large statistical sampling (20.2/20.3). 
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
* NEW (Persistence Phase 6.1/6.3/6.4): Autosave scheduler & metrics: `rogue_save_set_autosave_interval_ms(ms)` + periodic `rogue_save_manager_update(now_ms, in_combat)` trigger ring autosaves when idle; quicksave via `rogue_save_manager_quicksave()`. Metrics (`rogue_save_last_save_rc/bytes/ms`, `rogue_save_autosave_count`) expose telemetry for future UI indicator (Phase 6.5). Test `test_save_autosave_scheduler` simulates elapsed time generating multiple autosaves.
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

#### Loot Filter (Phase 12.1–12.2 Initial)
* Rule file parser supports lines: `rarity>=N`, `rarity<=N`, `category=...`, `name~substr`, `def=item_id`, and mode switches `MODE=ANY` / `MODE=ALL` (default AND semantics).
* Up to 64 rules stored; case-insensitive comparisons for names & substrings.
* Runtime evaluation marks each `RogueItemInstance` with `hidden_filter` flag; visible count via `rogue_items_visible_count`.
* Reapply API (`rogue_loot_filter_refresh_instances`) lets UI or console command reload filter and instantly hide/show existing ground items.
* Unit test `test_loot_filter_basic` validates loading, rule count, visibility transition (rarity>=3 hides common sword but not epic blade).
* Basic rarity-colored item outline (12.3) added in `rogue_world_render_items` drawing a faint border; foundation for future glow/pulse effects.

#### Logging & Debuggability
* Consistent structured log format includes location for grep-based triage.

#### Intentional Simplifications (Documented)
* Single prefix + suffix slot (multi-slot & rerolling deferred to economy/crafting phases).
* Hard-coded gating rules (will migrate to data-driven rule graph in 8.2+ extended or tooling 21.x).
* Simple additive dynamic weighting (PID / smoothing reserved for 9.x adaptive balancing).

---

## Persistence & Save System (Phase 1 Complete; Phase 2 Kickoff)
Phase 1 introduced a binary SaveManager with:
* Descriptor header (`RogueSaveDescriptor`): version, timestamp, component mask, section count, total size, CRC32 payload checksum.
* Deterministic section ordering (qsort by stable component id).
* Components implemented: player, world_meta, inventory (instances + affix fields), skills (rank + cooldown), buffs (active list), vendor (seed + timers).
* Atomic write (temp file then rename) minimizing corruption window.
* Roundtrip integration test (`test_save_roundtrip`).

Phase 2 foundations expanded:
* `ROGUE_SAVE_FORMAT_VERSION` bumped to 2 (no-op migration path prepared).
* Migration registry + linear walker (currently no data transforms required v1->v2).
* Autosave ring (`rogue_save_manager_autosave`) rotates `autosave_0..N-1.sav` (N=ROGUE_AUTOSAVE_RING).
* Durability toggle (`rogue_save_manager_set_durable`) optionally calls `_commit` / `fsync` after header rewrite.
* Refactored internal save path routine consolidating checksum + ordering.
* Incremental migration runner with rollback safety (loads payload into memory, applies sequential migrations, aborts on failure without mutating on-disk file).
* Migration metrics (steps, duration ms, failure flag) exposed via `rogue_save_last_migration_*` accessors.
* Test harnesses: `test_save_migration_chain` (legacy header upgrade), `test_save_migration_metrics` (metrics correctness), `test_save_ordering_determinism` (section ordering stable), plus existing roundtrip & autosave ring tests.

Upcoming (Phase 2 / 3 targets):
* Per-section encode/decode unit tests & corruption fuzzing.
* Section-level CRC + optional SHA256 (integrity hardening).
* Section-level CRC + optional SHA256 (integrity hardening).
* Debug CLI (inspect / diff saves) and JSON export.

---

---

## 3. Loot System (Current Focus Area)
Recent development concentrated on Phases 5–8 of the roadmap:
* Rarity Enhancements: dynamic weighting, per‑rarity spawn sound hooks, despawn overrides, rarity floor, pity counter.
* Affix Framework: definition parsing, rarity weighting, deterministic stat rolls, instance attachment & persistence.
* Advanced Generation Pipeline (8.x): context (enemy level / biome / archetype / player luck), multi‑pass rarity→base→affixes, seed mixing, affix gating, quality scalar bias, duplicate avoidance.
* Telemetry: rolling rarity window stats, on‑demand histogram print, JSON export snapshot.

These layers enable reproducible progression tuning while preserving deterministic seeds for testability.

### 3.1 Loot Tuning Console Commands (Phase 9.6)
A lightweight developer/testing command interface (string parser; no GUI dependency) allows rapid runtime tuning & verification of rarity dynamics:

Supported commands:
* `weight <rarity 0-4> <factor>` – Set dynamic rarity multiplier (applied before sampling). Factors <=0 clamped to tiny positive.
* `get <rarity>` – Query current multiplier.
* `reset_dyn` – Reset all rarity multipliers to 1.0.
* `reset_stats` – Clear rolling rarity statistics window.
* `stats` – One-line snapshot: `STATS: C=.. U=.. R=.. E=.. L=..` (Common→Legendary).

Usage pattern inside tests (see `test_loot_phase9_tuning_console.c`):
```c
char buf[128];
rogue_loot_run_command("weight 4 25", buf, sizeof buf); // Heavily bias legendary
rogue_loot_run_command("stats", buf, sizeof buf);        // Inspect rarity distribution window
```
This enables scripting future balancing harnesses (e.g., fuzz/statistical drift tests) without adding gameplay UI complexity.

### 3.2 Vendor Economy (Phase 10.1–10.3 Progress)
An initial vendor system now layers the economy foundation atop the loot pipeline:
* Inventory Generation (10.1): `rogue_vendor_generate_inventory(table_index, slots, ctx, &seed)` reuses deterministic loot generation (rarity roll, base item selection, affix eligibility) to populate shop slots.
* Price Formula (10.2): Rarity multiplier ladder applied to `base_value` (1x/3x/9x/27x/81x for Common→Legendary). Affix & demand scaling hooks slated for 10.4/10.5.
* Restock & Rotation (10.3): `RogueVendorRotation` tracks candidate loot tables, accumulates elapsed time, and on interval rotates active table + regenerates inventory (`rogue_vendor_update_and_maybe_restock`). Ensures evolving shop composition during extended sessions.
* Drop Rate Safety: Vendor generation force-initializes drop rate scalars to avoid Release-build static zero-initialization suppressing all categories when tests omit earlier gameplay initialization.
* Test Coverage: `test_vendor_inventory.c` validates (a) non-zero bounded generation, (b) price >0 for all items, (c) monotonic multiplier ladder. Runtime checks replace asserts so Release builds surface failures (asserts compiled out when `NDEBUG`).

Planned follow-ups (remaining 10.x): currency sinks (repairs/rerolls), reputation-based price scaling, buyback/sell value bounds, affix-aware dynamic pricing.

### 3.3 Economy & Salvage Extensions (10.4–10.6, 11.1)
New systems added:
* Gold Wallet: `rogue_econ_add_gold`, `rogue_econ_gold` for central currency tracking (clamped, overflow safe).
* Reputation Scaling (10.5): `rogue_econ_set_reputation` influences buy price via linear discount capped at 50% of base price.
* Buy/Sell Operations: `rogue_econ_try_buy` and `rogue_econ_sell` enforce bounded sell value (10%..70% of vendor base) with unit test safeguards.
* Salvage (11.1): `rogue_salvage_item` converts items to materials (rarity & value bracket scaled) using existing material defs (`arcane_dust`, `primal_shard`). Basic yield test in `test_salvage_basic.c`.
* Currency Sink Cost Formulas (10.4 partial): `rogue_econ_repair_cost(missing, rarity)` linear scaling (unit = 5 + rarity*5) and `rogue_econ_reroll_affix_cost(rarity)` exponential (50 * 2^rarity clamped) now implemented + unit test (`test_economy_sink.c`). Integration hooks (actual durability consumption, affix reroll op) still pending.
* Equipment & Durability Groundwork: Item instances now carry `durability_cur/max` (initialized for weapon/armor categories) and a minimal equip API (weapon + armor slots) to enable future repair fee application & stat recomputation pipeline.
Upcoming: full repair action, affix reroll operation consuming gold/materials, richer material tier mapping & recipe parser (11.2–11.3).

### In-Game Vendor & Equipment Panels (Initial 14.x Integration)
Interactive panels (development placeholder visuals) demonstrate economy + equipment linkage:
* Toggle Vendor Panel: press `V` (lists up to 8 generated items from first loot table). Navigate with UP/DOWN, ENTER to attempt purchase (applies reputation discount, requires sufficient gold), BACKSPACE or `V` to close.
* Toggle Equipment Panel: press `E` (shows core stats snapshot and placeholder slot labels for weapon/armor).
* Restock Timer: shop auto-regenerates every 30s real time (simple timer; will be replaced by rotation logic using multiple tables).
* Stat Aggregation (14.2 partial): agility affix values on equipped items add to player dexterity each frame via `rogue_equipment_apply_stat_bonuses`.
* Unit Test: `test_equipment_stat_bonus` deterministically validates dexterity increase from an attached agility affix on an equipped weapon instance.

Planned enhancements: equip/unequip UI actions, durability display & expanded repair/reroll UI (initial repair hotkey in place), affix reroll interface, comparison tooltips & delta coloring, multi-table vendor rotation, sell/tab & buyback.

### Derived Stat Cache & DPS / EHP Estimators (14.3–14.4)
Implemented a lightweight caching layer (`stat_cache.c`) providing:
* Dirty Flag Invalidation: `rogue_stat_cache_mark_dirty` invoked on equip/unequip and when equipment bonuses apply.
* Cached Totals: Strength/Dexterity/Vitality/Intelligence post-equipment aggregation retained for UI queries.
* Heuristic DPS Estimate: `base_weapon_damage * (1 + DEX/50) * (1 + critChance% * critDamageMultiplier)`
* Heuristic EHP Estimate: `max_health * (1 + VIT/200)` ensuring monotonic increase with vitality.
* Equipment Panel Display: Real-time DPS & EHP shown beneath raw stat lines.

Rationale: Avoid recomputing scaling formulas & affix traversals every frame across multiple UI/tooling consumers while keeping deterministic results. Future expansions will incorporate armor / resistance mitigation and percent-based affix modifiers; current heuristics are intentionally simple but validated through the new `test_stat_cache` unit test (asserts DPS increases after equipping a weapon).

### Repair Currency Sink (10.4 partial integration)
Minimal repair loop implemented:
* Hotkey `R` while Equipment panel open repairs equipped weapon (if damaged) using gold.
* Cost: `rogue_econ_repair_cost(missing, rarity)`; API `rogue_equip_repair_slot` / `rogue_equip_repair_all`.
* Unit test `test_repair_costs` asserts durability restoration and exact gold deduction.
* Reroll UI & multi-item flows pending.

### Equip / Unequip Stat Delta Tests (14.5)
### Mini-map Loot Pings (12.4)
### Item Tooltips & Comparison (12.5–12.6)
### Loot Filter Predicate Tests (12.7)
Added comprehensive predicate evaluation test `test_loot_filter_predicates` covering:
* MODE=ALL intersection of rarity threshold + category
* MODE=ANY union of explicit def ids
* MODE=ANY mixture of name substring and rarity threshold (ensuring substring broadens visibility)
This finalizes 12.x filter correctness ensuring AND/OR semantics won't regress with future rule additions.

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
* Unit Test `test_minimap_loot_pings` validates spawn -> ping creation, active count increment, and expiry after lifetime advancement.

Unit test `test_equipment_unequip_delta` verifies stat bonus application on equip and removal on unequip, completing roadmap item 14.5.

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
| 10.x Economy & Vendors | Partial | Shop generation + rarity-based pricing implemented; restock & scaling pending |

Refer to `implementation_plan.txt` for granularity (over 150 line milestones).

---

## 11. Changelog & Latest Highlights

### [Unreleased]
Planned: Dynamic drop balancing (9.x), economy systems (10.x), crafting (11.x), loot filtering UI (12.x).

### UI System Phase 1 Progress
* Completed Phase 1.1–1.7 core architecture slice:
  - Module scaffolding, `RogueUIContext` lifecycle, panel & text primitives.
  - Dedicated deterministic UI RNG channel (xorshift32) isolated from simulation RNG.
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

### v0.8.x (Current Development Branch)
**Added**
* Action Point (AP) economy core (Phase 1.5): player AP pool + regen + per-skill cost + soft throttle + tests.
* Minimal EffectSpec system (Phase 1.2 partial): registry + stat buff kind + skill linkage + tests.
* Casting & channel basics (Phases 1.3/1.4 partial): added `cast_time_ms`, `cast_type` with delayed completion for casts and simple duration channel start tick; unit test `test_skill_cast_and_channel` validates timing.
* Advanced loot generation context (enemy level / biome / archetype / player luck).
* Seed mixing & deterministic multi‑pass generation.
* Affix gating rules by item category.
* Quality scalar (luck‑driven) influencing upper stat roll bias.
* Duplicate affix avoidance in prefix/suffix selection.
* Telemetry exports & histogram command.
* Global + per-category drop rate control layer (phase 9.1) enabling early balancing knobs.
* Adaptive weighting engine (phase 9.2) applying smoothed corrective factors (0.5x–2.0x) to rarity & category frequencies to reduce streaky droughts without hard forcing outcomes.
* Player preference learning (phase 9.3) tracking pickup counts to gently dampen over-picked categories.
* Accelerated pity thresholds (phase 9.4) reducing required consecutive misses after midpoint.
* Session metrics (phase 9.5) tracking per-session items dropped/picked, rarity distribution, and instantaneous items/hour + rarity/hour rates.
* Session metrics instrumentation (phase 9.5) exposing real-time items/hour and rarity/hour allowing balancing dashboards & future drift alerts to use normalized rates independent of session length.

**Improved**
* Rarity sampling integrates global floor + pity adjustments seamlessly.
* Persistence ensures affix data round‑trips without loss.

### Persistence Phase 1 (Core Save Architecture)
Implemented initial binary save system:
* Save descriptor (version=1, timestamp, component_mask, section_count, total_size, CRC32 payload checksum)
* Deterministic component ordering (sorted by id) for stable hashing/diffs
* Atomic temp write then rename for slot saves (save_slot_0..2.sav)
* Components implemented: player (level/xp/health/talent_points), world_meta (seed + generation params subset), inventory (active item instances + affix data), skills (ranks + cooldown_end), buffs (active list snapshot), vendor (seed + restock timers)
* Simple CRC32 integrity verification on load

Deferred (next phases): autosave ring, fsync durability, migration registry & SAVE_FORMAT_VERSION constant, per-section hashes, compression, structured schema evolution, rollback & recovery.

Roadmap file `roadmaps/implementation_plan_persistence_migration.txt` updated marking completed Phase 1 items.

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

