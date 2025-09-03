@page combat_system_tutorial Combat System Tutorial

# Combat System Tutorial

## Overview

The Combat System implements a layered, deterministic combat framework with strictly classless design. Every character can learn any skill, wield any weapon, and equip any armor weight. Balance is achieved through soft caps, encumbrance, stamina & poise tradeoffs, and adaptive diminishing returns instead of class restrictions.

## Key Components

### Attack State Machine
- **Phases**: Idle → Windup → Strike → Recover
- **Combo System**: Speed acceleration and damage scaling
- **Input Buffering**: Queued attacks during non-idle phases

### Damage Pipeline
- **Types**: Physical, Bleed, Fire, Frost, Arcane, Poison, True
- **Mitigation**: Armor, Resistances, Penetration
- **Critical Hits**: Pre/Post mitigation options with multiplicative stacking

### Stamina, Poise, Encumbrance
- **Resource Pools**: Separate stamina, guard, poise
- **Encumbrance**: Weight-based movement and regen penalties
- **Hyper Armor**: Poise ignore frames for certain attacks

### Defensive Mechanics
- **Guard**: Directional blocking with chip damage
- **Perfect Guard**: Parry timing window with posture break
- **Dodge Roll**: I-frame granting with stamina cost
- **Parry/Riposte**: Special damage windows post-parry

### Weapon & Equipment Integration
- **Archetypes**: Light, Heavy, Thrust, Ranged, Spell Focus
- **Infusions**: Elemental damage splits and buildup
- **Familiarity**: Usage-based bonuses with soft caps
- **Durability**: Damage reduction over time

### Advanced Features
- **Charged Attacks**: Damage scaling with nonlinear curves
- **Aerial Attacks**: Bonus damage with landing lag
- **Backstab/Crit Detection**: Positional bonuses
- **Projectile Deflection**: Parry-based reflection

### Hit Detection & Spatial
- **Hitboxes**: Capsule, Arc, Swept, Multi-segment
- **Broadphase**: AABB culling for performance
- **Lock-on**: Optional assist with magnetize logic
- **Terrain Interaction**: Obstruction damping

## Usage Example

To set up combat for a player:

1. Initialize attack definitions with timing and damage.
2. Configure weapon stats and infusion.
3. Set up stamina and poise pools.
4. Handle input for attacks, guards, dodges.
5. Process damage events and apply mitigation.

## Cross-System Integrations

- **Skills**: Effect application and cooldown gating
- **Equipment**: Stat bonuses and proc triggers
- **Progression**: Attribute scaling for damage and mitigation
- **AI**: Enemy behavior based on combat state

## Best Practices

- Use deterministic RNG for crit rolls in testing.
- Balance stamina costs to prevent infinite combos.
- Implement soft caps to curb stacking power.
- Test with various weapon types for classless feel.

## Further Reading

Refer to `roadmaps/implementation_plan_combatsystem.txt` for detailed phases.
