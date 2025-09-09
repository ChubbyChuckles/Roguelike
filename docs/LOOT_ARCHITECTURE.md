# Loot System Architecture

This document (Phase 23.1) provides a contributor‑oriented deep dive into the loot subsystem design so new features can be integrated *consistently* and *safely*.

## 1. Layered Phases Overview
The implementation proceeds in numbered phases (1..22+) each adding an isolated capability with deterministic tests. Every public API added in a phase must:
1. Use `rogue_` prefix.
2. Avoid leaking internal structs (only POD or opaque handles).
3. Provide at least one regression test enumerating boundary cases.

## 2. Core Data Flow
```
[Config Files] -> (Parsing) -> Item / Affix Registries
   |                                    |
   +--> Loot Tables  -------------------+
                (roll) -> Rarity -> Base Item -> Affixes -> Instance (pool)
                                                 |           |
                                          (Telemetry)   (Persistence)
```

## 3. Determinism Contract
* Single LCG (`state = state*1664525 + 1013904223`) seeded explicitly per roll path.
* Multi‑pass generation: rarity -> item -> prefix -> suffix -> affix value(s).
* Hash helpers (e.g., `rogue_loot_roll_hash`) produce platform‑stable FNV1a values for verification and server reconciliation.

## 4. Module Boundaries
| Module | Responsibility | Notes |
|--------|----------------|-------|
| `loot_item_defs.*` | Parse & store base item definitions | Max cap enforced (`ROGUE_ITEM_DEF_CAP`). |
| `loot_tables.*` | Weighted table parsing & rolling | Supports quantity ranges & rarity band overrides. |
| `loot_rarity.*` / `loot_rarity_adv.*` | Rarity selection + pity + floors | Advanced weighting isolated. |
| `loot_affixes.*` | Affix registry + value rolls | No allocation during rolls (static arrays). |
| `loot_generation.*` | Orchestrates multi‑pass roll | Accepts context (enemy level, biome, luck). |
| `loot_instances.*` | Ground instance pool + despawn | O(1) spawn & merge scanning. |
| `inventory.*` / `inventory_ui.*` | Player ownership & UI projection | Keeps UI pure (no mutation). |
| `loot_perf.*` | Performance metrics & scratch pools | SSE2 fast path guarded by feature checks. |
| `loot_security.*` | Roll hashing, tamper checks, anomaly detection | Optional server authoritative verification. |
| `loot_analytics.*` | Telemetry ring buffers, heatmap | Decoupled from generation for minimal overhead. |

## 5. Performance Principles
* No heap allocations inside hot roll path (affix selection / rarity sampling / table iteration).
* Scratch buffers (weights) acquired from pool (`loot_perf.c`) and released deterministically.
* SIMD (SSE2) used for weight summation; scalar fallback keeps portability.

## 6. JSON Item System Integration

The loot system now supports JSON-first item definitions with comprehensive schema validation and graceful fallbacks:

### Loading Pipeline
```
[JSON Directory] -> Schema Validation -> Item Registry
       |                                      |
   [CFG Directory] -> Legacy Parser ----------+
       |                                      |
  [Single CFG File] -> Fallback Parser ------+
```

**Loading Functions:**
- `rogue_item_defs_load_directory_json()` - Primary JSON loader
- `rogue_item_defs_load_directory()` - CFG directory fallback  
- `rogue_item_defs_load_from_cfg()` - Single file fallback

### Schema Validation
JSON items are validated against a comprehensive schema with:
- Required fields: `id`, `name`, `category`, `level_req`, `stack_max`, `base_value`
- Optional fields: damage, armor, sprites, rarities, implicit stats, sockets
- Per-file validation with partial failure recovery
- Migration hooks for schema version evolution

### Integration Points
All subsystems use unified APIs that work with both JSON and CFG loaded items:
- **Crafting**: `rogue_item_def_index()` for material references
- **Equipment**: `rogue_item_def_at()` for stat calculations  
- **Inventory**: `rogue_item_def_at()` for filtering and display
- **Persistence**: `def_index` values maintain save compatibility

### Performance Optimizations
- Fast hash-based lookups via `rogue_item_defs_build_index()`
- O(1) average lookup with `rogue_item_def_index_fast()`
- JSON parsing overhead <10% vs CFG loading
- Memory footprint identical between formats

### Asset Structure
```
assets/items/
├── weapon_iron_sword.json
├── potion_small_heal.json  
├── armor_leather_cap.json
├── material_iron_ore.json
└── misc_scroll_town_portal.json
```

### Vendor Integration
Vendor inventory templates maintain category alignment:
- Category weights array matches `RogueItemCategory` enum order
- JSON items properly categorized for vendor selection
- Price policies work identically with JSON definitions

## 7. Persistence Integration
Item instances serialize with: definition index, quantity, rarity, affix ids/values, durability, enchant, ownership. Backward compatibility maintained via size heuristics; new fields append only.

## 8. Security / Cheat Resistance Hooks
* Verify reported roll: recompute FNV1a hash -> compare.
* Seed obfuscation optional: mix(seed,salt) before roll.
* Config tamper detection: snapshot combined file hash & verify periodically.
* Anomaly detector observes high rarity frequency spikes (windowed, decayed) -> flag for tooling.

## 9. Testing Strategy
Each phase contributes at least one test prefixed `test_loot_phase<phase>_...` ensuring:
* Deterministic reproduction under fixed seeds.
* Boundary enforcement (caps, missing lines, malformed rows, rarity distribution tolerance).
* Security primitives (hash stability, tamper unchanged, anomaly spike detection) remain functional.

## 10. Extension Guidelines
When adding a new feature:
1. Decide target phase (append sub‑phase if small, else create next integer).
2. Update roadmap entry from `X` to `In Implementation` then to `Done` when tests pass.
3. Keep new public APIs minimal; prefer extending context structs / enums privately.
4. Document new APIs in `loot_api_doc.c` entry list.
5. Add style guide note if affix / rarity semantics change.

## 11. Failure Modes & Error Codes
* Parsing returns count added; negative = fatal open/format error.
* Roll APIs return produced count (0 valid if table empty after filtering).
* Security verify returns: 0 match, 1 mismatch, <0 IO/config error.

## 12. Known Future Work
* Data‑driven affix gating rules (currently hard‑coded).
* Expanded anomaly classifier (statistical z‑score baseline).
* Optional network serialization of roll intent for replay/audit.

---
Maintained with roadmap Phase 23.1 status. Contributors should reference this before altering cross‑module contracts.
