@page character_progression_tutorial Character Progression Tutorial

# Character Progression Tutorial

## Overview

The Character Vertical Progression System provides a deep, extensible framework for infinite XP & leveling, attributes, passive & active skill acquisition via a maze-based skill graph, mastery, universal skill learning, perpetual advancement scaling, synergy with equipment/skills/affixes, performance-friendly stat aggregation, deterministic & auditable growth, anti-grind safeguards, and rich analytics.

## Key Components

### Infinite XP & Leveling
- **XP Curve**: Multi-component (linear + quad + pow + log) for smooth growth
- **Catch-up Multiplier**: Tanh-based up to 1.75x for late joiners
- **Overflow Safety**: 64-bit accumulator with saturating add

### Attribute Allocation
- **Primary Attributes**: Strength, Dexterity, Vitality, Intelligence
- **Points per Level**: +3 allocation points
- **Re-spec Mechanism**: Journal hash for integrity

### Rating & Diminishing Returns
- **Rating Categories**: Crit, Haste, Avoidance
- **DR Curves**: Multi-band concave for balance
- **Aggregation Order**: Base → Rating → Multiplicative

### Maze Skill Graph
- **Graph Structure**: Nodes with geometric positions and edges
- **Unlock Gating**: Level and attribute requirements
- **Pathfinding**: Dijkstra for shortest cost paths

### Skill & Passive Integration
- **Effect DSL**: Parse STR+5 etc. for stat changes
- **Runtime Compilation**: Dispatch tables for effects
- **Unlock Journal**: Timestamped node unlocks with hash

### Mastery System
- **Mastery XP**: Earned via skill usage
- **Rank Thresholds**: Bracketed scalar bonuses (1.01→1.20)
- **Decay Mechanic**: Inactivity decay to prevent specialization

### Expanded Passive Rings
- **Milestone Levels**: Dynamic expansion for deeper progression
- **Keystone Nodes**: Powerful effects with cost & prerequisite chains
- **Anti-Stack Safeguards**: Category-based diminishing (offense/defense/utility)

### Perpetual Scaling
- **Micro-Node Progression**: Minor stat nodes always available
- **Diminishing Power**: Sublinear growth with global coefficient
- **Inflation Guard**: Telemetry-driven adjustments

### Synergy with Equipment & Skills
- **Unified Modifier Order**: Equipment → Passives → Mastery → Micro
- **Cap Enforcement**: Hard/soft caps for key stats
- **Conditional Logic**: Tag-based synergies (e.g., Fire tag scaling)

### Buff/Debuff Integration
- **Stat Engine Extension**: Timed buffs as layer after passives
- **Snapshot vs Dynamic**: Per-buff flag for recalculation
- **Stacking Rules**: Unique, Refresh, Extend, Add

### Performance & Caching
- **Incremental Recalc**: Dirty flags for selective updates
- **SoA Arrays**: Cache-friendly passive effect iteration
- **Micro-Bench**: Stat recompute under 200 effects

### Persistence & Migration
- **Versioned Headers**: V1/V2/V3 with migration paths
- **Talent Bitset**: Sparse list encoding for unlocks
- **Re-spec Journal**: Ops journal with replay

## Usage Example

To set up progression:

1. Initialize XP and level tracking.
2. Allocate attribute points on level up.
3. Unlock maze nodes based on requirements.
4. Apply passive effects to stat cache.
5. Handle re-spec with journal replay.

## Cross-System Integrations

- **Equipment**: Stat baselines before passive layering
- **Combat**: Attribute-derived damage and mitigation
- **Skills**: Mastery XP and cooldown modifiers
- **Loot**: Rarity weighting for progression rewards

## Best Practices

- Use deterministic hashing for integrity checks.
- Implement soft caps to prevent runaway power.
- Monitor performance of stat recalculations.
- Provide guided paths for new players.

## Further Reading

See `roadmaps/implementation_plan_character_progression_system.txt`.
