@page economy_guide Economy and Trading Guide

# Economy and Trading Guide

## Overview

The economy in our roguelike is a sophisticated system that rewards strategic thinking, resource management, and market awareness. Unlike simple gold sinks, our economy creates meaningful player agency through vendor manipulation, resource optimization, and long-term investment strategies.

## 💰 Core Economic Systems

### Currency Types

#### Primary Currency (Gold)
```
Sources:
├── Enemy drops (scaled by level and rarity)
├── Chest loot (fixed amounts)
├── Quest rewards (milestone-based)
├── Vendor sales (item value × markup)
└── Achievement bonuses (one-time rewards)

Uses:
├── Item purchases from vendors
├── Equipment repairs and maintenance
├── Crafting material acquisition
├── Service fees (storage, identification)
└── Luxury purchases (cosmetics, upgrades)
```

#### Secondary Currencies
```
Crafting Tokens:
├── Salvage from equipment
├── Quest rewards
├── Achievement unlocks
└── Seasonal events

Enhancement Shards:
├── Boss drops
├── Rare enemy encounters
├── Treasure room rewards
└── Crafting byproducts
```

### Economic Scaling

#### Inflation Mechanics
```
Gold Inflation = (Total_Gold_In_Economy ÷ Base_Gold_Economy) - 1

Inflation Effects:
├── Vendor prices increase by 10-25%
├── Material costs rise proportionally
├── Rare item values appreciate
└── Service fees scale upward
```

#### Deflation Safeguards
```
Gold Sinks:
├── Repair costs (durability-based)
├── Enhancement fees (rarity-scaled)
├── Storage fees (time-based)
├── Luxury item purchases
└── Gambling mechanics (optional)
```

## 🏪 Vendor System Deep Dive

### Vendor Types and Behaviors

#### General Merchants
```
Inventory Management:
├── 20-30 item slots
├── Restock every 10-15 minutes
├── Price fluctuation based on stock
├── Special offers on overstocked items

Pricing Strategy:
├── Base price = Item_Value × (0.8 - 1.2)
├── Stock multiplier = 1 + (Stock_Level ÷ Max_Stock)
├── Demand multiplier = 1 + (Sales_Velocity × 0.1)
└── Reputation modifier = 0.9 - 1.1
```

#### Specialty Vendors
```
Weapon Smith:
├── Focus: Weapons and weapon materials
├── Stock: 15-25 weapon varieties
├── Services: Repairs, modifications
└── Special: Weapon enhancement services

Armor Merchant:
├── Focus: Armor and defensive gear
├── Stock: 15-25 armor pieces
├── Services: Repairs, resizing
└── Special: Armor reinforcement

Alchemist:
├── Focus: Potions and reagents
├── Stock: 20-30 consumable items
├── Services: Potion crafting, identification
└── Special: Custom potion creation
```

### Advanced Vendor Manipulation

#### Price Manipulation Strategies
```
Stock Control:
├── Buy low-stock items to increase prices
├── Sell excess items to create demand
├── Time purchases for restock cycles
└── Coordinate with other players (multiplayer)

Market Timing:
├── Buy during high supply periods
├── Sell during high demand periods
├── Watch for economic events
└── Monitor global market trends
```

#### Vendor Relationship Management
```
Reputation System:
├── Purchase frequency increases favor
├── Large transactions build loyalty
├── Timely payments maintain trust
└── Special services unlock at high reputation

Benefits of High Reputation:
├── Better prices (5-15% discount)
├── Priority restocking of preferred items
├── Exclusive item access
└── Bulk purchase discounts
```

## 📊 Item Valuation Mathematics

### Base Value Calculation

#### Equipment Valuation
```
Item_Base_Value = (Rarity_Multiplier × Level_Scaling × Affix_Sum) + Socket_Value

Where:
├── Rarity_Multiplier = {Common: 1.0, Uncommon: 1.25, Rare: 1.5, Epic: 2.0, Legendary: 3.0}
├── Level_Scaling = 1 + (Item_Level × 0.1)
├── Affix_Sum = Σ(Affix_Value × Affix_Quality)
└── Socket_Value = Socket_Count × Average_Gem_Value
```

#### Material Valuation
```
Material_Value = (Base_Value × Quality_Multiplier × Scarcity_Multiplier) × Market_Condition

Quality Factors:
├── Purity: 0.8 - 1.2 (material quality)
├── Quantity: 0.9 - 1.1 (stack size bonus)
└── Source: 0.95 - 1.05 (rarity of source)
```

### Market Value Adjustments

#### Supply and Demand
```
Market_Price = Base_Value × Supply_Multiplier × Demand_Multiplier

Supply_Multiplier = 1 ÷ (1 + Supply_Abundance)
Demand_Multiplier = 1 + (Demand_Pressure × 0.2)

Where:
├── Supply_Abundance = Current_Stock ÷ Average_Stock
└── Demand_Pressure = Sales_Velocity ÷ Average_Velocity
```

#### Economic Events
```
Event Multipliers:
├── Scarcity Event: ×1.5 (rare materials)
├── Abundance Event: ×0.7 (common materials)
├── Festival Event: ×1.2 (luxury goods)
└── Crisis Event: ×2.0 (essential goods)
```

## 🎒 Inventory Optimization

### Space Management

#### Inventory Capacity
```
Base_Capacity = 50 slots
Expansion_Methods:
├── Backpack upgrades (+10-20 slots)
├── Efficiency enchantments (+5-15% effective capacity)
├── Organization systems (stacking bonuses)
└── External storage (paid services)
```

#### Item Stacking Strategy
```
Stacking Efficiency = (Total_Items ÷ Used_Slots) × 100%

Optimization Techniques:
├── Material consolidation (merge small stacks)
├── Consumable grouping (potions, ammo)
├── Equipment sorting (weapons vs armor)
└── Quick-access prioritization
```

### Inventory Valuation

#### Portfolio Analysis
```
Inventory_Value = Σ(Item_Value × Quantity)
Portfolio_Diversity = Unique_Item_Types ÷ Total_Item_Types
Portfolio_Liquidity = Cash_Equivalent ÷ Total_Value

Health Metrics:
├── High liquidity = Easy to convert to cash
├── High diversity = Flexible trading options
├── Balanced value = Sustainable economy
```

#### Asset Allocation
```
Optimal Distribution:
├── 40% Trading goods (high turnover)
├── 30% Equipment (personal use)
├── 20% Materials (crafting/investment)
├── 10% Consumables (immediate use)
└── Cash reserves for opportunities
```

## 🏭 Production and Crafting Economics

### Crafting Profitability

#### Cost-Benefit Analysis
```
Crafting_Profit = (Product_Value - Material_Cost - Labor_Cost) × Success_Rate

Break-even Analysis:
├── Minimum_Sales_Price = Total_Cost ÷ (1 - Tax_Rate)
├── Optimal_Production_Run = Fixed_Cost ÷ Variable_Profit
└── Return_on_Investment = (Profit × Production_Rate) ÷ Initial_Investment
```

#### Production Scaling
```
Economies of Scale:
├── Small batch: 80% efficiency (setup overhead)
├── Medium batch: 95% efficiency (optimal balance)
├── Large batch: 90% efficiency (market saturation risk)

Batch Size Optimization:
├── Material availability constraints
├── Market absorption capacity
├── Storage and transportation costs
└── Time value of money
```

### Specialization Strategies

#### Production Focus
```
Weapon Smithing:
├── High demand for combat gear
├── Premium pricing for quality work
├── Customization opportunities
└── Repair service revenue

Alchemy Specialization:
├── Consumable production (potions, bombs)
├── High turnover potential
├── Research and development
└── Rare ingredient trading
```

## 📈 Investment and Speculation

### Long-term Investment

#### Equipment Investment
```
Investment Strategy:
├── Buy low-level gear with growth potential
├── Upgrade path planning (material requirements)
├── Market timing for rare components
└── Speculation on meta shifts

ROI Calculation:
├── Upgrade_Cost = Σ(Component_Value × Quantity)
├── Value_Increase = Post_Upgrade_Value - Pre_Upgrade_Value
└── ROI_Percentage = (Value_Increase ÷ Upgrade_Cost) × 100%
```

#### Material Speculation
```
Market Analysis:
├── Supply trend monitoring
├── Demand forecasting
├── Seasonal pattern recognition
└── Economic indicator tracking

Speculation Tactics:
├── Buy low during abundance events
├── Hold through scarcity periods
├── Sell high during peak demand
└── Diversify across material types
```

### Risk Management

#### Portfolio Diversification
```
Asset Allocation Strategy:
├── 50% Safe investments (common materials)
├── 30% Moderate risk (rare materials)
├── 15% High risk (speculative items)
└── 5% Cash reserves (liquidity buffer)

Risk Assessment:
├── Volatility measurement
├── Correlation analysis
├── Worst-case scenario planning
└── Stop-loss mechanisms
```

## 🌐 Market Dynamics

### Global Economic Factors

#### Player Population Impact
```
Population Effects:
├── High population = Increased competition
├── Low population = Market opportunities
├── Population growth = Inflation pressure
└── Population decline = Deflation benefits

Market Efficiency:
├── High population = Efficient pricing
├── Low population = Arbitrage opportunities
└── Optimal population = Balanced economy
```

#### Economic Cycles
```
Boom Cycle:
├── High demand, rising prices
├── Investment opportunities
├── Speculative bubbles
└── Risk of correction

Bust Cycle:
├── Low demand, falling prices
├── Buying opportunities
├── Distress sales
└── Recovery planning
```

### Inter-Market Relationships

#### Cross-Market Arbitrage
```
Price Differentials:
├── Regional price variations
├── Vendor type discrepancies
├── Time-based fluctuations
└── Event-driven anomalies

Arbitrage Strategy:
├── Identify price inefficiencies
├── Calculate transportation costs
├── Account for time value
└── Execute risk-free profits
```

## 🎯 Advanced Strategies

### Market Making
```
Market Maker Role:
├── Provide liquidity in thin markets
├── Profit from bid-ask spreads
├── Stabilize price volatility
└── Earn reputation bonuses

Techniques:
├── Buy low, sell high cycles
├── Inventory management optimization
├── Price stabilization algorithms
└── Volume-based profit strategies
```

### Economic Warfare
```
Competitive Strategies:
├── Price undercutting (temporary)
├── Stock manipulation
├── Information asymmetry exploitation
├── Strategic alliances formation

Ethical Considerations:
├── Market manipulation detection
├── Community impact assessment
├── Long-term relationship building
└── Sustainable business practices
```

## 📊 Performance Analytics

### Economic Metrics

#### Personal Performance
```
Key Indicators:
├── Net Worth Growth Rate
├── Portfolio Turnover Ratio
├── Win Rate on Investments
└── Economic Efficiency Score

Benchmarking:
├── Compare against market averages
├── Track improvement over time
├── Identify strengths and weaknesses
└── Set realistic performance goals
```

#### Market Analysis Tools
```
Technical Indicators:
├── Price momentum analysis
├── Volume trend monitoring
├── Market sentiment tracking
└── Economic cycle prediction

Advanced Analytics:
├── Regression analysis for pricing
├── Correlation studies
├── Predictive modeling
└── Risk assessment algorithms
```

## 🎮 Practical Examples

### Early Game Economy (Levels 1-20)
```
Strategy Focus:
├── Resource gathering efficiency
├── Basic crafting profitability
├── Equipment upgrade planning
└── Foundation building

Key Tactics:
├── Farm high-value common materials
├── Learn profitable crafting recipes
├── Save for important equipment upgrades
└── Build vendor relationships
```

### Mid Game Economy (Levels 21-40)
```
Strategy Focus:
├── Market manipulation
├── Specialization development
├── Investment opportunities
└── Economic optimization

Key Tactics:
├── Identify profitable markets
├── Develop crafting specializations
├── Time major equipment purchases
└── Build economic buffer
```

### Late Game Economy (Levels 41+)
```
Strategy Focus:
├── Portfolio management
├── Speculative investments
├── Market influence
└── Legacy building

Key Tactics:
├── Diversified investment portfolio
├── High-risk, high-reward speculation
├── Market maker activities
└── Economic empire building
```

## 🔮 Future Economic Developments

### Advanced Features
```
Planned Enhancements:
├── Player-owned shops
├── Auction house system
├── Economic contracts
├── Investment vehicles
└── International trade
```

### Economic Balancing
```
Dynamic Adjustments:
├── Inflation control mechanisms
├── Scarcity event management
├── Player feedback integration
└── Economic health monitoring
```

---

## 📚 Additional Resources

- **[Market Data Dashboard](market_dashboard.md)** - Real-time economic indicators
- **[Crafting Calculator](crafting_calculator.md)** - Profitability analysis tools
- **[Investment Guide](investment_guide.md)** - Advanced portfolio strategies
- **[Economic Theory](economic_theory.md)** - Mathematical foundations

Mastering the economy requires understanding both micro-level transactions and macro-level market dynamics. Strategic thinking about resources, timing, and relationships will significantly enhance your roguelike experience.

**Want to discuss economic strategies?** Join our [trading Discord](https://discord.gg/trading) or participate in our [market analysis forums](https://forum.roguelike.com/economy)!

*Economic systems evolve with player behavior. Stay informed about market trends and adapt your strategies accordingly.*
