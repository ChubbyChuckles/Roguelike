## Debug overlay tips

Press F1 in-game to open the debug overlay.

- Panels selector: A small "Panels" window appears in the top-right. Use its checkboxes to toggle which panels are visible. The selector itself can’t be hidden to avoid lock-out.
- Command Palette: Press Ctrl+Shift+P to open a palette of overlay commands. Type to filter; Enter to run. Defaults include Validation (Run Now/Show Panel), Skills (Save/Load Overrides), Content Graph (Export DOT), and quick “Open Items/Skills/Map”. You can register your own commands at startup.
- Global Search: Press Ctrl+K to search across Items and Skills by id or name (case-insensitive substring). Up/Down navigate results; Enter jumps to the right panel, opens it, selects the entry, and scrolls it into view. Initial scope covers Items/Skills; more registries and fuzzy ranking are planned.
- Navigation history & breadcrumbs: Use Alt+Left/Right to move Back/Forward through your overlay navigation history (panel + selection). Supporting panels render a breadcrumb header (e.g., Items > Weapons > iron_sabre) that updates as you change selection. Global Search jumps are tracked in history.
- Toast notifications: Non‑modal toasts appear in the overlay for common actions (info/warn/error) and auto‑dismiss. You’ll see them when saving/loading Skills overrides, when Auto‑Reload applies changes, when Validation starts/completes, and when running actions from the Command Palette. They’re headless‑safe and won’t interfere with input.
- Theme & Accessibility: In the System panel, adjust Theme Preset (Dark, Light, High Contrast), DPI scale, and Font Size. Your choices persist to `build/overlay_theme.json`. Core widgets have been migrated to use theme colors for consistent visuals and better contrast. Map and Audio/VFX panels now consume theme accents for their world-space gizmos (brush ghost, colliders, path heatmap, autotile hints, falloff ring). Entities panel adds a themed world‑space selection gizmo (rect + crosshair) around the selected entity using theme accents; toggle it in the Entities panel options.
- Items panel: Type in the search box to filter by id or name (case-insensitive). Click table headers to sort (toggles asc/desc). The list is virtualized for large registries; scroll with the mouse wheel while hovering the table or use Up/Down keys. Home/End jump to top/bottom. Hold Shift to accelerate step.
  Now includes a right-side vertical scrollbar: click the track to page and drag the thumb to scroll; it stays in sync with the virtualized row window.
  Default row density is tuned for performance: row height 16px with 1px padding (adjustable via the Row Height slider). Perf smoke tests on 50k rows showed a small but consistent frame-time improvement over the previous 18px/2px default.
  Templates & Presets: In the Create New Item wizard, use curated Presets to prefill common item shapes, or save your own Templates and reapply later. "Save as Template" writes a JSON fragment to `build/templates/items/<name>.json` (atomic). "Load Template" applies fields back to the wizard. Toasts confirm success or report errors; all paths are headless-safe.
  Batch Create (Items): Pattern expansion with preview and validation. Provide Id Format and Name Format using printf-style specifiers (e.g., "iron*sword*%03d", "Iron Sword %d"), plus Start and Count. The preview flags invalid formats and duplicate ids. Apply creates items via the current Create wizard fields; a toast summarizes successes/failures.
  Items Preview (M4.1): The Create wizard shows a live textual preview under "Preview" summarizing Category, Rarity, Level, Base Stats (Damage dmin–dmax or Armor), Sockets (min..max), and an informational Est. Value heuristic. Updates immediately as you edit; headless-safe.
- Skills Effects Node Graph: Canvas, node fills/borders, edge lines, and labels are now driven by the overlay theme for consistent contrast across presets (Dark/Light/High-Contrast). Linking and node selection visuals adapt to accent colors.
- Content Graph: SDL preview and diagnostics visuals consume overlay theme colors, including accents and text tones, improving readability and consistency.
  Controls (M5.1): In the mini‑view, Right‑mouse drag pans, the mouse wheel zooms (cursor‑centered; 0.5×–2.0× clamp), and F fits the selection. A toggle "Isolate subgraph (limit list)" filters the list to nodes reachable from the current selection up to the current Preview depth. Click a node in the mini‑view to select/drill and update the crumb trail; a small hint inside the view reminds: "RMB drag=pan, Wheel=zoom, F=fit". Headless‑safe: drawing is guarded and won’t run in tests.
- Map panel (M4.4): Live previews for authoring. A brush ghost shows your current footprint (square/rect). Optional overlays visualize colliders and a pathfinding heatmap. Autotiling hints highlight neighbors the placement would affect. A local 64‑step Undo/Redo ring protects edits (Ctrl+Z / Ctrl+Y) with a Clear History button. All visuals are SDL‑guarded with headless‑safe textual fallbacks.
- Dialogue panel (M4.5): Textual live preview of the current line and a mini branch graph (ASCII list with a current marker). Includes a simple conversation runner (Start/Advance/Reset) and an optional style inspector that applies via debug APIs. Auto-registers a sample script when none are present so you can try it immediately. Headless‑safe; SDL‑guarded where applicable.
- Entity inspect: In the Entities panel, hold Shift and LeftClick on a unit in the world to select and inspect it. This uses the camera and tile mapping to pick the nearest enemy under the cursor.
  Duplicate & batch tools: With an enemy selected, use "Duplicate Enemy" to spawn a copy with configurable dx/dy offset (headless‑safe). The "Batch Creator (spawn copies)" can place a ring of N duplicates around the player at a chosen radius (8‑directional pattern, repeats as needed). Useful for quick combat/AI triage.
- Skills: The Skills panel now uses top-level tabs: Overview, Effects, Visuals, Audio, and Testing. The Overview tab includes the Create New Skill wizard with a template workflow. Select a template skill id and click "Apply Template" to prefill name/max-rank/passive/timing; optionally enable "Copy Coeffs" to duplicate coefficient params to the preview and to the created skill. New: a searchable template picker (filter + table) lets you quickly find a template by substring and select it by clicking the row.
  Duplicate from selected: Click "Duplicate From Selected" to open the wizard prefilled from the current skill. The new name gets a "\_Copy" suffix; max rank, passive flag, and timing fields are carried over. Enable "Copy Coeffs" to also clone coefficient parameters.
  Batch Create (Skills): Pattern expansion with preview and duplicate detection. Provide a Name Format (printf-style), Start, and Count, and optionally set Passive/Max Rank/timing fields. The preview lists generated names and flags duplicates. Apply creates the batch and reports a concise summary.
- Skills validation: Saving Overrides JSON now runs validation first and blocks the save on errors (a message explains what to fix). Creating a new skill will run validation and show a warning if the definition is invalid (creation still proceeds so you can iterate). A headless-safe API `rogue_skill_debug_validate(err, cap)` is available for tools/tests. New: a sticky "Validation Status" banner at the top of the Skills panel shows OK or "ERROR: <reason>" and refreshes live on edits (timing/coeffs/visuals) and on Create/Save/Load. Also new: inline validation messages inside the Effects tab for primary/node EffectSpec IDs and timing/HP gate fields; errors display next to the inputs as you type.
- Skills Visuals / Advanced: Edit optional fields live (sprites: cast/projectile/impact/aoe; animation: frame_count/frame_duration_ms/loops/grid; audio: cast/impact/loop + volume/pitch variance; AoE: shape/radius/angle; projectile: velocity/trajectory/pierce/homing). Changes apply via headless-safe setters and persist to overrides JSON. Validation checks asset existence and parameter bounds and surfaces issues on save/create.
- Optional: `skill_type` enum can be set on skills (MELEE, RANGED, AOE_SPELL, BUFF, DEBUFF, HEAL, SUMMON, PASSIVE, ULTIMATE). If omitted, it defaults to UNKNOWN for back-compat.
- New: Skills Meta section includes a Skill Type selector with human-readable labels and "Type Presets" buttons that apply safe baseline timing/coeff defaults per type. The property panels are context-sensitive (e.g., AoE-only fields for AOE_SPELL, projectile-only fields for RANGED). Simulation controls moved to the Testing tab.
- Real-time preview (Testing tab): Toggle "Enable Real-time Preview" to see the selected skill’s visuals rendered with sprite-sheet animation; use "Auto-animate" and "Zoom" to control playback and scale. The preview selects cast/projectile/impact/AoE sprites based on `skill_type`, caches textures, and is headless-safe (no SDL calls when no renderer is present).
  Skills Live Preview (M4.2, textual): Above the simulation controls, a headless-safe table summarizes the selected skill: name/type, Cast and Cooldown timing, Coeffs (base/per-rank), Primary Effect kind/magnitude and Effect Nodes (with delays/repeats), plus a rough estimated total tick count computed from repeats/periods. Complements the sprite preview; updates as you edit.
  Audio/VFX (M4.3): In the Audio/VFX panel, audition sounds and inspect attenuation deterministically.
  - Toggles: Enable Positional Audio, Listener follows player; adjust Falloff Radius (tiles).
  - Play Sound @ Cursor: Emits through the FX bus at the cursor’s world position using camera/tile mapping.
  - Attenuation Preview: Headless‑safe ASCII bar samples effective gain at key radius fractions (0/25/50/75/100%).
  - Optional Gizmo: Falloff ring visualization near listener/player (rendered only when an SDL renderer is available).
- Effects overrides import: The Skills overrides loader now applies the primary `effect_spec_id` and any `effect_nodes[]` atomically after parsing, regardless of JSON key order. This prevents partial application and matches the Effects tab editor’s export format.
- Effects tab – EffectSpec palette: Toggle a mini palette to browse all registered EffectSpecs with filters (substring, Kind, Debuff-only, and Category toggles: Offensive/Defensive/Movement/Utility). Select an entry and choose Assign-To (Primary or Node 0..2), then click Assign Selected to update the composition. Use "Clear Nodes" to reset node assignments quickly. Changes mark the composition dirty and persist via overrides. Powered by a new safe enumeration helper (`rogue_effect_count`).
- Effects tab – Node Graph: Toggle the Node Graph to arrange up to a few nodes visually; drag to move, click to select, and edit parameters (EffectSpec ID, delay/duration/repeats/interval, HP gate) in the side panel. New: explicit connections.
  - Linking workflow: select a source node, click "Start Link", then click a destination node to connect; use "Cancel Link" to abort. Cycles are prevented.
  - Unlink/Clear: "Unlink Selected" removes the parent link; "Clear Selected Node" resets its EffectSpec and timings.
  - Apply timing: "Apply Connections to Delays" derives each node's delay_ms by summing parent spans (duration or repeat_count\*repeat_interval_ms). "Chain Nodes" remains as a quick X‑order auto‑sequence.
  - Visuals: edges render between connected nodes; nodes tint by validation status (red = invalid id, orange = timing/param issues, green = OK). Rendering is headless‑safe.
- Effects tab – Authoring help: A small "Repeat Mode" helper (0 none / 1 count / 2 window) maps inputs to repeat_count, repeat_interval_ms, and duration_ms safely. Contextual suggestions appear when the selected EffectSpec defines timing: "Apply spec duration (ms)" and "Use spec pulse period (ms)" (optionally deriving repeat_count). Extra one‑click fixes address common interval/duration inconsistencies.
- Effects tab – Presets & Batch helpers: Quick Node Presets (Instant, Periodic Window, Counted Pulses), HP gate quick-set buttons (0/25/50%), and Batch Helpers (Fill empty nodes with Primary, Normalize window durations to whole pulses, Clear all HP gates) to speed up common authoring flows; UI-only and headless-safe.

Notes

- Panels are movable and their layout (x, y, width, visibility) persists across runs. The layout file is `build/overlay_layout.json`. To reset, delete that file. Tip: drag any panel by its title bar; height clamps to the viewport. Long tables auto-cap visible rows to avoid overflow.
- If a click doesn’t select, make sure the cursor is over the world (not another UI), and keep Shift held while clicking.

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

| Section                        | Purpose                                           |
| ------------------------------ | ------------------------------------------------- |
| 1. High‑Level Overview         | Elevator pitch & design pillars                   |
| 2. Feature Matrix              | Snapshot of implemented vs planned features       |
| 3. Systems Overview            | One‑paragraph summaries of each major subsystem   |
| 4. Build & Run                 | Configure, build, test commands                   |
| 5. Configuration & Assets      | Data formats & asset locations                    |
| 6. Testing & Quality Gates     | Determinism, CI gates, fuzz/stat suites           |
| 7. Development Workflow        | Everyday contributor loop                         |
| 12. Contributing               | Contribution standards                            |
| 13. License                    | MIT license reference                             |
| 14. Media / Screenshots        | Placeholder images / planned diagrams             |
| 15. Quick Reference Cheatsheet | Frequently used commands                          |
| Appendix A                     | Full original README phase logs (complete detail) |

---

## 1. Description

Layered, deterministic top‑down action roguelike engine emphasizing: modular boundaries, reproducible simulation, incremental phase roadmaps, strong test coverage (unit, integration, fuzz, statistical), and data‑driven extensibility (hot reload, schema docs, JSON/CSV/CFG ingest). Systems include loot & rarity, equipment layering (implicits → uniques → sets → runewords → gems → affixes → buffs), dungeon & overworld procedural generation, AI behavior trees + perception + LOD scheduling, skill graph & action economy, persistence with integrity hashing, UI virtualization & accessibility, and emerging economy/crafting pipelines.

### Core Design Pillars

- Determinism first (explicit seeds, reproducible golden snapshots)
- Progressive complexity (phased roadmaps with tests per slice)
- Data before code (config/schema/hot reload/tooling surfaces early)
- Integrity & telemetry (hashes, anomaly detectors, analytics export)
- Maintainability (module boundaries audit, minimal cross‑module coupling)

Note for Windows contributors: prefer ASCII punctuation in docs (e.g., '-' instead of '–') to avoid codepage‑dependent test failures when tests read fixed‑size buffers.

### Testing & Quality Gates (quick)

    - Build: use CMake multi‑config generators with parallelism (e.g., -j12)
    - Run tests: ctest -C Debug -j12 --timeout 10 --output-on-failure (use -R <regex> for targeted runs)

Notes:
Latest CI verification: Debug build (SDL2) and full suite with -j12 passed 100% (594/594). Worldgen optimization benchmark stabilized via adaptive repetition to avoid timer granularity flakiness in CI. Persistence tests use centralized save path builders; a recent fix updated `test_save_incremental_basic` to honor per-test directories via `rogue_build_slot_path(0)` for stability under parallel runs. On Windows/MSVC, the Content Graph SDL preview avoids VLA-like locals by using compile-time caps (OVERLAY_CG_MAX_NODES/EDGES); the System panel shows FPS via overlay_last_dt() when metrics aren’t initialized. Recent local run: all tests green (594/594) on Debug SDL2 with -j12. New validation path: `tests/unit/test_skills_validation_pipeline.c` confirms that an offensive-looking skill without coefficients fails validation until a coeff entry is added, after which validation passes. Phase 1.3 adds two units: `test_skills_phase1_3_interrupt` and `test_skills_phase1_3_context_defaults`.

MSVC compatibility note:

- Some overlay panels compile as C under MSVC; ensure declarations precede statements (C89/C90 rule). The Skills Effects Node Graph source was adjusted by moving a typedef and static state declarations to the top of the block to satisfy this requirement.

Phase 1.3 runtime slice:

- Execution state now includes INTERRUPTED. `rogue_skill_get_exec_state` reports INTERRUPTED when an activation was canceled via interrupt until the next activation clears it.
- Skill context (`RogueSkillCtx`) carries cast_pos/target_pos and an affected entity array; when unset, cast_pos defaults to the player’s position and target_pos defaults to cast_pos. Affected count is clamped to 8 for safety.
- Interrupt API `rogue_skill_interrupt(id, ctx)` cancels active cast/channel, sets `interrupted_active` and timestamp, triggers end FX (`skill/<id>/end`), and applies partial resource refunds honoring `refund_on_cancel_pct`. New activations clear the interrupted flag. - Re-verified after AI Utility Selector destructor wiring (advanced_nodes): Debug SDL2 full suite remains 100% green (582/582) under -j12. - Optional: enable AI blackboard write/get tracing during fuzz triage by defining ROGUE_TRACE_BB=1 at build time (writes bb_trace.txt in the test working dir). Default is off for quiet CI. - Test helper for speed: in unit tests that load skills content, call `rogue_skills_set_skip_icon_loads(1)` to bypass icon texture I/O. Used by `test_skills_roundtrip_schema` and `test_skills_base_autoreload` to cut runtime to milliseconds while preserving semantics.
  Formatting: - Run per-file clang-format locally: use the CMake targets `format` (auto-fix) and `format-check` (verify). These work on Windows without hitting command-line length limits. - CI installs clang-format and runs `cmake --build build --target format-check`; failures will show which files need formatting.
  Windows CI notes: - The workflow uses the Ninja generator under an MSVC dev environment to avoid intermittent MSBuild stalls when compiling many small unit test targets in parallel.

### Build flags and modules

APIs: `src/debug_overlay/overlay_core.h` plus widgets in `overlay_widgets.h` (Label, Button, Checkbox, SliderInt/Float, InputText, Combo, TreeNode/Pop, ColorEdit RGBA, Table). Input capture in `overlay_input.h`.

- ROGUE_ENABLE_JSON_CONTENT (default ON): Compiles the content JSON foundation (I/O and schema envelope). Built as an object library (`rogue_content_json`) and linked into `rogue_core` when enabled. A vendored cJSON stub lives under `third_party/cjson` and is linked as `rogue_thirdparty_cjson` for now; replace with the full cJSON later.
- Outputs: HTML, LaTeX/PDF, and XML generated via a dedicated CMake target.
- Prereqs (Windows):
  - Doxygen 1.9.8+ and Graphviz (dot) for diagrams
  - LaTeX distribution (MiKTeX/TeX Live) for PDF (optional)
- Build docs:

  - Generate your build first (CMake multi-config), then run the docs target.
  - In VS Code: run the “docs” task or invoke the CMake target.
  - Outputs land in `build/docs/html`, `build/docs/latex`, and `build/docs/xml`; the PDF (refman.pdf) is under `build/docs/latex` if LaTeX is installed.
  - Custom HTML theme is applied from `docs/templates/theme.css`.
  - CI: Every push/PR builds docs on Windows with SDL enabled and uploads the HTML as an artifact named `docs-html` and as a Pages artifact; GitHub Pages deploys automatically from CI.
    - Pages URL: enable GitHub Pages for this repo to serve the latest docs. See Actions → Deploy Pages job for the live link after a successful run.

  Contributor notes:

  - See docs/DOXYGEN_GUIDE.md for short templates and style rules when adding comments.
  - Prefer documenting headers (APIs) and hard-to-understand internal modules. Keep examples minimal and link related symbols via @ref/@see.

### Debug Overlay (early)

- Unified in-game debug overlay behind a compile-time flag.
- Feature flag: ROGUE_ENABLE_DEBUG_OVERLAY (default ON). When OFF, headers provide no-op stubs for zero overhead.
- Toggle with F1; the overlay renders after the HUD. Input is captured while active so gameplay doesn’t receive keys/mouse.
- APIs: `src/debug_overlay/overlay_core.h` plus widgets in `overlay_widgets.h` (Label, Button, Checkbox, SliderInt/Float, InputText). Input capture in `overlay_input.h`.
  - Layout: simple columns via `overlay_columns_begin/overlay_next_column/overlay_columns_end` (equal or custom widths). Widgets honor column width.
  - Layout now auto-wraps rows across columns; `overlay_next_column` advances within the row, wrapping to the next row after the last column. Row spacing uses the tallest widget in the row for clean grids.
  - Focus: Tab/Shift+Tab traversal across all interactive widgets; Enter/Space activate buttons/checkboxes; sliders respond to Left/Right. InputText supports caret navigation (Home/End/Left/Right), insertion/backspace at caret; clicking the field gives focus and captures input.
  - Split views and preferences: A resizable splitter (`overlay_splitter_begin/end`) enables two-pane layouts with persisted widths per panel via a small preferences store. Current adopters: Items (list + details), Skills (tabs/selection + content), and Map (controls + preview placeholder). Preferences persist to `build/overlay_prefs.json`; delete that file to reset.
- Headless-safe: widget drawing guards avoid SDL calls when no renderer is present (useful in unit tests).
- Tests: `test_overlay_core` and `test_overlay_widgets` (smoke), with the latter validating headless usage and basic interactions via simulated input.
  - New: `test_overlay_layout_focus` covers 2-column auto-wrap and focus traversal.
  - New: `test_overlay_table_widget` validates Table header sorting toggles and row selection using simulated input.
  - New: `test_overlay_inputtext_caret` exercises caret navigation (Home/End/Left/Right), insertion, and backspace sequencing under focus changes.
  - Player debug APIs covered in `test_player_debug_api`: clamps, derived stat recompute on stat changes, god-mode damage bypass, noclip flag roundtrip, and teleport.
  - Verification: Overlay tests pass headlessly in Debug (SDL2) with parallel ctest. Full suite currently all‑green in Debug with SDL2 and -j12.

Overlay panels:

- System panel shows FPS, frame time, draw calls, and tile quads, and includes a toggle for the metrics/overlay and overlay enable.
- Player panel exposes HP/MP/AP and core stats (STR/DEX/VIT/INT) with sliders, God Mode/No-clip toggles, and simple teleports (spawn/center). Player debug APIs are headless-safe and used by the panel.
- Skills panel (new) lists skills and lets you edit timing (cooldown/cast/channel) and coefficients (RogueSkillCoeffParams). Includes a quick 2s single-skill simulate action that dumps a JSON summary for inspection.

  - JSON overrides integration: Save and Load buttons persist skill overrides to/from `build/skills_overrides.json`.
  - Auto-load on startup: set `ROGUE_SKILL_OVERRIDES` to point at an overrides JSON file; when unset, the app attempts `build/skills_overrides.json`.
  - Implementation uses atomic write helpers from json_io; manual edits to the file can be loaded live via the panel's Load button.
  - New: Auto‑Reload Overrides toggle polls the overrides file mtime each frame and applies changes automatically without clicking Load.
  - New: Auto‑Reload Base Skills JSON toggle watches the base skills JSON and performs a full registry reload when the file changes; exposed as a separate checkbox in the Skills panel.
  - Validator coverage extended: added cross‑reference tests for proc.effect_spec_id and coeff requirements across offensive vs passive skills.
  - Internals: the Skills panel is now modularized into per‑tab units (overview/effects/visuals/audio/testing) with headers/guards and a small orchestrator; shared helpers centralize validation banners and overrides save/refresh. Headless‑safe preview paths are preserved for CI runs.

- Entities panel (new): quick inspector for runtime enemies.

  - Lists current enemies with id, alive flag, and position; select a row to target.
  - Actions: Kill, Teleport to Player, and Spawn at Player. Useful for triaging combat/AI.
  - Backed by headless-safe APIs in `src/core/entities/entity_debug.{h,c}` and covered by unit test `tests/unit/test_entity_debug_api.c`.
  - Content validation: a headless schema validator lives in `src/content/schema_entities.{h,c}` with a unit `tests/unit/test_entity_schema.c` that loads enemy type defs from assets and validates fields like group bounds, radii, speed, XP, and loot chance.

- Tilesets schema (new foundation for Map Editor):
  - Headless schema module `src/content/schema_tilesets.{h,c}` defines tilesets.json with fields: `id` (string), `tile_size` (int), `atlas` (string), and `tiles[]` array of objects `{ name, col, row }`.
  - Legacy adapter synthesizes JSON from `assets/tiles.cfg` so existing content validates without format migration.
  - Unit `tests/unit/test_tilesets_schema.c` validates the default assets/tiles.cfg via the schema; runs headlessly in the suite.

### Validation System

The runtime includes a state/content validation system to catch schema issues and cross-reference errors:

- Manager: `src/core/integration/state_validation_manager.{h,c}` registers per-system validators and cross-rule checks, schedules runs on an interval, logs events to a ring buffer, and exposes telemetry (runs, warnings, corruptions, time).
- Wiring: `src/core/integration/validation_wiring.{h,c}` registers default checks at startup: items, entities, tilesets, tag registry, and a cross-rule for skills.
- Overlay: A “Validation” panel lets you Run Now, Force All (bypass snapshot hash incremental skip), and adjust the interval. It shows stats and recent events.
- Headless/CI: A CLI tool `validate_content` runs these checks headlessly and returns a CI-friendly exit code.

Skills panel validation integration:

- Save Overrides JSON: runs `rogue_skill_debug_validate` first; blocks the save when invalid and surfaces the error text in-panel.
- Create flow: runs validation and shows a warning when invalid. This is warn-only to keep prototyping fast; you can tighten to hard-block later if desired.

CLI usage (optional):

```
validate_content --force-all --fail-on-warn
```

Exit codes: 0 OK, 1 warnings (only with --fail-on-warn), 2 corruption. Use `--quiet` to suppress per-event prints.

### Content Graph (Phase 13.3 – expanded)

Status: Complete. - `rogue_asset_dep_count()`, `rogue_asset_dep_get(index, &id, &path)`, `rogue_asset_dep_get_deps(id, out, max)`, and `rogue_asset_dep_hash(id, &out_hash)` (cached; invalidated on edits). - Filters: substring and advanced prefixes `id:`, `path:`, `group:`, `hash:`, `dep:X`, `rev:X`. - For the selected node: direct dependencies, reverse dependencies (who depends on me), and the node’s file hash (hex). - Group size summaries and path collision markers. - Actions: “Compute All Hashes”, “Export DOT” (writes `build/content_graph.dot`), and “Export JSON” (writes `build/content_graph.json`).
• DOT → `build/content_subgraph.dot`
• JSON → `build/content_subgraph.json` with shape `{ "root": string, "depth": int, "nodes": [{ "id", "hash" }], "edges": [{ "from", "to" }] }`. - Layered SDL preview for the selected node and its multi‑hop dependencies (BFS), with a max‑depth slider; toggleable on‑panel. - Visuals: nodes tinted by group; diagnostics line shows last dependency registration rejection (cycle or path conflict) when present; if the last rejection was a cycle and both endpoints are visible in the preview, the connecting edge is highlighted in red. - Interactions: Shift+LMB drag to pin/move node; use ‘Clear Pins’ to reset. - skills/base → assets/skills_uhf87f.json; skills → depends on skills/base - tiles (assets/tiles.cfg), sounds (assets/sounds.cfg), enemies/types (assets/enemies.json) - dialogue/style (assets/dialogue/style_default.json), dialogue/scripts (assets/dialogue/dialogues.json), dialogue (aggregates both) - ui/hud_layout (assets/hud_layout.cfg), ui/theme (assets/ui_theme_default.cfg) - player/anim (assets/player_anim.cfg), player/sheets (assets/player_sheets.cfg) - projectiles (assets/projectiles.cfg), tag_registry (assets/tag_registry.json) - world/biomes, world/trees, world/plants, world/resource_nodes, world/mining_nodes (assets/\*.cfg)

• Content Graph panel: filter by id/path/group/hash, export DOT/JSON, SDL preview with BFS, and quick finders for Orphans and Hubs (top 10 by out‑degree). - Core APIs in `src/core/world/map_debug.{h,c}` provide simple editing and JSON persistence: - `rogue_map_debug_set_tile(x, y, id)`, `rogue_map_debug_brush_square(x, y, radius, id)`, `rogue_map_debug_brush_rect(x, y, w, h, id)` - `rogue_map_debug_save_json(path, err, cap)` and `rogue_map_debug_load_json(path, err, cap)` with compact RLE tiles inside `{ "w": W, "h": H, "tiles": "..." }`. - Loader fixes prevent malformed literal detection and off‑by‑one pointer advance; validated by unit `tests/unit/test_map_debug.c`. - Map Editor panel (now usable): - Labeled tile selection using RogueTileType names; Erase toggle. - Brush modes: Square (radius) and Rect (x0,y0,x1,y1 inputs). - Utilities: Pick tile under player, Fill entire map with current tile, Clear map (EMPTY). - Persistence: editable JSON path with Save/Load buttons using compact RLE format via map_debug APIs. Edits invalidate the tile sprite LUT so visuals refresh immediately.

    - Items panel (new): manage and live-edit item definitions.
    	- Lists item defs in a sortable table with row selection.
    	- Edit core fields for the selected item (e.g., name, sockets, rarity) using safe setters.
    	- Persistence: specify a JSON path and use Save/Load to export/import item defs via `rogue_item_debug_save_json`/`_load_json` (atomic save, dynamic buffer sizing under the hood).
    	- Live vendor repricing: editing item defs updates vendor slot prices in-place via `rogue_vendor_on_item_def_changed`; bulk JSON loads trigger `rogue_vendor_reprice_all`.
    	- Creation Wizard: a guided form to add new base items (ID/Name/Category and core stats). Backed by `rogue_item_debug_create` which validates input, ensures unique IDs, sanitizes fields, and appends via `rogue_item_defs_add`.
    	- Duplicate-as-template: "Duplicate From Selected" opens the Create wizard prefilled from the selected row and suggests a unique id automatically.
    	- Virtualized list (prototype): large registries use a simple row-offset slider to render only visible rows (20 by default) for headless stability; mouse wheel/scrollbar integration planned.
    	- Headless-safe: backed by `src/core/loot/item_debug.{h,c}`; covered by unit `tests/unit/test_item_debug_api.c`.

Content schemas (foundation):

- Items schema (new): `src/content/schema_items.{h,c}` defines and validates items.json (id/name/category required; ranges for stack_max, rarity, sockets, etc.). Units `tests/unit/test_items_schema.c` and `tests/unit/test_items_roundtrip_schema.c` cover validation and a schema-backed export→envelope→reload roundtrip.
- Skills schema (new): `src/content/schema_skills.{h,c}` defines and validates skills.json with required fields and ranges, allowing extra fields for forward-compat. A new unit `tests/unit/test_skills_roundtrip_schema.c` exports a subset to JSON, wraps it in a versioned "skills" envelope, validates, atomically writes, and reloads via the loader to exercise the envelope path.

Items registry & migration:

- Stable handles: `RogueItemDefHandle` provides a generation-checked handle for item base defs. Helpers: `rogue_item_def_handle_from_index`, `rogue_item_def_index_from_handle`, `rogue_item_def_get_by_handle`. Generations bump on add/reset to invalidate stale references.
- Migration/export tool: `items_migrate` (built by CMake) exports the current registry to a versioned JSON envelope using `json_envelope` + `json_io`. Usage: `items_migrate <out.json>`. It attempts to load `assets/items` or falls back to `assets/test_items.cfg` if the registry is empty.
- Loader compatibility: `rogue_item_defs_load_from_json(path)` accepts both a raw JSON array of item defs and a versioned envelope with `$schema: "items"` (reads from `entries`). The schema-backed roundtrip test covers both paths.

Integration harness stabilization:

- `tests/integration/test_boot.c` now aligns its working directory with the repo root (like other integration tests) and uses the full `RogueAppConfig` initializer (logical dimensions and background color). This resolved a previous headless/SDL segfault in CI.

  Test save‑path isolation (stability under parallel ctest):

  - Centralized builders in `src/core/persistence/save_paths.{h,c}` construct slot/autosave/backup/json/quicksave paths and create directories as needed.
  - Tests run in isolated prefixes via `rogue_save_paths_set_prefix_tests()` which uses `ROGUE_TEST_SAVE_DIR` when set, else a per‑PID temp prefix. Persistence tests were updated to use `rogue_build_*` helpers instead of hardcoded filenames.
  - Result: previously flaky save/analytics tests are stable under `ctest -j12`.

Data I/O utilities (for upcoming content schemas):

- json_io: read whole file, atomic write (temp + replace), and file mtime in ms; all return detailed errors via char\* buffers.
- json_envelope: versioned envelope helpers for JSON content files: { "$schema": string, "version": u32, "entries": object|array }. Provides create/parse and frees.
- Tests: `test_json_io` and `test_json_envelope` validate round-trip, mtime, and parse error cases in Debug (SDL2) with parallel ctest.

### Data‑Driven Skill Coefficients (Phase 10.1)

- Centralized coefficients can be loaded from JSON/CSV via `skills_coeffs_load` into the runtime registry.
- The effective scalar per skill is: mastery × specialization × central_coeff(skill_id).
- Minimal APIs:
  - `int rogue_skill_coeffs_parse_json_text(const char* json)`
  - `int rogue_skill_coeffs_load_from_cfg(const char* path)`
  - `int rogue_skill_coeff_exists(int skill_id)`
- Test: run `ctest -C Debug -j8 -R test_skills_phase10_coeffs_loader` to validate parsing and scaling against the stat cache.

### External EffectSpec Config (Phase 10.2)

- Load EffectSpec definitions from a JSON array via `effect_spec_load` and register them in the runtime registry.
- API:
  - `int rogue_effects_load_from_json_text(const char* json, int* out_ids, int max_ids)`
  - `int rogue_effects_load_from_file(const char* path, int* out_ids, int max_ids, char* err, int errcap)`
- Supported fields mirror the legacy key=value parser:
  - kind, debuff, buff_type, magnitude, duration_ms, stack_rule, snapshot
  - scaling: scale_by_buff_type, scale_pct_per_point, snapshot_scale
  - preconditions: require_buff_type, require_buff_min
  - DOT/AURA: pulse_period_ms, damage_type, crit_mode, crit_chance_pct, aura_radius, aura_group_mask
  - SPAWN_PROJECTILE: proj_speed (float, units/sec), proj_life_ms (float, ms), proj_count (int, 1..n)
- Defaults & semantics:
  - Unset kind → STAT_BUFF; unset stack_rule → ADD; disabled preconditions/scaling use sentinel values.
  - Multiplicative stacking is a no‑op without a baseline; magnitude is percent (100 = no change).
  - For SPAWN_PROJECTILE: damage inherits from EffectSpec.magnitude; projectiles spawn from player and use facing to set initial direction. When proj_count > 1, multiple identical projectiles are emitted in the same frame.
  - New kinds: DAMAGE (direct damage), AOE_BLAST (instant area damage), TELEPORT (moves the player along facing by magnitude units; clamped to map bounds). All are available via the loader and runtime.
  - Target inference defaults: if target is omitted, HEAL and TELEPORT default to SELF; AURA and AOE_BLAST default to AREA.
- Test: `test_effectspec_json_loader` covers additive and multiplicative behavior; some tests disable buff dampening (`rogue_buffs_set_dampening(0.0)`) for deterministic rapid re‑applies.
  - Additional test: `tests/unit/test_effectspec_spawn_projectile.c` validates projectile count and damage mapping; full suite is green in Debug (SDL2) under `-j12` (588/588).

### Skills Validator (Phase 10.3)

- Central validation entry point: `int rogue_skills_validate_all(char* err, int err_cap)`.
- Checks performed:
  - Skill `effect_spec_id` references must exist when set.
  - Proc definitions must reference valid EffectSpecs; duplicate (event_type, effect_spec_id) pairs are flagged.
  - "Offensive" skills require a coefficient entry in the central table (`rogue_skill_coeff_exists`).
- Proc introspection helpers: `rogue_skills_proc_count()` and `rogue_skills_proc_get_def(int idx, RogueProcDef* out)` for tools/tests.
- Test: `test_skills_phase10_3_validator` initializes the event bus, registers procs/skills, asserts failures for bad refs/dupes/missing coeffs, then passes after fixes.
- Run focused tests: from `build` dir, `ctest -C Debug -j12 -R "test_skills_phase10_3_validator|test_effectspec_json_loader"`.

### Skills Auto‑doc (Phase 10.4)

- Curated documentation generator for skills inputs and related configs.
- API: `int rogue_skills_generate_api_doc(char* buf, int cap)` → writes a stable multi‑section text; returns bytes written, or -1 if `cap` is too small.
- Sections (in order): SKILL_SHEET_COLUMNS, SKILL_FLAGS_AND_TAGS, COST_MAPPING_EXTENSIONS, COEFFS_JSON_FIELDS, EFFECTSPEC_JSON_REFERENCE, VALIDATION_TOOLING.
- Test: `ctest -C Debug -j12 -R test_skills_phase10_4_api_doc` validates ordering and small‑buffer failure.

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
  - Note: The relative regression check only applies after baseline sampling completes; setting the threshold negative disables the relative check (useful for perf smoke tests). For the Start Screen perf smoke unit, the absolute check is disabled by setting `start_perf_budget_ms = 0.0` to avoid CI variance while still exercising the path.
- ROGUE_LOG_LEVEL=debug|info|warn|error: control console verbosity.
- ROGUE_SKILL_OVERRIDES: optional path to a JSON file with per-skill override values; used by the debug Skills panel and auto-loaded during app init.

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

Skills overrides quickstart:

- To capture your current tuning edits, open the Skills panel and click "Save Overrides JSON" (writes to `build/skills_overrides.json`).
- To apply external edits, modify the JSON on disk, then click "Load Overrides JSON" in the panel.
- To use a custom location, set the environment variable before launch:

  - PowerShell: `$env:ROGUE_SKILL_OVERRIDES = 'C:/path/to/overrides.json'`
  - cmd: `set ROGUE_SKILL_OVERRIDES=C:\path\to\overrides.json`
  - Clear in PowerShell: `Remove-Item Env:ROGUE_SKILL_OVERRIDES`

  Persistence robustness:

  - The save system tolerates empty/initial saves (no registered components) by computing CRC/SHA over an empty payload and still writing integrity footers. This enables the initial New Game save path to succeed in minimal test harnesses.
  - Save/Load debug spam has been routed through the central logger at DEBUG level; default WARN keeps the console quiet unless explicitly enabled.

## Audio & VFX (Phases 1–7 Snapshot)

Current capabilities:

- Unified FX bus with deterministic ordering & frame compaction (merged repeat counts) feeding audio (SDL_mixer-backed) and VFX spawning.
- Audio registry with lazy load, per-category + master mixer gains, positional attenuation stub, deterministic variant selection (id suffixed \_N), and music category isolation.
- Music system (Phase 6.1–6.6): logical state machine (explore/combat/boss) with per-state track registration, linear cross-fade (configurable ms), bar-aligned deferred transitions (`rogue_audio_music_set_tempo`, `rogue_audio_music_set_state_on_next_bar`), side-chain ducking envelope, procedural layering (deterministic sweetener selection), environmental reverb preset stubs (`rogue_audio_env_set_reverb_preset` smoothing wet mix), and distance-based low-pass attenuation (`rogue_audio_enable_distance_lowpass`, `rogue_audio_set_lowpass_params`).
- VFX registry & instance pool (lifetime scaling, layers BG→UI, world vs screen space, time scaling/freeze) plus particle emitter pool (variation distributions UNIFORM/NORMAL for scale & lifetime) with composite effects (CHAIN/PARALLEL) and per-instance overrides (scale, lifetime, color).
- Gameplay mapping layer (keys → audio/vfx with priority) wired to damage, skills, buffs, loot triggers.
- Config loader (CSV) with hot-reload watcher & validation error surfacing.
- Deterministic RNG seed override for reproducible particle/audio variant selection.

Recent additions (Phase 6–7):

- APIs: rogue_audio_music_register, rogue_audio_music_set_state, rogue_audio_music_set_state_on_next_bar, rogue_audio_music_set_tempo, rogue_audio_music_update, rogue_audio_duck_music, rogue_audio_music_current, rogue_audio_music_track_weight.
- Cross-fade weights and duck envelope integrated into rogue_audio_debug_effective_gain for testability; future SDL channel volume automation will hook into these scalars.
- New tests:
  - test_audio_vfx_phase6_1_4_music_system (cross-fade midpoint & completion + duck envelope)
  - test_audio_vfx_phase6_2_beat_aligned (verifies bar-aligned deferred transition & fade timing)

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

- Per-frame VFX stats snapshot and profiler API: `rogue_vfx_profiler_get_last(RogueVfxFrameStats*)` with counters for spawned*core/trail, culled*{pacing,soft,hard}, and active pools.
- Spawn control: `rogue_vfx_set_pacing_guard(enable, threshold_per_frame)` runs before `rogue_vfx_set_spawn_budgets(soft, hard)` each frame; culled counts are attributed to the stage that clamps.
- Pool audits: `rogue_vfx_particle_pool_audit` and `rogue_vfx_instance_pool_audit` expose active/free and simple run metrics for fragmentation checks.
- Stress: 100 simultaneous impacts test ensures pacing/soft/hard caps work under load without pool corruption.
  All Audio/VFX tests are green locally in Debug with SDL2 and `-j8`.

### Debug Overlay: Audio / VFX Panel (Phase 12)

- New overlay panel "Audio / VFX" enables rapid iteration on audio and visuals without leaving the game:
  - Play sounds by id/key and spawn VFX at a world position or at the cursor (screen→world conversion uses camera and tile scale).
  - Mixer controls: master and per‑category gains, mute toggle, and positional attenuation toggle.
  - Performance controls: global perf scale, pacing guard enable/threshold, and soft/hard spawn budgets.
  - Live stats readout via `RogueVfxFrameStats` (spawned core/trail, culled pacing/soft/hard, active pools).
- A thin headless‑safe API (`src/core/audio_vfx/audiovfx_debug.{h,c}`) decouples the overlay from core FX modules and is exercised by a unit test `tests/unit/test_audio_vfx_overlay_debug_panel.c`.

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

HEAL EffectSpec (Phase 1.2 slice):

- New effect kind ROGUE_EFFECT_HEAL restores player health up to max, clamped to prevent overheal.
- Classification: defaults to non-debuff (beneficial); supports negative magnitudes for damage if needed.
- Integration: applies via combat event stream for logging; supports periodic pulses and child chaining like other effects.
- Parser/Loader: recognizes "HEAL" in KV and JSON formats; runtime apply path handles health restoration deterministically.

Skill Execution State and Profiling (Phase 1.3 slice):

- Minimal execution state enum: RogueSkillExecState { IDLE, CASTING, CHANNELING, COOLDOWN } with inline getter `rogue_skill_get_exec_state`.
- Lightweight profiling: per-skill timestamps for last_act_start_ms, last_act_end_ms, last_cast_begin_ms, last_cast_end_ms; populated at runtime transitions for performance monitoring.
- Headless-safe: getters work without SDL; profiling aids in debugging execution timing without full state machine overhead.

### Skill Events on the Event Bus (Phase 7.1)

Two new skill events are emitted by the skills runtime and delivered via the central event bus:

- ROGUE_EVENT_SKILL_CHANNEL_TICK (0x0701): fired at deterministic channel tick boundaries. Payload: skill_id (u16), tick_index (1-based, u16), when_ms (double scheduled time).
- ROGUE_EVENT_SKILL_COMBO_SPEND (0x0702): fired when a combo spender consumes points (instant, cast-complete, or channel). Payload: skill_id (u16), amount (u8), when_ms (double).

Usage pattern (tests and gameplay):

- Subscribe with `rogue_event_subscribe(event_id, callback, user)`.
- Publish occurs inside skills runtime; to deliver callbacks in headless/unit tests, call `rogue_event_process_priority(now_ms)` periodically. The queue is deterministic and callbacks run in publish order by (when_ms, seq).
- See `tests/unit/test_skills_phase7_event_bus.c` for a minimal example that subscribes, advances skills over simulated time, pumps the bus, and asserts receipt.

### UI: Dual Cooldown + Cast/Channel Progress (Phase 11.1)

- The skill bar now shows both cooldown remaining (vertical dark overlay) and a thin bottom progress bar for an active cast/channel.
- Casts fill left→right over `cast_time_ms`; channels show elapsed fraction over the total channel duration.
- Colors: casts use a blue‑green bar; channels tint brighter cyan for readability. Rendering only; no gameplay logic changed.

### UI: Buff/Debuff Belt Enhancements (Phase 11.2)

- HUD buff belt groups active buffs by type, displays stack counts (xN), and shows a duration mini-bar.
- Border color reflects category/source (offensive, defensive, movement, utility; CC variants tinted red).
- Driven by `rogue_buffs_snapshot` and `rogue_buffs_type_categories`; pure UI change.

### Debug: Aura & Channel Area Overlay (Phase 11.3)

- Toggle F11 to display a world-space overlay for active auras and channel areas.
- Uses a radial falloff gradient (visual-only) projected with camera offsets and tile scale.
- Backed by minimal EffectSpec AURA introspection APIs; no gameplay side effects.

### Skill Coefficients (Phase 8)

- Centralized coefficient registry provides per-skill base scalar + per-rank increments and stat-based contributions for STR/DEX/INT (declared as percent-per-10 points).
- Soft caps are applied through the stat cache helper with a final hard clamp to enforce configured maxima under extreme stats; coefficients are consumed by `skill_get_effective_coefficient`.
- API: see `src/core/skills/skills_coeffs.h` and registration via `rogue_skill_coeff_register`.
- Test: `tests/unit/test_skills_phase8_coeffs.c` validates rank scaling and soft-cap behavior; green in Debug with SDL2 and parallel tests.

### Skills Persistence (Phase 9)

- Extended skill state is persisted in a versioned TLV save: rank, cooldown_end_ms, cast_progress_ms, channel_end_ms, next_charge_ready_ms, charges_cur, casting_active, channel_active.
- Active buffs/debuffs persist as compact tuples (type, magnitude, remaining_ms relative to now). Legacy absolute end-time layouts are auto-detected and converted.
- Versioning: section headers are version-tagged; counts use varints from v4+. Integrity helpers include a replay hash and a simple signature stub for tamper checks.
- Implementation: `write_skills_component`/`read_skills_component` and `write_buffs_component`/`read_buffs_component` in `src/core/persistence/save_manager.c`.
- Tests: `test_save_phase7_skill_buff_roundtrip`, `test_save_phase7_skill_buff_extensions`, `test_persistence_versions`, `test_save_v4_varint_counts`, `test_save_v8_replay_hash`, `test_save_v9_signature` — all green in Debug (SDL2) with parallel builds.

### Proc Engine (Phase 7.2)

A minimal Proc system is available for skills/effects integration:

- Register procs with `rogue_proc_register(const RogueProcDef*)`, specifying the triggering event type, optional predicate, EffectSpec id to apply, and internal cooldowns (global and per-target).
- Subscriptions are created lazily per event type; the engine fans events out to matching procs and enforces ICD before applying effects.
- Per-target ICD uses the event payload’s `target_entity_id` when present (e.g., DAMAGE_DEALT).
- See `src/core/skills/skills_procs.h` for the API and `tests/unit/test_skills_phase7_2_procs_icd.c` for usage and expected behavior.

Note: Event bus statistics now clamp ultra-fast measurements to a minimum of 1µs to avoid zero-valued metrics on very fast runs; this stabilizes stats-focused tests.

#### Probability & Smoothing (Phase 7.3)

- Proc defs include `chance_pct` (0..100). If omitted, defaults to 100 for back-compat.
- Deterministic RNG stream ensures reproducible results across runs; optional `use_smoothing` accumulates misses to bound variance so triggers converge under sustained attempts.
- Covered by `test_skills_phase7_3_probability`.

#### Loop Guard and Dynamic Scaling (Phases 7.4–7.5)

- Loop guard prevents proc storms and cycles using:
  - Depth cap (max = 8) on nested proc-triggered applications.
  - Cycle signature hash combining event type, effect/proc ids, and time bucket; repeated signatures within the active call tree are suppressed.
- Dynamic proc scaling reduces effective chance after recent repeated triggers:
  - Per-proc recent activity window; each extra trigger reduces chance by ~12% up to ~60%.
  - Deterministic: scaling is derived from the recorded recent slots and the dedicated RNG stream.
- Test notes: Some tests disable buff dampening (`rogue_buffs_set_dampening(0.0)`) to allow rapid re-application when validating scaling/loop guard behavior. See `tests/unit/test_skills_phase7_4_5_loop_and_scaling.c`.

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
