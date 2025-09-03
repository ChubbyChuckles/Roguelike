@page combat_guide Combat Guide

# Combat Guide for Players

## Overview

Combat in our roguelike is built around **timing, positioning, and mechanical precision**. Unlike many games that reward button-mashing, our system demands understanding of frames, windows, and spatial relationships. Master these mechanics and you'll dominate any encounter.

## 🎯 Combat Fundamentals

### The Attack State Machine

Every attack follows this precise sequence:

```
IDLE → WINDUP → STRIKE → RECOVER → IDLE
```

#### Phase Breakdown
- **Idle**: Ready to attack, can buffer inputs
- **Windup**: Preparation phase, cannot cancel
- **Strike**: Active damage window, precise timing required
- **Recover**: Vulnerable period, can be interrupted

### Timing Windows

#### Attack Timing
- **Windup**: 200-800ms (varies by weapon)
- **Strike**: 100-300ms (the "sweet spot")
- **Recovery**: 300-600ms (vulnerability period)

#### Defensive Windows
- **Perfect Guard**: First 140ms of guard input
- **Parry Window**: 160ms timing window
- **Dodge I-frames**: 400ms invulnerability

## ⚔️ Weapon Types & Strategies

### Melee Weapons

#### Swords (Balanced, Technical)
```
Characteristics:
├── Fast attack speed (300ms total)
├── Good range (2.5 tiles)
├── High crit potential
└── Excellent for combos

Strategy:
├── Use speed for poke damage
├── Perfect for hit-and-run tactics
├── Master parry windows
└── Chain light attacks for pressure
```

#### Axes (Heavy, Powerful)
```
Characteristics:
├── Slow but devastating (600ms total)
├── Short range (1.8 tiles)
├── High stagger potential
└── Armor penetration

Strategy:
├── Time heavy attacks for maximum damage
├── Use for crowd control
├── Position for optimal reach
└── Combine with movement for spacing
```

#### Spears (Reach, Control)
```
Characteristics:
├── Longest range (3.2 tiles)
├── Medium speed (450ms total)
├── Good poke potential
└── Area denial capability

Strategy:
├── Control space around you
├── Keep enemies at optimal distance
├── Use for kiting strategies
└── Excellent for single-target focus
```

### Ranged Weapons

#### Bows (Precision, Mobility)
```
Characteristics:
├── Fast projectile speed
├── High crit potential
├── Requires ammo management
└── Long range (8+ tiles)

Strategy:
├── Maintain distance from threats
├── Use terrain for cover
├── Time shots during enemy recovery
└── Position for clear lines of sight
```

#### Crossbows (Power, Slow)
```
Characteristics:
├── Slow reload (800ms)
├── High damage per shot
├── Piercing capability
└── Medium range (6 tiles)

Strategy:
├── Plan shots carefully
├── Use for high-value targets
├── Position for clear shots
└── Combine with movement for safety
```

### Magic Weapons

#### Wands (Versatile, Fast)
```
Characteristics:
├── Quick casting (250ms)
├── Various elemental effects
├── Resource management required
└── Medium range (4 tiles)

Strategy:
├── Chain spells for combo damage
├── Use elements strategically
├── Manage mana carefully
└── Position for optimal spell ranges
```

## 🛡️ Defensive Mechanics

### Guard System

#### Basic Guard
- **Activation**: Hold block button
- **Coverage**: 180° frontal arc
- **Damage Reduction**: 80% physical, 60% magical
- **Chip Damage**: 20% of blocked damage still applies

#### Perfect Guard
- **Timing**: First 140ms of guard input
- **Benefits**:
  - No damage taken
  - Stamina refund
  - Poise restoration
  - Riposte opportunity

### Parry System

#### Perfect Parry
- **Window**: 160ms after enemy attack telegraph
- **Requirements**: Face enemy, timing precision
- **Rewards**:
  - Complete damage negation
  - Enemy stagger
  - Riposte window (650ms)
  - 2.25x damage multiplier

#### Riposte Mechanics
- **Window**: 650ms after successful parry
- **Damage Multiplier**: 2.25x base damage
- **Guaranteed Crit**: Next attack cannot miss crit
- **Extended Window**: Grace period for follow-up

### Dodge System

#### Basic Dodge
- **I-frames**: 400ms invulnerability
- **Stamina Cost**: 18 points
- **Movement**: Fixed distance roll
- **Recovery**: Brief end-lag

#### Advanced Dodging
- **Directional Influence**: Dodge toward/away from threats
- **Terrain Interaction**: Use walls for positioning
- **Combo Potential**: Chain dodges with attacks
- **Risk/Reward**: High risk, high reward mechanic

## 🎯 Combat Timing & Precision

### Frame-Perfect Mechanics

#### Attack Timing
```
Light Attack Chain:
├── Attack 1: 300ms (windup 100ms + strike 100ms + recover 100ms)
├── Attack 2: 320ms (follow-up window 50ms)
├── Attack 3: 350ms (final attack, extended recovery)
└── Reset: 200ms (back to idle)
```

#### Cancel Windows
- **Early Cancel**: Hit confirm reduces recovery by 60%
- **Whiff Cancel**: 75% of normal recovery time
- **Perfect Cancel**: 90% reduction on perfect hits

### Enemy Attack Patterns

#### Telegraph Recognition
```
Visual Cues:
├── Weapon glow (charging)
├── Body positioning (wind-up)
├── Sound effects (attack calls)
└── Particle effects (spell casting)
```

#### Common Patterns
- **Slash**: Wide arc, dodge sideways
- **Thrust**: Linear, dodge back
- **Overhead**: Slow, perfect guard timing
- **Combo**: Multiple hits, time dodges between

## 🌍 Positioning & Spacing

### Optimal Ranges

#### Melee Positioning
```
Close Range (0-1.5 tiles):
├── High damage potential
├── Vulnerable to interrupts
└── Best for aggressive play

Medium Range (1.5-2.5 tiles):
├── Safe from most attacks
├── Good for poke damage
└── Control-focused play

Long Range (2.5+ tiles):
├── Very safe positioning
├── Limited damage output
└── Kiting strategies
```

#### Ranged Positioning
```
Optimal Range (4-6 tiles):
├── Clear line of sight
├── Outside melee threat
└── Good for sustained damage

Extreme Range (6+ tiles):
├── Very safe
├── Lower accuracy
└── Limited mobility options
```

### Terrain Utilization

#### Environmental Combat
- **Walls**: Use for cover and flanking
- **Elevation**: High ground advantages
- **Chokepoints**: Control narrow passages
- **Open Areas**: Mobility and kiting

#### Hazard Integration
- **Lava Pits**: Force enemy positioning
- **Spiked Traps**: Area denial tools
- **Moving Platforms**: Dynamic positioning
- **Destructible Objects**: Environmental weapons

## 🎪 Advanced Techniques

### Combo Systems

#### Light Attack Chains
```
Basic Chain:
├── Light 1 → Light 2 → Light 3
├── Damage scaling: 100% → 110% → 125%
└── Speed increase: 300ms → 280ms → 260ms

Advanced Chain:
├── Light 1 → Dodge → Light 2 → Parry → Riposte
├── Combines offense and defense
└── High skill ceiling
```

#### Heavy Attack Integration
```
Heavy Interrupts:
├── Light 1 → Heavy (interrupt enemy)
├── Heavy → Light Chain (follow-up damage)
└── Heavy → Dodge (reset positioning)
```

### Status Effect Management

#### Debuff Handling
- **Stun**: Use for guaranteed follow-ups
- **Slow**: Create distance for kiting
- **Poison**: Position to avoid spread
- **Burn**: Time attacks during damage ticks

#### Buff Utilization
- **Speed**: Chain attacks during buff
- **Damage**: Time heavy attacks during buffs
- **Defense**: Position safely during buffs
- **Regeneration**: Manage health during fights

## 👥 Enemy Behavior Patterns

### Enemy Archetypes

#### Aggressive Melee
```
Behavior:
├── Constant pressure
├── Short recovery windows
└── Vulnerable during attacks

Counter:
├── Use reach weapons
├── Perfect parry timing
└── Maintain distance
```

#### Defensive Tank
```
Behavior:
├── High damage reduction
├── Slow attack speed
└── Long recovery times

Counter:
├── Wait for openings
├── Use stagger mechanics
└── Position for flanks
```

#### Ranged Attacker
```
Behavior:
├── Keep distance
├── Projectile attacks
└── Vulnerable in melee

Counter:
├── Close distance quickly
├── Use terrain for cover
└── Time dodges for projectiles
```

#### Spell Caster
```
Behavior:
├── Telegraphed spells
├── Area denial
└── Resource management

Counter:
├── Interrupt casting
├── Use anti-magic abilities
└── Close distance during recovery
```

### Group Combat

#### Positioning
```
Triangle Formation:
├── Tank in front (aggro control)
├── DPS on sides (flanking damage)
└── Support in back (healing/utility)
```

#### Threat Management
- **Aggro Control**: Use taunts and positioning
- **Crowd Control**: Stun and slow for management
- **Prioritization**: Focus high-threat targets first
- **Resource Management**: Balance damage output with sustainability

## 📊 Combat Statistics

### Performance Metrics

#### Personal Stats
- **Hit Rate**: Percentage of successful attacks
- **Crit Rate**: Critical hit frequency
- **Dodge Success**: Successful evasion rate
- **Parry Rate**: Perfect parry frequency

#### Combat Efficiency
- **DPS**: Damage per second
- **DTPS**: Damage taken per second
- **Resource Usage**: Stamina/mana efficiency
- **Positioning Score**: Optimal range maintenance

### Optimization Goals

#### Beginner Targets
- **Hit Rate**: >70%
- **Crit Rate**: >15%
- **Dodge Rate**: >60%
- **Parry Rate**: >20%

#### Advanced Targets
- **Hit Rate**: >85%
- **Crit Rate**: >25%
- **Dodge Rate**: >75%
- **Parry Rate**: >40%

## 🎮 Practice Drills

### Timing Training
1. **Perfect Parry Drill**: Practice 160ms windows
2. **Dodge Timing**: Master 400ms i-frames
3. **Attack Chains**: Link light attacks smoothly
4. **Cancel Windows**: Learn early/late cancel timing

### Positioning Practice
1. **Range Control**: Maintain optimal weapon range
2. **Terrain Usage**: Learn environmental combat
3. **Flanking**: Position for side/rear attacks
4. **Kiting**: Control distance from threats

### Advanced Scenarios
1. **Boss Fights**: Learn telegraph recognition
2. **Group Combat**: Master crowd control
3. **Resource Management**: Balance damage with sustainability
4. **High-Pressure Situations**: Perform under stress

## 🔧 Equipment Considerations

### Weapon Selection
- **Match Playstyle**: Choose weapons that fit your preferences
- **Mastery Bonuses**: Gain bonuses for weapon specialization
- **Synergy Potential**: Consider equipment interactions
- **Upgrade Path**: Plan weapon progression

### Armor Optimization
- **Damage Type Resistance**: Match enemy attack types
- **Mobility Trade-offs**: Balance protection with speed
- **Special Effects**: Look for unique armor abilities
- **Upgrade Strategy**: Plan armor improvement path

## 🎯 Success Mindset

### Learning Progression
1. **Mechanical Mastery**: Learn timing and positioning
2. **Strategic Understanding**: Grasp enemy patterns
3. **Optimization**: Improve efficiency and damage output
4. **Creativity**: Develop unique combat approaches

### Common Mistakes
- **Button Mashing**: Learn precise timing
- **Poor Positioning**: Master range control
- **Resource Waste**: Manage stamina/mana carefully
- **Predictable Patterns**: Vary your approach

### Mental Game
- **Patience**: Wait for openings
- **Adaptation**: Adjust to enemy behavior
- **Risk Assessment**: Balance aggression with safety
- **Continuous Learning**: Always look for improvement

---

## 📚 Additional Resources

- **[Weapon Comparison Chart](weapon_comparison.md)** - Detailed weapon statistics
- **[Enemy Tactics Guide](enemy_tactics.md)** - Advanced enemy behavior analysis
- **[Combo Guides](combo_guides.md)** - Specific combo tutorials
- **[Timing Training](timing_training.md)** - Practice exercises

Combat mastery comes with practice and understanding. Study enemy patterns, perfect your timing, and develop spatial awareness. The reward is a deep, satisfying combat system that scales with your skill level.

**Want to improve your combat skills?** Join our [Discord community](https://discord.gg/roguelike) for practice sessions and advanced tutorials!
