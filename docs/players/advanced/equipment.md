@page equipment_optimization_guide Equipment Optimization Guide

# Equipment Optimization Guide

## Overview

Equipment optimization is the art and science of maximizing your character's power through intelligent gear selection, enhancement, and maintenance. This guide covers the mathematical foundations, strategic decision-making, and practical techniques for achieving optimal equipment performance.

## 🎯 Understanding Affix Mathematics

### Affix System Fundamentals

#### Modifier Types
```
Flat Modifiers:
├── +X to Attribute (Strength, Dexterity, etc.)
├── +X to Max Health/Mana
├── +X% Critical Chance/Damage
└── +X Armor/Resist

Percentage Modifiers:
├── +X% Damage
├── +X% Attack Speed
├── +X% Movement Speed
└── +X% Resource Regeneration
```

#### Rarity Tiers and Modifier Count
| Rarity | Modifier Count | Quality Range | Base Value |
|--------|----------------|----------------|------------|
| **Common** | 1-2 | 80-100% | 100% |
| **Uncommon** | 2-3 | 85-105% | 125% |
| **Rare** | 3-4 | 90-115% | 150% |
| **Epic** | 4-5 | 100-130% | 200% |
| **Legendary** | 5-6 | 110-150% | 300% |

### Value Calculation

#### Effective Value Formula
```
Item Value = Base_Value × Quality_Multiplier × Σ(Modifier_Values)
```

#### Modifier Value Assessment
```
Attribute Modifiers:
├── Strength/Dexterity/Intelligence: 1.0 per point
├── Vitality: 0.8 per point (diminishing returns)
└── All Attributes: 0.7 per point (versatility penalty)

Combat Modifiers:
├── Critical Chance: 2.0 per 1%
├── Critical Damage: 1.5 per 1%
├── Attack Speed: 1.8 per 1%
└── Damage %: 1.2 per 1%
```

## 💎 Socket Gem Strategies

### Gem Types and Effects

#### Primary Gems
```
Red Gems (Offensive):
├── Ruby: +X Fire Damage
├── Garnet: +X% Attack Speed
├── Spinel: +X Critical Damage
└── Carnelian: +X% Physical Damage

Blue Gems (Defensive):
├── Sapphire: +X% Resist All
├── Aquamarine: +X Max Health
├── Lapis: +X% Dodge Chance
└── Turquoise: +X% Healing Received

Green Gems (Utility):
├── Emerald: +X% Resource Regeneration
├── Jade: +X Movement Speed
├── Peridot: +X% Experience Gain
└── Malachite: +X% Gold Find
```

#### Meta Gems
- **Requirements**: Specific gem combinations
- **Effects**: Powerful bonuses when conditions met
- **Rarity**: Epic/Legendary tier only

### Socket Optimization

#### Socket Count by Slot
```
Weapons: 3-6 sockets
Armor: 2-4 sockets
Accessories: 1-3 sockets
Shields: 2-3 sockets
```

#### Gem Selection Strategy
```
1. Primary Role Gems (60% of sockets)
   ├── Core damage/defense modifiers
   └── Essential utility bonuses

2. Synergy Gems (30% of sockets)
   ├── Build-specific combinations
   └── Meta gem enablers

3. Quality of Life Gems (10% of sockets)
   ├── Minor convenience bonuses
   └── Situational utilities
```

## 🔧 Durability Management

### Durability Mechanics

#### Degradation Rates
```
Combat Degradation:
├── Light Attacks: 0.5% per hit
├── Heavy Attacks: 1.0% per hit
├── Critical Hits: 1.5% per hit
└── Blocked Attacks: 0.2% per hit

Environmental Degradation:
├── Falling: 5-15% based on distance
├── Fire Damage: 2% per second
├── Acid Exposure: 3% per second
└── Time-based: 0.1% per minute
```

#### Repair Economics
```
Repair Cost = (Max_Durability - Current_Durability) × Base_Repair_Cost × Rarity_Multiplier

Repair Efficiency:
├── Full Repair: 100% durability, full cost
├── Partial Repair: 50% durability, 75% cost
└── Emergency Repair: 25% durability, 50% cost
```

### Maintenance Strategies

#### Proactive Maintenance
```
Repair Thresholds:
├── Weapons: Repair at 80% durability
├── Armor: Repair at 70% durability
├── Accessories: Repair at 60% durability
└── Emergency: Repair at 30% durability

Repair Scheduling:
├── Pre-boss: Full repair all gear
├── Mid-dungeon: Partial repair essentials
├── Post-combat: Repair heavily damaged items
└── Daily: Full maintenance cycle
```

#### Economic Optimization
```
Break-even Analysis:
├── Repair vs Replace: Compare costs
├── Durability Investment: Higher durability gear
└── Repair Kit Usage: Consumable efficiency
```

## 📊 Min-Maxing Techniques

### Statistical Optimization

#### Damage Per Second (DPS) Calculation
```
Weapon DPS = (Base_Damage × Attack_Speed) × (1 + Damage_Modifiers) × Crit_Multiplier

Crit_Multiplier = 1 + (Crit_Chance × Crit_Damage)

Effective DPS = Weapon_DPS × (1 + Skill_Damage_Bonuses) × (1 + Equipment_Damage_Bonuses)
```

#### Survivability Metrics
```
Effective Health = Max_Health × (1 + Damage_Reduction) × (1 + Healing_Modifiers)

Damage Reduction = Σ(Armor_Value ÷ (Armor_Value + Scaling_Constant)) + Flat_Reduction

Tankiness Score = Effective_Health × Damage_Reduction × Healing_Efficiency
```

### Build Optimization

#### Theorycrafting Process
```
1. Define Build Goals
   ├── Primary Role (DPS, Tank, Support)
   ├── Secondary Objectives
   └── Playstyle Preferences

2. Calculate Base Stats
   ├── Attribute Requirements
   ├── Gear Dependencies
   └── Skill Prerequisites

3. Optimize Modifiers
   ├── Primary Stats (40% priority)
   ├── Secondary Stats (35% priority)
   └── Tertiary Stats (25% priority)

4. Test and Iterate
   ├── Simulation Testing
   ├── Real-world Validation
   └── Performance Analysis
```

#### Diminishing Returns Analysis
```
Stat Efficiency = Marginal_Gain ÷ Marginal_Cost

Point of Diminishing Returns:
├── When efficiency drops below 50%
├── When alternative stats provide better value
└── When soft caps are approached
```

## 🏗️ Equipment Progression Strategy

### Early Game (Levels 1-20)
```
Focus: Basic Functionality
├── Prioritize: Basic damage and survivability
├── Budget: Common/Uncommon gear
├── Strategy: Upgrade frequently, don't over-invest
└── Goal: Establish fundamental capabilities
```

### Mid Game (Levels 21-40)
```
Focus: Specialization
├── Prioritize: Role-specific modifiers
├── Budget: Rare gear with good affixes
├── Strategy: Hold out for quality over quantity
└── Goal: Optimize for preferred playstyle
```

### Late Game (Levels 41+)
```
Focus: Perfection
├── Prioritize: Perfect modifier combinations
├── Budget: Epic/Legendary gear
├── Strategy: Meta gem completion and synergy maximization
└── Goal: Theoretical optimal performance
```

## 💰 Economic Considerations

### Item Valuation

#### Market Value Calculation
```
Item_Value = Base_Cost × Rarity_Multiplier × Affix_Quality_Sum × Socket_Value

Affix_Quality_Sum = Σ(Affix_Value × Affix_Quality)

Socket_Value = 1 + (Gem_Count × Average_Gem_Value)
```

#### Investment Strategy
```
Short-term: Sell low, buy high-value items
Long-term: Invest in upgradeable gear
Speculative: Rare items with potential
Conservative: Reliable, high-utility items
```

### Crafting vs Buying
```
Crafting Decision Matrix:
├── Material Cost vs Item Value
├── Time Investment vs Gold Value
├── Skill Requirements vs Available Time
└── Market Saturation vs Demand
```

## 🔄 Respec and Adaptation

### Equipment Respec Economics
```
Respec Cost = Item_Value × 0.3 + Base_Respec_Fee

Break-even Point = Respec_Cost ÷ Value_Improvement

Respec ROI = (New_Item_Value - Old_Item_Value - Respec_Cost) ÷ Respec_Cost
```

### When to Respec
- **Major Build Changes**: Different attribute priorities
- **Meta Shifts**: New optimal gear configurations
- **Quality Improvements**: Significantly better item drops
- **Role Changes**: Switching from DPS to Tank/Support

## 📈 Advanced Analytics

### Performance Tracking
```
Key Metrics:
├── Damage Output: DPS, DPM, peak damage
├── Survivability: DTPS, effective health, healing received
├── Efficiency: Resource usage, time to kill
└── Consistency: Performance variance, reliability
```

### Comparative Analysis
```
Gear Comparison:
├── Absolute Values: Raw stat differences
├── Percentage Improvements: Relative gains
├── Synergy Effects: Combined gear performance
└── Opportunity Cost: Alternative gear options
```

### Optimization Tools
- **Stat Weight Calculators**: Determine optimal stat priorities
- **Gear Comparison Tools**: Side-by-side equipment analysis
- **Simulation Software**: Theoretical performance modeling
- **Community Databases**: Shared optimization data

## 🎯 Practical Examples

### High-DPS Build Optimization
```
Primary Stats (60% priority):
├── Critical Chance: 25-30%
├── Critical Damage: 150-200%
├── Attack Speed: 20-30%
└── Strength/Dexterity: 50+ points

Secondary Stats (30% priority):
├── Damage %: 15-25%
├── Haste: 10-15%
└── Mastery: 20-30%

Tertiary Stats (10% priority):
├── Versatility: 5-10%
├── Leech: 3-5%
└── Avoidance: 10-15%
```

### Tank Build Optimization
```
Primary Stats (50% priority):
├── Armor: Maximum available
├── Health: 50,000+ effective
├── Damage Reduction: 30-40%
└── Vitality: 40+ points

Secondary Stats (35% priority):
├── Avoidance: 20-25%
├── Leech: 5-10%
└── Versatility: 15-20%

Tertiary Stats (15% priority):
├── Critical Avoidance: 10-15%
├── Speed: 10-15%
└── Utility Stats: As needed
```

## 🌟 Mastery Techniques

### Perfect Item Theory
```
Requirements for "Perfect" Item:
├── All desirable affixes present
├── No wasted modifier slots
├── Optimal affix quality distribution
├── Socket configuration matches build
└── Durability management plan in place
```

### Meta Gem Completion
```
Strategy for Meta Gems:
├── Plan socket layout around meta requirements
├── Acquire necessary gems early
├── Balance meta bonuses with individual gem value
└── Consider opportunity cost of meta-focused builds
```

### Long-term Investment
```
Gear Investment Strategy:
├── Early: Functional but upgradeable items
├── Mid: High-quality items with growth potential
├── Late: Perfect items with meta gem completion
└── Endgame: Theoretical optimal configurations
```

---

## 📚 Additional Resources

- **[Affix Database](affix_database.md)** - Complete modifier reference
- **[Gem Encyclopedia](gem_encyclopedia.md)** - Detailed gem information
- **[Build Calculator](build_calculator.md)** - Optimization tools
- **[Market Analysis](market_analysis.md)** - Economic data and trends

Equipment optimization is both an art and a science. Understanding the mathematics behind modifiers, making strategic decisions about gem sockets, and maintaining your gear properly will significantly enhance your character's performance.

**Need help optimizing your gear?** Join our [equipment optimization Discord](https://discord.gg/equipment-opt) or check our [build theory forum](https://forum.roguelike.com/equipment) for community advice and calculations.
