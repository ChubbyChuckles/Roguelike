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
| Integration | System taxonomy (15+ systems classified), integration manager lifecycle, dependency graph construction, health monitoring, resource tracking, capability matrix analysis | Event bus, hot-reload coordination, advanced orchestration |
| World Gen | Multi‑phase continents→biomes→caves→dungeons→weather→streaming | Advanced erosion SIMD, biome mod packs expansion |
| AI | BT engine, perception, LOD scheduler, pool, stress tests | Higher order tactics, adaptive difficulty tie‑ins |
| Combat & Skills | Attack chains, mitigation, poise/guard, reactions, AP economy, multi‑window attacks | DOTs, advanced status, combo extensibility |
| Loot & Rarity | Dynamic weights, pity, rarity floor, affixes, generation context | Economy balancing, adaptive vendor scaling |
| Equipment | Slots, implicits, uniques, sets, runewords, sockets, gems, durability, crafting ops, optimization, integrity gates | Deeper multiplayer authority, advanced enchant ecosystems |
| Persistence | Versioned sections, integrity hash/CRC/SHA256, incremental saves | Network diff sync, rolling signature policies |
| UI | Virtualized inventory, skill graph, animation, accessibility, headless hashing | Full theme hot swap diff + advanced inspector polish |
| Economy/Vendors | Pricing, restock rotation scaffold, reputation, salvage, repair | Dynamic demand curves, buyback analytics |
| Crafting | Recipes, rerolls, enchant/reforge, fusion, affix orb, success chance | Expanded material tiers, automation tooling |
| Progression | Infinite leveling design, Phase 0 stat taxonomy, Phase 1 infinite XP curve (multi-component, catch-up, 64-bit accum + tests), Phase 2 attribute allocation & re-spec (journal hash + tests), Phase 3 ratings & diminishing returns (crit/haste/avoidance DR + tests), Phase 4 maze progression framework (wrapper build, optional branches, gating, pathfinding + tests), maze skill graph UI (initial), stat scaling | Mastery loops (Phase 6), expanded passive rings |
| Difficulty | Relative ΔL model (enemy vs player level differential) + adaptive performance scalar (+/-12%) | Dynamic encounter budget + boss phase adaptivity |
| Tooling | Audit scripts, schema docs, diff tools, profilers, fuzz harnesses, **project restructuring system** | Extended golden baselines & editor integration |

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

### Integration Plumbing

**Phase 0: Integration Architecture Foundation** - Complete ✅
Cross-system communication architecture with comprehensive system taxonomy (15+ systems classified by type/priority/capability), dependency graph construction with circular dependency detection, integration manager with lifecycle state machine (Uninitialized→Running→Failed), health monitoring with exponential backoff restart, resource usage tracking, capability matrix analysis, and initialization sequence planning. Enables coordinated system orchestration and provides foundation for event-driven communication.

**Phase 1: Event Bus & Message Passing System** - Complete ✅  
Comprehensive event-driven communication layer featuring:
- **RogueEventBus**: Thread-safe central dispatcher with Windows/Linux compatibility
- **Event Type Registry**: 27 predefined event types covering entity lifecycle (CREATED, DESTROYED, MODIFIED), player actions (MOVED, ATTACKED, EQUIPPED, SKILLED), combat (DAMAGE_DEALT, CRITICAL_HIT, STATUS_APPLIED), progression (XP_GAINED, LEVEL_UP, SKILL_UNLOCKED), economy (ITEM_DROPPED, TRADE_COMPLETED, CURRENCY_CHANGED), world events (AREA_ENTERED, RESOURCE_SPAWNED), and system events (CONFIG_RELOADED, SAVE_COMPLETED, ERROR_OCCURRED)
- **Priority & Ordering**: Multi-level priority queue (Critical→Background) with deterministic timestamp+sequence ordering for replay consistency, event batching, coalescing, and deadline system
- **Subscription System**: Flexible API supporting conditional predicates, rate limiting (events/second), automatic lifecycle cleanup, and performance analytics
- **Processing**: Synchronous/asynchronous dispatch with time budget management, failure handling with retry logic, circuit breakers, and load balancing
- **Replay & Debugging**: Configurable event history recording, replay functionality, filtering, diff system, search capabilities, and development visualization tools  
- **Performance Monitoring**: Comprehensive latency measurement, queue depth alerts, memory tracking, throughput monitoring, regression detection, and automatic optimization

**Phase 2.1: JSON Schema Validation System** - Complete ✅
Advanced configuration standardization framework featuring:
- **RogueJsonSchema**: Comprehensive validation framework with registry management, field constraint validation (string length, integer ranges, enum values), type safety enforcement, and schema inheritance/composition support
- **Validation Engine**: Detailed error reporting with field paths, constraint violation detection, strict mode for unknown field prevention, and multi-error collection for comprehensive feedback
- **JSON Parser Integration**: Lightweight RogueJsonValue structures with type-safe manipulation API, memory management, and Windows MSVC compatibility
- **Builder System**: Helper functions for schema construction (add_field, set_required, set_description, set_range, set_string_length) enabling fluent schema definition patterns
- **Testing Foundation**: Comprehensive validation test suite covering registry management, field validation, constraint checking, and error reporting scenarios

**Phase 2.2: CFG to JSON Migration Infrastructure** - Complete ✅
Comprehensive configuration file analysis and migration framework featuring:
- **RogueCfgParser**: Advanced CFG file analysis system with automatic format detection (CSV, Key-Value pairs, Lists), data type classification (7 types: integer, float, string, boolean, path, ID, color), and file categorization (15 categories including Items, Affixes, Skills, UI, etc.)
- **File Analysis Engine**: Smart pattern recognition analyzing 78 existing CFG files, detecting field structures, value distributions, and migration complexity assessment with comprehensive statistics reporting
- **Migration Framework**: Infrastructure for systematic CFG-to-JSON conversion with validation integration, error recovery mechanisms, and progress tracking across priority categories
- **Real-World Validation**: Successfully analyzed 21 production CFG files from assets directory achieving 100% analysis success rate with detailed categorization and format detection
- **Cross-Platform Compatibility**: Windows MSVC and Linux GCC support with proper error handling, memory management, and comprehensive logging integration

**Phase 2.3: Priority Migration Categories** - Complete ✅
Operational CFG-to-JSON migration system with production-ready capabilities:
- **Live Migration Engine**: Complete CFG-to-JSON conversion system using Phase 2.2 parser infrastructure with support for CSV-based configuration files, JSON array output formatting, and comprehensive error recovery
- **Items & Equipment Migration**: Full implementation of Phase 2.3.1 converting item definitions from assets/test_items.cfg to structured JSON format with weapon/armor/consumable/accessory categorization, stat property preservation, and ID validation
- **Affixes & Modifiers Migration**: Complete Phase 2.3.2 implementation migrating affix definitions from assets/affixes.cfg to JSON with prefix/suffix type classification, stat range preservation, and weight distribution maintenance
- **Business Logic Validation**: Integrated validation rules including weapon damage requirement validation, min/max range consistency checks, affix type validation (PREFIX/SUFFIX), and cross-reference integrity checking
- **Production Pipeline**: Batch migration processing with Phase 2.3.1/2.3.2 specialized workflows, migration statistics reporting (files processed, records migrated, validation errors), formatted JSON output, and comprehensive error handling
- **Windows Integration**: Full Windows MSVC compatibility with security warning suppression, const-correct type casting, and proper API integration with existing CFG parser infrastructure

**Phase 2.4: Hot-Reload System Implementation** - Complete ✅
Live configuration reloading infrastructure with comprehensive change management:
- **File System Watching**: Cross-platform file system monitoring with Windows ReadDirectoryChanges and Linux inotify support, change detection with hash-based verification, and efficient polling algorithms
- **Change Detection**: SHA-256 content hashing for reliable change detection, incremental file monitoring, and false-positive elimination through content comparison
- **Staged Reloading**: Multi-phase reload process with validation-before-apply, atomic configuration updates, and graceful error recovery with rollback capabilities
- **Transaction System**: ACID-compliant reload transactions with commit/rollback semantics, dependency-aware batching, and consistency guarantees across related configuration files
- **Selective Reloading**: Intelligent system identification for minimal reload scope, dependency graph traversal, and impact-minimized configuration updates
- **Error Handling & Rollback**: Comprehensive error detection with automatic rollback to last-known-good configuration, corruption detection, and recovery mechanisms
- **Notification System**: Event-driven notifications to dependent systems, reload completion callbacks, and comprehensive logging with performance metrics

**Phase 2.5: Configuration Dependency Management** - Complete ✅
Advanced dependency resolution and impact analysis system:
- **Dependency Graph Construction**: Automated dependency discovery for configuration files with strong/weak relationship modeling, circular dependency detection using DFS algorithms, and efficient graph representation
- **Dependency Validation**: Runtime dependency resolution with missing reference detection, type consistency checking, and comprehensive validation reporting
- **Load Order Generation**: Topological sorting for dependency-aware loading sequence, priority-based ordering with conflict resolution, and deterministic load order generation
- **Impact Analysis**: Change propagation analysis identifying affected systems and files requiring reload, dependency chain traversal, and selective update recommendations
- **Weak Dependencies**: Optional reference support with graceful degradation, missing dependency tolerance, and flexible configuration relationships
- **Visualization Tools**: GraphViz export for dependency graph visualization, comprehensive debug output, and dependency relationship documentation
- **Performance Optimization**: Efficient dependency resolution algorithms, cached dependency graphs, and minimal overhead during normal operations

**Phase 2.9: Project Structure Reorganization** - Complete ✅
Comprehensive project restructuring system leveraging dependency management for safe file organization:
- **File Group Classification**: Organized 108+ core files into 8 logical subdirectories: integration/ (7 files), equipment/ (17 files), loot/ (27 files), vendor/ (20 files), crafting/ (11 files), progression/ (10 files), vegetation/ (4 files), enemy/ (12 files)
- **Dependency-Aware Restructuring**: Uses Phase 2.5 dependency manager for impact analysis, circular dependency prevention, and safe file movement validation with automatic rollback capabilities
- **Build System Integration**: Automated CMakeLists.txt updates, include path corrections, and test file path adjustments with comprehensive validation ensuring no regression
- **Restructuring Tools**: Complete toolchain including analyze_structure.c (project analysis), execute_restructure.c (interactive workflow), actually_move_files.c (physical operations), and demo_structure.ps1 (demonstration scripts)
- **Safety & Validation**: Transaction-based operations with commit/rollback semantics, dependency validation before movement, and comprehensive error recovery mechanisms
- **Windows Compatibility**: Full MSVC support with secure API usage, proper path handling, and Windows-specific file system operations

Testing: 342+ comprehensive integration plumbing tests + project restructuring validation suite - All Pass ✅

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

**Project Structure Reorganization**: Comprehensive dependency-aware file organization system that safely restructures projects into logical subdirectories (integration/, equipment/, loot/, vendor/, crafting/, progression/, vegetation/, enemy/) while automatically updating build files, include paths, and maintaining full system integrity through dependency manager integration.

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

**Project Structure Management**: Complete dependency-aware file organization system (`src/core/project_restructure.*`) enabling safe reorganization of the codebase into logical subdirectories. Includes interactive tools, impact analysis, build system integration, and comprehensive validation ensuring zero regression during structural changes.

---

## 10. Roadmap Snapshot
See `roadmaps/` for subsystem implementation plans (inventory, crafting & gathering, character progression w/ infinite leveling & maze skill graph, enemy difficulty ΔL model, vendors, dungeons, world bosses). Snapshot table preserved in Appendix A original Section 10.

---

## 11. Changelog (Curated Recent Highlights)
* Infinite leveling + maze skill graph design integrated (progression roadmap).
* Enemy difficulty ΔL relative scaling model adopted.
* Added vendor, dungeon, world boss comprehensive implementation plans.
* Integration Phase 3.9 (NEW): UI ↔ Systems Integration Bridge added. Real-time HUD bindings (health, mana, xp, level, gold), combat log ring buffer, inventory change ring buffer, skill/vendor/crafting/world map event capture (xp gain, currency change, trade, area enter, resource spawn), binding dirtiness enumeration API, metrics (events processed, dropped, per-channel counts). New test `test_phase3_9_ui_integration_bridge` (14 assertions) validates initialization, event flow, binding updates, world map updates, forced binding set. Roadmap 3.9.* entries marked Done.
* Integration Phase 3.10 (NEW): Persistence Integration Bridge added (`persistence_integration_bridge.[ch]`). Subscribes to progression/economy/world/config events (item pickup, xp gained, level up, skill unlock, trade completed, currency changed, area entered, config reload) and marks relevant save components dirty (player, world meta, inventory, entries, skills). Adds unified save/autosave/quicksave wrappers publishing SAVE_COMPLETED, incremental section reuse/write metrics, dirtiness query, integrity/tamper passthrough, validation helper iterating sections, and toggles for incremental + compression. Extends `save_manager` with `rogue_save_component_is_dirty` + `rogue_save_last_section_reuse`. New test `test_phase3_10_persistence_integration_bridge` covers initialization, reuse after clean save, event-driven dirtiness, mixed reuse/write behavior, section enumeration. Roadmap 3.10.* entries marked Done.
* Vendor System Phase 0 (baseline slice): Added material catalog (econ_materials.[ch]), deterministic item value model (econ_value.[ch]) with rarity ladder, slot factors, diminishing affix scalar p/(p+20), durability floor scalar (0.4+0.6f), integrated vendor pricing path, and unit test `test_vendor_phase0_value` (catalog population, rarity & affix monotonicity, durability ordering).
  * Added inflow baseline simulation (econ_inflow_sim.[ch]) + `test_vendor_phase0_inflow` (reference kills/min arithmetic, value composition) completing Phase 0 (0.4).
* Vendor System Phase 1 (complete): Registries for vendors, price policies, reputation tiers, and negotiation rules with JSON assets (`vendors.json`, `price_policies.json`, `reputation_tiers.json`, `negotiation_rules.json`) + template entries. Loader prefers JSON with fallback to legacy `.cfg`; uniqueness audit detects duplicate IDs. Tests: `test_vendor_phase1_registry` (counts, linkage, ordering, negotiation presence) and `test_vendor_phase1_negotiation` (collision scan, field invariants, forward compat extra field ignored).
* Vendor System Phase 2 (complete – 2.1–2.5): Inventory template registry (`inventory_templates.json`) + deterministic seed (`rogue_vendor_inventory_seed`) and new constrained generation API `rogue_vendor_generate_constrained` enforcing uniqueness, rarity caps (legendary<=1, epic<=2, rare<=4 with graceful rarity downgrade when cap hit), guaranteed consumable slot, material injection, and optional recipe blueprint injection. Deterministic ordering (sorted def indices) for hashing/replay. Tests: `test_vendor_phase2_inventory` (templates + seed) and `test_vendor_phase2_constrained` (constraints, determinism, caps) – passing.
* Vendor System Phase 3 (Done 3.1–3.5): Added pricing engine module (`vendor_pricing.[ch]`) composing base econ value + condition scalar + vendor policy margins (rarity & category modifiers) + reputation discount/bonus + negotiation discount + dynamic demand scalar (EWMA sales vs buybacks) + scarcity multiplier (long‑term EWMA mapped ~[0.9,1.2]) + clamp/rounding. Integrated into vendor price formula. Test `test_vendor_phase3_pricing` validates ordering, demand monotonicity, condition scaling, negotiation & reputation effects, margin differential, and scarcity upward pressure under sustained sales.
* Vendor System Phase 4 (Done 4.1–4.5): Implemented reputation & negotiation module (`vendor_reputation.[ch]`) with per-vendor rep accumulation, diminishing returns logistic scaling (min scale 0.15 near tier), deterministic negotiation attempts (attribute-mapped skill tags insight->INT, finesse->DEX + d20 style roll), uniform discount range, success/ failure lockouts (5s / 10s) and penalties (rep -1 on failure). Test `test_vendor_phase4_reputation_negotiation` validates diminishing rep deltas and higher success probability with higher attributes.
* Vendor System Phase 5 (Done 5.1–5.5): Added buyback subsystem (`vendor_buyback.[ch]`) per-vendor circular buffer (cap 32) storing sold item guid, base def, rarity, condition, original price, timestamps. Implemented depreciation (10% per minute to 50% floor) and assimilation timeout (5 min -> inactive placeholder for future stock merge). Added hash-chained transaction journal (`vendor_tx_journal.[ch]`) using FNV1a (sale/buyback actions). Introduced recent GUID lineage ring (128) for duplicate sale exploit detection. Test `test_vendor_phase5_buyback_journal` covers ring wrap, monotonic depreciation, journal hash determinism (replay identical), and duplicate guid detection.
* Vendor System Phase 6 (Done 6.1–6.5): Implemented special offers module (`vendor_special_offers.[ch]`) supporting up to 4 concurrent time‑limited offers (10 min expiry) with scarcity weighting (select lowest base_value material), Nemesis performance hook (25% blueprint injection chance, rarity 4 bias), and pity timer (guarantee after 3 consecutive miss cycles). Offers store origin flags (nemesis/scarcity) and pricing snapshot via existing pricing engine. Test `test_vendor_phase6_special_offers` validates offer persistence across rolls, expiry compaction, nemesis rare injection occurrence, and bounded slot usage.
* Vendor System Phase 7 (Done 7.1–7.5): Added currency & sink mechanics module (`vendor_sinks.[ch]`) with categorized sink accumulation (repair/upgrade/trade-in/fees), upgrade service API (`rogue_vendor_upgrade_reroll_affix`) performing single or dual affix rerolls (dual consumes catalyst) with gold fee scaling by player level (linear factor 1+0.015*level), unfavorable material trade‑in service (6:1 + gold fee scaled by source material value), and cumulative sink telemetry helpers. Test `test_vendor_phase7_sinks` exercises upgrade & trade-in paths and verifies sink accounting.
* Vendor System Phase 8 (Done 8.1,8.3–8.5): Added crafting integration module (`vendor_crafting_integration.[ch]`) implementing recipe unlock purchase API (idempotent unlock with gold fee), batch refinement service (loops `rogue_material_refine` with percentage gold fee), and scarcity feedback loop (deficit record/score for gathering weighting). Test `test_vendor_phase8_crafting_integration` validates recipe unlock (no double charge), scarcity accumulation, and batch refine invocation.
* Vendor System Phase 9 (Done 9.1–9.5): Introduced adaptive behavior module (`vendor_adaptive.[ch]`) tracking per-category purchase & sale counts, computing smoothstep preference boosts (up to +15%) for under-purchased categories injected into constrained inventory generation. Implemented rapid flip exploit detection (purchase->sale within 15s) accumulating pairs to raise vendor sell prices slightly (+1% per pair, max +10%). Added batch purchase gating (>8 purchases in 10s triggers 5s cooldown). Pricing engine now includes exploit scalar when vendor selling. Unit test `test_vendor_phase9_adaptive` validates scalar bounds, boost activation, exploit scalar >1 after flip, and cooldown behavior (token VENDOR_PHASE9_ADAPTIVE_OK).
* Vendor System Phase 10 (Done 10.1–10.3,10.5): Added multi-vendor balancing module (`vendor_econ_balance.[ch]`) implementing global inflation index (EWMA alpha 0.05), automated margin rebalance (global scalar adjusts ±5% over ±0.5 drift, clamped [0.90,1.10]), and deterministic regional biome variance scalar [0.97,1.03] via FNV1a of biome tags. Pricing engine composes demand, scarcity, exploit, global, and biome scalars then feeds observed price back into inflation tracker. Test `test_vendor_phase10_balancing` validates inflation index bounds, global scalar range, biome scalar envelope, and success token VENDOR_PHASE10_BALANCING_OK. (10.4 competition events left optional X.)
* Vendor System Phase 11 (Done 11.1,11.2,11.4,11.5): Introduced performance/memory module (`vendor_perf.[ch]`) with SoA arrays for demand/scarcity/last refresh tick, configurable slice scheduler (default slice 8) to batch vendor refresh work, and memory usage query. Unit test `test_vendor_phase11_perf` initializes 50 vendors, asserts memory footprint <16KB, runs scheduler slices to cover all vendors within 6 ticks (token VENDOR_PHASE11_PERF_OK). (11.3 micro-bench for 200 vendors deferred.)
* Vendor System Phase 12 (Done 12.1–12.5): Added RNG governance (`vendor_rng.[ch]`) with multi-stream seed composition (INVENTORY, OFFERS, NEGOTIATION) using world_seed ^ vendor_id ^ refresh_epoch ^ stream id avalanche. Integrated governed seeds into `rogue_vendor_generate_constrained` and `rogue_vendor_offers_roll` (negotiation already used deterministic attempt seed). Implemented snapshot hashing (`rogue_vendor_snapshot_hash`) combining sorted inventory (defs/rarities/prices), world seed, vendor id, refresh epoch, and pricing modifier hash (`rogue_vendor_price_modifiers_hash`). Added pricing modifier hash capturing demand/scarcity EWMA state. New unit test `test_vendor_phase12_determinism` validates identical snapshot hash under replay and divergence across epoch change (token VENDOR_PHASE12_DETERMINISM_OK). Roadmap Phase 12 entries marked Done.
* Equipment integrity gates (fuzz, mutation, snapshot, proc stats) formalized.
* Expanded crafting: sockets, gems, enchant/reforge, fusion, affix orbs, success chance.
* NEW: Material Registry (Crafting & Gathering Phases 0–1) – unified cfg-driven taxonomy (`material_registry.[ch]`) linking material entries to item defs (tier, category, base value) with deterministic ordering, duplicate protection, seed mixing utility for future gathering node generation. Test `test_crafting_phase0_1_material_registry` covers parsing & lookup invariants.
* NEW: Gathering Nodes (Crafting & Gathering Phase 2) – `gathering.[ch]` added: node defs parsing (weighted material table, roll range, respawn, tool tier, spawn chance, rare proc chance & multiplier), deterministic per-chunk spawn hashing, harvest API with tool gating & depletion/respawn, rare proc yield multiplier & counters. Test `test_crafting_phase2_gathering` validates gating, spawn presence, respawn cycle, rare proc path, and analytics counters.
* NEW: Material Quality & Refinement (Crafting & Gathering Phase 3) – `material_refine.[ch]` introduces per-material quality buckets (0..100), aggregation helpers (average, bias) and refinement operation converting source quantity with outcome branches (10% failure reduced yield, 5% critical boosted yield + partial promotion). Test `test_crafting_phase3_refinement` exercises ledger seeding, multiple refinements, and prints CRAFT_P3_OK.
* NEW: Crafting Recipes Phase 4.1–4.2 – Extended `RogueCraftRecipe` schema (time_ms, station, skill_req, exp_reward) and parser with forward-compatible token handling; unit test `test_crafting_phase4_recipes` validates parsing & execution (CRAFT_P4_OK). Remaining queue/station registry features pending.
* NEW: Crafting Queue Phase 4.3–4.6 – Added station registry (forge, alchemy_table, workbench, mystic_altar) with capacity model (forge=2, others=1), deterministic FIFO crafting queue (`crafting_queue.[ch]`) supporting timed jobs, promotion, cancellation (full refund waiting, 50% active refund), and unit test `test_crafting_phase4_queue` (capacity enforcement, timing, cancel invariants) printing CRAFT_P4Q_OK. Phase 4 now fully Done.
* NEW: Crafting Skill & Proficiency Phase 5.1–5.5 – Added `crafting_skill.[ch]` (per-discipline XP/levels, perk tiers: material cost & speed reductions, duplicate output chance, quality floor hook), integrated perks into queue enqueue & completion (cost/time scaling, XP gain, deterministic duplicate outputs), recipe discovery bitset + dependency unlock, and unit test `test_crafting_phase5_skill` validating XP gain & perk activation (CRAFT_P5_OK).
* NEW: Enhancement Pathways Phase 6 – Added `equipment_enhance.[ch]` implementing Imbue (new affix with catalyst & budget clamp), Temper (increment affix with fracture risk on failure via durability damage), socket add & reroll operations (deterministic seed), integrated fracture state impact (damage scaling already honored), and unit test `test_crafting_phase6_enhancement` validating budget preservation, affix mutation, socket APIs, and failure path execution (CRAFT_P6_OK).
* NEW: Crafting & Gathering Phase 7 – Determinism & RNG Governance: multi-stream RNG (gathering/refinement/quality/enhancement) via `rng_streams.[ch]`, append-only crafting journal with FNV-1a hash chaining (`crafting_journal.[ch]`), and tests (`test_crafting_phase7_rng_journal`, `test_crafting_phase7_determinism`) ensuring stream isolation & replay hash stability.
* NEW: Crafting & Gathering Phase 8 – UI / UX Layer slice: Added `crafting_ui.[ch]` providing recipe panel (search filter, batch-aware availability dim, upgrade tag heuristic), enhancement risk preview (expected fracture damage), material ledger panel (avg quality + bias), queue progress list, batch quantity slider state, gathering node overlay (respawn timers & rare flag), accessibility text-only fallback (stdout tokens), and unit test `test_crafting_phase8_ui` validating search persistence, batch qty state, text-only mode, and risk function bounds.
* NEW: Crafting & Gathering Phase 9 – Automation & Smart Assist: Added `crafting_automation.[ch]` with deterministic advisory APIs (craft planner with optional recursive expansion, gathering route suggestion via scarcity coverage heuristic, auto-refine suggestions for low-quality buckets, salvage vs craft decision scoring, idle material recommendation). Included unit test `test_crafting_phase9_automation` (token: CRAFT_P9_OK) exercising all helper paths. Roadmap Phase 9 marked Done.
* NEW: Character Progression Phase 9 – Synergy & Caps: Added `progression_synergy.[ch]` implementing unified layered damage aggregation (`rogue_progression_layered_damage`), crit chance soft/hard caps (60% soft with diminishing to 95% hard), cooldown reduction soft/hard caps (50% -> 70%), equipment -> skill tag bridge via weapon infusion mapping (`rogue_progression_synergy_tag_mask`), and conditional fire synergy helper (`rogue_progression_synergy_fire_bonus`). Included unit test `test_progression_phase9_synergy` validating ordering math, attribute layering, cap enforcement, and tag-gated bonus. Roadmap Phase 9 entries marked Done.
* NEW: Character Progression Phase 10 – Buff/Debuff Integration: Extended stat cache layering to include timed buff strength contributions post-passives; introduced buff stacking rules (unique, refresh, extend, add), snapshot flag, dampening interval to prevent rapid flicker, and public strength bonus accessor. Added unit test `test_progression_phase10_buffs` covering stacking semantics, buff layer aggregation, and dampening behavior. Roadmap Phase 10 updated to Done.
* NEW: Character Progression Phase 11 (Performance & Caching – Initial): Added incremental dirty bit system with layer mask (attr=1, passive=2, buff=4, equipment=8) & selective recompute in stat cache; instrumentation counters (`recompute_count`, `heavy_passive_recompute_count`); unit test `test_progression_phase11_cache` verifies passive layer skipped on buff-only dirties and struct size (<2 KB) far below <64 KB target. Roadmap 11.1 & 11.4 marked Done; 11.5 Partial (latency micro-bench pending).
* NEW: Character Progression Phase 12 (Persistence & Migration – Initial): Added `progression_persist.[ch]` with versioned progression header (V1/V2) capturing level, total XP, attributes, spent points, respec tokens, attribute & passive journal hashes, and stat registry fingerprint placeholder for migration. Implemented save component (id=27), chain hash over critical fields, migration flag for stat registry changes, and unit test `test_progression_phase12_persistence` covering roundtrip + legacy V1 load.
* UPDATE: Character Progression Phase 12 (Persistence & Migration – Complete): Added V3 header with passive unlock sparse list & attribute operation journal serialization, migration flags (TALENT_SCHEMA, ATTR_REPLAY), replay logic for legacy V1/V2 saves, extended test `test_progression_phase12_v3` validating roundtrip, chain hash stability, backward compatibility, and journal replay counts. Roadmap 12.2–12.5 marked Done.
* UPDATE: Character Progression Phase 11 (SoA & Micro-Bench): Implemented SoA mirrors for passive effects (stat_id/delta arrays) used in unlock accumulation loop, reducing indirection & improving cache locality; added micro-benchmark `test_progression_phase11_bench` comparing full vs buff-only recompute (expects >5% speedup) fulfilling 11.2 & 11.3. Roadmap updated; 11.5 remains Partial (P95 timing & re-spec stale verification pending).
* UPDATE: Character Progression Phase 11 (Re-spec Cache Integrity): Added `test_progression_phase11_respec` ensuring stat cache fingerprint & passive totals change after passive DSL reload + replay (guards against stale cache). 11.5 now Done for functional correctness after integrating passive_* layer into stat cache (P95 latency metric deferred).
* NEW: Crafting & Gathering Phase 10 – Economy & Balance Hooks: Added `crafting_economy.[ch]` (scarcity metric + dynamic spawn scalar, crafting XP inflation guard with diminishing scalar, soft cap pressure for high-tier raw accumulation, enhanced value model incorporating material quality + tempered rarity curve). Unit test `test_crafting_phase10_economy` prints token CRAFT_P10_OK. Roadmap Phase 10 marked Done.
* NEW: Crafting & Gathering Phase 11 – Analytics & Telemetry: Added `crafting_analytics.[ch]` tracking gathering nodes/hour & rare rate, craft output quality histogram & success ratio, enhancement risk variance, material flow acquisition/consumption, drift alert (avg quality deviation), JSON export API; unit test `test_crafting_phase11_analytics` emits token CRAFT_P11_OK.
* NEW: Progression Phase 6.2–6.5 – Mastery System Extensions: Added extended mastery implementation (`progression_mastery.[ch]`) featuring ring point unlocks (skills reaching rank>=5), bracketed mastery bonus scalar tiers (1.01–1.20), optional inactivity decay (grace 60s then 10% surplus decay per 15s window), and extended unit test `test_progression_phase6_mastery_extended` validating rank growth, ring currency accrual, bonus ordering, and decay behavior.
* NEW: Progression Phase 5 – Passive Unlock Integration: Added `progression_passives.[ch]` implementing DSL for node stat effects (e.g., `0 STR+5 DEX+2`), compact effect tables, unlock journal with FNV-1a hash chaining, stat accumulation snapshot, duplicate unlock prevention, and hot reload replay validator preserving journal hash. Unit test `test_progression_phase5_passives` validates DSL parse, totals, duplicate rejection, hash delta & reload invariants (token PROG_P5_OK).
* NEW: Progression Phase 7 (Scaffold) – Added ring expansion milestone function (`rogue_progression_ring_expansions_unlocked`) granting +1 outer ring at level 50 then every +25 levels (capped +4) and provisional keystone heuristic flagging high-degree outer ring nodes. Added meta flag `ROGUE_MAZE_FLAG_KEYSTONE`, helper queries, and unit test `test_progression_phase7_rings` ensuring milestone logic & optional-keystone exclusivity. Roadmap updated (7.1–7.3,7.5 Partial).
* UPDATE: Progression Phase 7 (Dynamic Expansion) – Implemented `rogue_progression_maze_expand` for procedural outer ring growth (node placement + adjacency rebuild). Extended Phase 7 test now validates expansion (added ring count, keystone invariants). Roadmap marks 7.1 & 7.2 Done, 7.3 Partial (structural anti-stack), 7.5 Done initial.
* UPDATE: Progression Phase 7 (Anti-Stack Safeguards) – Implemented per-category (offense/defense/utility) keystone diminishing returns: coefficient = 1/(1+0.15*(k-1)) applied at passive unlock time with effect classification heuristic (STR/DEX/CRIT -> offense, resist/armor -> defense, others -> utility). Added keystone count query APIs and unit test `test_progression_phase7_antistack` (verifies reduced second offense increment and independent defense keystone value). Roadmap 7.3 marked Done.
* NEW: Progression Phase 7 (Visualization Layering) – Added ring layer extraction (`rogue_progression_maze_layers`), node polar projection (`rogue_progression_maze_project`), and ASCII overview generator (`rogue_progression_maze_ascii_overview`) to support zoomable UI rendering of dynamically expanded passive rings. Unit test `test_progression_phase7_visualization` validates layer monotonicity, projection success, and non-empty plot output. Roadmap 7.4 marked Done.
* NEW: Progression Phase 8 (Perpetual Scaling Layer) – Introduced micro-node framework (`progression_perpetual.[ch]`): continuous spend allowance (level/2 + milestone bonus), diminishing per-node contributions (BASE/(1+CURV*i)), sublinear level scalar `(1-exp(-L/140))^0.80`, adjustable global coefficient with inflation guard adjust (targets stable TTK), and unit test `test_progression_phase8_perpetual` verifying allowance, diminishing increments, and sublinear 100→200 effective power ratio.
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
* Enemy Difficulty Phase 5 (AI behavior intensity layers): Added intensity system (Passive→Frenzied) with action/move/cooldown multipliers, escalation triggers (proximity, player low health, recent pack deaths), de-escalation drift & hysteresis cooldown to prevent oscillation, integrated into movement speed, attack cooldown, and animation pacing; unit test drives escalation to Frenzied then relaxation.

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
Enemy Integration Phase 1 (Deterministic Seeds & Replay Hash): Added encounter seed derivation API (`rogue_enemy_integration_encounter_seed`) implementing world_seed ^ region_id ^ room_id ^ encounter_index, replay hash generator (`rogue_enemy_integration_replay_hash`) summarizing template id + unit levels (modifier ids placeholder for Phase 3), and a debug ring buffer with dump API for last 32 encounters. New test `test_enemy_integration_phase1` validates seed reproducibility (two passes identical) and divergence under a different world seed. Roadmap Phase 1 (1.1–1.5) marked Done.
Enemy Integration Phase 2 (Encounter Template → Room Placement): Implemented room-based encounter template selection with depth-based logic (`rogue_enemy_integration_choose_template`), room difficulty computation incorporating depth, area, and tags (`rogue_enemy_integration_compute_room_difficulty`), template placement validation with space requirements, and graceful fallbacks to basic templates. Added complete encounter preparation pipeline (`rogue_enemy_integration_prepare_room_encounter`) combining seed derivation, template selection, and validation. New test `test_enemy_integration_phase2` provides comprehensive coverage of template selection determinism, difficulty calculation, and placement validation. Roadmap Phase 2 (2.1–2.5) marked Done.
Enemy Integration Phase 3 (Stat & Modifier Application at Spawn): Implemented stat application with elite scaling bonuses (1.5x HP, 1.2x damage, 1.1x defense), probabilistic modifier system with type-based application chances (boss: always, elite: 75%, normal: 20%), and budget cap system (boss: 1.0, elite: 0.8, normal: 0.6) for modifier selection. Added comprehensive spawn finalization pipeline (`rogue_enemy_integration_finalize_spawn`) combining stats, modifiers, and encounter metadata with extensive validation (`rogue_enemy_integration_validate_final_stats`). New test `test_enemy_integration_phase3` validates stat consistency, modifier determinism, and budget adherence. Roadmap Phase 3 (3.1–3.5) marked Done. Enemy integration now supports complete encounter generation with proper stat scaling and modifier application.
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
New tile types: `ROGUE_TILE_LAVA`, `ROGUE_TILEORE_VEIN`. Implementation in `world_gen_local.c` using micro RNG channel; roadmap Phase 4 items marked Done.

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
* Phase 5.1 Skill Graph (Initial): Added zoomable, pannable skill graph API with quadtree culling and node emission (base icon panel placeholder, rank pip bars, synergy glow ring) and unit test `test_ui_phase5_skillgraph` validates visible subset emission & glow presence.
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

Demo Layout: The runtime builds a radial distribution of placeholder nodes with sample ranks, synergy flags, and tag bits to exercise quadtree culling + layered emission (background, glow, ring, icon placeholder, pip bar, rank text) and unit test `test_ui_phase5_skillgraph` validates visible subset emission & glow presence.

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
  - Affix reroll API (`rogue_item_instance_reroll_affixes`) consuming materials + gold, delegating to existing affix roll generator; integrates economy & new inventory consume API.
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
* Affix selection uses on-stack arrays; no heap fragmentation during generation.
