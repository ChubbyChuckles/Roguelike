@page tutorials_index Tutorials Index

# Tutorials Index

This section provides custom tutorial pages for every subsystem implemented in the roguelike game engine. Each tutorial summarizes the key components, usage, integrations, and best practices based on the implementation roadmaps.

## AI & Behaviour System

Deterministic enemy behavior with behavior trees, perception, pathing, and group tactics. Supports seeded RNG for reproducibility and debug tracing.

- **Key Features**: Behavior trees, blackboard memory, LOS/vision cones, flow field pathing, combat reactions, group coordination.
- **Usage**: Initialize with seeded RNG, build trees from nodes, update with player state.
- **Integrations**: Combat for threat updates, Progression for difficulty scaling.
- **Best Practices**: Use seeded RNG, monitor tick budgets, leverage trace for debugging.

## Audio & VFX Pipeline

Unified event-driven effects system with pooling, layering, and deterministic replay. Supports positional audio and screen/world space VFX.

- **Key Features**: Event bus, audio registry/mixer, VFX emitters/particles, config authoring, hot reload.
- **Usage**: Define effects in configs, map events to IDs, emit with rogue_fx_emit.
- **Integrations**: Combat for damage cues, Skills for activation sounds.
- **Best Practices**: Deterministic seeds, performance scaling, asset validation.

## Character Vertical Progression System

Infinite leveling with attributes, maze skill graph, mastery, and perpetual scaling. Includes anti-grind mechanics and stat synergies.

- **Key Features**: XP curves, attribute allocation, maze unlocks, mastery ranks, passive rings, soft caps.
- **Usage**: Track XP/levels, allocate points, unlock nodes, apply effects.
- **Integrations**: Equipment for stat baselines, Combat for attribute scaling.
- **Best Practices**: Deterministic hashing, soft caps, performance monitoring.

## Combat System

Classless combat with attack state machines, damage pipeline, stamina/poise, defensive mechanics, and weapon integration.

- **Key Features**: Attack phases, damage types/mitigation, encumbrance, guard/parry/dodge, hit detection.
- **Usage**: Define attack defs, handle input, process damage events.
- **Integrations**: Skills for effects, Equipment for stats.
- **Best Practices**: Deterministic crits, stamina balance, soft caps.

## Crafting & Gathering System

Data-driven crafting with material registry, gathering nodes, refinement, recipes, and skill progression.

- **Key Features**: Material tiers, node spawning, refinement recipes, proficiency XP, enhancement.
- **Usage**: Salvage for materials, craft recipes, enhance items.
- **Integrations**: Inventory for stacking, Vendor for material sinks.
- **Best Practices**: Scarcity feedback, deterministic RNG.

## Debugging Overlay System

In-game debug UI for inspection, editing, and testing without restarts. Includes panels for all systems.

- **Key Features**: Overlay UI, JSON editing, validation, live reload, panels for entities/map/items/etc.
- **Usage**: Toggle with F1, navigate panels, edit values.
- **Integrations**: All systems for debugging APIs.
- **Best Practices**: Headless-safe, persisted settings.

## Dialogue System

Linear NPC dialogue with token expansion, effects, persistence, and localization.

- **Key Features**: Script parsing, token replacement, effect triggers, save/load resume.
- **Usage**: Define scripts, start conversations, advance lines.
- **Integrations**: Persistence for state, UI for display.
- **Best Practices**: Forward-compatible structs, deterministic effects.

## Documentation System

Doxygen-based comprehensive developer reference with diagrams, cross-references, and custom styling.

- **Key Features**: Doxyfile config, HTML/PDF output, graphviz diagrams, custom CSS.
- **Usage**: Run doxygen on codebase, view generated docs.
- **Integrations**: Code comments for API docs.
- **Best Practices**: Consistent comment templates, validation.

## Dungeon Generator

Layered procedural dungeon creation with rooms, corridors, keys/locks, traps, and thematic tagging.

- **Key Features**: BSP/Voronoi rooms, corridor carving, secret areas, key progression.
- **Usage**: Generate from seed, place in world.
- **Integrations**: World gen for entrances, Loot for rewards.
- **Best Practices**: Deterministic seeds, reachability checks.

## Enemy Difficulty System

Relative ΔL scaling with adaptive adjustments, telegraph systems, and balance analytics.

- **Key Features**: ΔL curves, adaptive difficulty, telegraph clarity, analytics.
- **Usage**: Apply ΔL to enemy stats, adjust based on player performance.
- **Integrations**: Combat for mitigation, Progression for power index.
- **Best Practices**: Conservative penalties, telemetry monitoring.

## Equipment System

Item stats, affixes, durability, sockets, infusions, and set bonuses.

- **Key Features**: Affix budget, durability degradation, socket gems, infusions.
- **Usage**: Equip items, apply affixes, repair durability.
- **Integrations**: Combat for stat bonuses, Loot for generation.
- **Best Practices**: Budget enforcement, soft caps.

## Hit System

Pixel-accurate hit detection using slash/weapon masks with broadphase culling.

- **Key Features**: Mask generation, broadphase AABB, narrowphase sampling.
- **Usage**: Generate masks offline, test hits in combat.
- **Integrations**: Combat for attack processing.
- **Best Practices**: Performance broadphase, deterministic transforms.

## Integration Plumbing

Cross-system communication with event buses, shared data, and configuration management.

- **Key Features**: Event bus, shared structs, hot-reload configs.
- **Usage**: Register systems, emit events, reload configs.
- **Integrations**: All systems for cohesion.
- **Best Practices**: Standardized APIs, backwards compatibility.

## Inventory System

Infinite stacking, material ledger, drag/drop, context menus, and price calculations.

- **Key Features**: Stacking, ledger, UI interactions, vendor integration.
- **Usage**: Add/remove items, stack materials, calculate values.
- **Integrations**: Crafting for materials, Vendor for prices.
- **Best Practices**: Efficient lookups, deterministic sorting.

## Item System

Item definitions, rarity, categories, stats, and JSON schema with validation.

- **Key Features**: JSON schemas, validation, registry handles.
- **Usage**: Load from JSON, validate, access via handles.
- **Integrations**: Loot for generation, Equipment for stats.
- **Best Practices**: Forward-compatible schemas, migration.

## Maintainability

Code quality tools, CI/CD, fuzzing, and performance monitoring.

- **Key Features**: Linting, testing, fuzzing, profiling.
- **Usage**: Run tools in CI, monitor metrics.
- **Integrations**: All code for quality.
- **Best Practices**: Automated gates, regression tests.

## Persistence & Migration

Save/load with integrity hashing, versioned headers, and migration for new fields.

- **Key Features**: Hash chains, TLV headers, migration handlers.
- **Usage**: Serialize state, load with migration.
- **Integrations**: All systems for state.
- **Best Practices**: Deterministic ordering, backwards compat.

## Skill Creation Suite

Advanced UI for skill authoring with visual editors, testing, and validation.

- **Key Features**: Node graphs, effect palettes, real-time preview, templates.
- **Usage**: Edit skills in overlay, test in sandbox.
- **Integrations**: Skills system for definitions.
- **Best Practices**: Headless-safe, persisted settings.

## Skill / Buff / Debuff System

Active skills, passives, procs, DOT/HOT, auras, cooldowns, and deterministic execution.

- **Key Features**: EffectSpec, stacking rules, periodic ticks, proc system.
- **Usage**: Define specs, activate skills, apply effects.
- **Integrations**: Combat for damage, Progression for scaling.
- **Best Practices**: Deterministic RNG, performance budgets.

## Start Screen

Polished start screen with save loading, settings, credits, and async prewarming.

- **Key Features**: Menu navigation, save discovery, settings overlay.
- **Usage**: Select continue/new/load, adjust settings.
- **Integrations**: Persistence for saves, UI for theming.
- **Best Practices**: Headless testing, deterministic hashes.

## UI / UX System

Modular UI with widgets, input abstraction, theming, animation, and virtualization.

- **Key Features**: Immediate-mode widgets, focus/navigation, themes, animations.
- **Usage**: Build UIs with widgets, handle input.
- **Integrations**: All systems for displays.
- **Best Practices**: Headless-safe, performance monitoring.

## Vendor System

Procedural shops with pricing, reputation, negotiation, buyback, and material sinks.

- **Key Features**: Inventory generation, dynamic pricing, reputation tiers.
- **Usage**: Visit vendors, buy/sell, negotiate prices.
- **Integrations**: Inventory for items, Crafting for materials.
- **Best Practices**: Scarcity feedback, deterministic RNG.

## World Boss System

Large-scale boss encounters with phases, mechanics, scaling, and rewards.

- **Key Features**: Multi-phase scripts, add waves, telegraphs, loot drops.
- **Usage**: Spawn bosses, fight phases, collect rewards.
- **Integrations**: Combat for mechanics, Loot for drops.
- **Best Practices**: Budget caps, deterministic seeds.

## World Generation & Procedural Content

Layered world gen with continents, biomes, caves, structures, and spawn ecology.

- **Key Features**: Macro layout, biome classification, local terrain, dungeons.
- **Usage**: Generate from seed, stream chunks.
- **Integrations**: AI for spawns, Loot for nodes.
- **Best Practices**: Deterministic seeds, performance budgets.

## Pixel-Driven Hit Detection Rework

Mask-based hit detection for accurate weapon/slash collisions.

- **Key Features**: Mask generation, broadphase, narrowphase.
- **Usage**: Generate masks, test hits.
- **Integrations**: Combat for attacks.
- **Best Practices**: Broadphase culling, deterministic.

For detailed implementation, refer to the respective roadmap files in `roadmaps/`.
