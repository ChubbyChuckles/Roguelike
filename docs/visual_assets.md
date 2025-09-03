@page visual_assets_page Visual Assets Guide

# Visual Assets for Documentation

This page documents the visual assets needed for the professional documentation presentation, including specifications, usage guidelines, and placeholder representations.

## 📋 Asset Inventory

### Logo and Branding
- **Main Logo**: Professional logo for header and branding
- **Banner Images**: Hero banners for different documentation sections
- **Favicon**: Browser tab icon
- **Social Media Assets**: Square logo variants for Discord/GitHub

### Screenshots and Gameplay
- **Gameplay Showcase**: Main gameplay with UI overlay
- **Combat Demonstration**: Action shots of different combat styles
- **World Exploration**: Dungeon and overworld screenshots
- **Character Progression**: Skill maze and character screens
- **Debug Overlay**: Real-time editing interface

### System Diagrams
- **Architecture Overview**: High-level system relationships
- **Combat Flow**: Attack state machine and damage pipeline
- **AI Behavior Tree**: Decision-making flow visualization
- **World Generation**: Procedural content creation pipeline

### Feature Illustrations
- **UI Mockups**: Interface design concepts
- **Flow Charts**: Player progression and system interactions
- **Icon Set**: Consistent icons for different systems
- **Infographics**: Statistics and performance metrics

## 🎨 Logo Specifications

### Primary Logo
```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│                ██████╗  ██████╗  ██████╗ ██╗   ██╗███████╗  │
│                ██╔══██╗██╔═══██╗██╔════╝ ██║   ██║██╔════╝  │
│                ██████╔╝██║   ██║██║  ███╗██║   ██║█████╗    │
│                ██╔══██╗██║   ██║██║   ██║██║   ██║██╔══╝    │
│                ██║  ██║╚██████╔╝╚██████╔╝╚██████╔╝███████╗  │
│                ╚═╝  ╚═╝ ╚═════╝  ╚═════╝  ╚═════╝ ╚══════╝  │
│                                                             │
│             Advanced Roguelike Game Engine                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Compact Logo
```
██████╗ ██████╗  ██████╗ ██╗   ██╗███████╗
██╔══██╗██╔═══██╗██╔════╝ ██║   ██║██╔════╝
██████╔╝██║   ██║██║  ███╗██║   ██║█████╗
██╔══██╗██║   ██║██║   ██║██║   ██║██╔══╝
██║  ██║╚██████╔╝╚██████╔╝╚██████╔╝███████╗
╚═╝  ╚═╝ ╚═════╝  ╚═════╝  ╚═════╝ ╚══════╝
```

## 📸 Screenshots Gallery

### Gameplay Showcase
```
┌─────────────────────────────────────────────────────────────┐
│ ╔═══════════════════════════════════════════════════════════╗ │
│ ║                    GAMEPLAY SCREENSHOT                    ║ │
│ ║                                                           ║ │
│ ║  [Player Character]         [Enemy]                      ║ │
│ ║       ⚔️                     🛡️                           ║ │
│ ║                                                           ║ │
│ ║  [Health Bar] [██████████████░░░░░░░░] 85/100            ║ │
│ ║  [Mana Bar]  [████████████████████] 100/100              ║ │
│ ║                                                           ║ │
│ ║  [Inventory Grid] [Skill Bar] [Mini Map]                 ║ │
│ ║                                                           ║ │
│ ║  [Combat Log]                                            ║ │
│ ║  Attack hits for 25 damage!                              ║ │
│ ║  Enemy is staggered!                                     ║ │
│ ║                                                           ║ │
│ ╚═══════════════════════════════════════════════════════════╝ │
│                                                             │
│          Main gameplay showing combat, UI, and world       │
└─────────────────────────────────────────────────────────────┘
```

### Debug Overlay
```
┌─────────────────────────────────────────────────────────────┐
│ ╔═══════════════════════════════════════════════════════════╗ │
│ ║                 DEBUG OVERLAY PANELS                      ║ │
│ ║                                                           ║ │
│ ║  ┌─ System ─┬─ Skills ─┬─ Items ─┬─ Map ─┬─ Audio/VFX ─┐  ║ │
│ ║  │ FPS: 60  │ [Skill]  │ [Item]  │ [Tile]│ [Sound]     │  ║ │
│ ║  │ Mem: 150M│ Effects  │ Stats   │ Brush │ Volume      │  ║ │
│ ║  │ Draw: 2.3K│Cooldown │ Rarity  │ Size  │ Effects     │  ║ │
│ ║  └─────────┴─────────┴─────────┴─────────┴─────────────┘  ║ │
│ ║                                                           ║ │
│ ║  [Real-time Asset Editing Interface]                      ║ │
│ ║                                                           ║ │
│ ╚═══════════════════════════════════════════════════════════╝ │
│                                                             │
│        Debug overlay showing real-time content editing      │
└─────────────────────────────────────────────────────────────┘
```

## 📊 System Architecture Diagrams

### High-Level Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                    ENGINE ARCHITECTURE                      │
│                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│  │   PLAYER    │    │   WORLD     │    │   COMBAT    │     │
│  │  • Stats    │    │  • Terrain  │    │  • Damage   │     │
│  │  • Skills   │    │  • Entities │    │  • Timing   │     │
│  │  • Inventory│    │  • AI       │    │  • Effects  │     │
│  └─────────────┘    └─────────────┘    └─────────────┘     │
│         │                   │                   │           │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│  │ PROGRESSION │◄──►│  SYSTEMS   │◄──►│   AUDIO     │     │
│  │  • Levels   │    │  • Input   │    │  • Sound    │     │
│  │  • XP       │    │  • UI      │    │  • Music    │     │
│  │  • Skills   │    │  • Save    │    │  • Effects  │     │
│  └─────────────┘    └─────────────┘    └─────────────┘     │
│                                                             │
│  [Modular C Architecture with Clean System Separation]     │
└─────────────────────────────────────────────────────────────┘
```

### Combat State Machine
```
┌─────────────────────────────────────────────────────────────┐
│                  COMBAT STATE MACHINE                        │
│                                                             │
│          ┌─────────────┐                                    │
│          │   IDLE      │                                    │
│          └──────┬──────┘                                    │
│                 │                                           │
│                 ▼                                           │
│          ┌─────────────┐                                    │
│          │  WINDUP     │◄─── Attack Input                   │
│          └──────┬──────┘                                    │
│                 │                                           │
│                 ▼                                           │
│          ┌─────────────┐                                    │
│          │   STRIKE    │◄─── Frame-perfect timing           │
│          └──────┬──────┘                                    │
│                 │                                           │
│                 ▼                                           │
│          ┌─────────────┐                                    │
│          │  RECOVER    │◄─── Hit confirm or timeout         │
│          └──────┬──────┘                                    │
│                 │                                           │
│                 └─────── Cancel windows ──────►            │
│                                                             │
│  [Frame-perfect combat with defensive options]              │
└─────────────────────────────────────────────────────────────┘
```

### AI Behavior Tree
```
┌─────────────────────────────────────────────────────────────┐
│                    AI BEHAVIOR TREE                          │
│                                                             │
│  ┌─────────────┐                                            │
│  │  SELECTOR   │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│    ┌────▼────┐                                              │
│    │ SEQUENCE │                                             │
│    └────┬────┘                                              │
│         │                                                   │
│  ┌──────▼──────┐                                            │
│  │  CONDITION  │                                            │
│  │ Player Visible│                                           │
│  └──────┬──────┘                                            │
│         │                                                   │
│  ┌──────▼──────┐                                            │
│  │   ACTION    │                                            │
│  │  Move To    │                                            │
│  │   Player    │                                            │
│  └─────────────┘                                            │
│                                                             │
│  [Modular decision-making with reusable nodes]              │
└─────────────────────────────────────────────────────────────┘
```

## 🎮 Gameplay Flow Charts

### Player Progression Flow
```
┌─────────────────────────────────────────────────────────────┐
│               PLAYER PROGRESSION FLOW                       │
│                                                             │
│  ┌─────────────┐                                            │
│  │   START     │                                            │
│  │  Character  │                                            │
│  │  Creation   │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │   COMBAT    │◄─── Kill enemies for XP                    │
│  │  & Combat   │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │   LEVEL UP  │◄─── Gain levels                            │
│  │  Attributes │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │ SKILL MAZE  │◄─── Unlock skill nodes                     │
│  │  Navigation │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │  EQUIPMENT  │◄─── Find/craft gear                        │
│  │  & Crafting │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │   MASTERY   │◄─── Use skills to improve                  │
│  │  & Scaling  │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         └─────── Infinite progression ──────►               │
│                                                             │
│  [Endless character growth with strategic depth]            │
└─────────────────────────────────────────────────────────────┘
```

### World Generation Pipeline
```
┌─────────────────────────────────────────────────────────────┐
│             WORLD GENERATION PIPELINE                       │
│                                                             │
│  ┌─────────────┐                                            │
│  │    SEED     │                                            │
│  │ Generation  │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │ CONTINENTS │◄─── FBM noise + land ratio                  │
│  │  & Oceans  │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │   BIOMES    │◄─── Climate simulation                     │
│  │  & Climate  │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │   TERRAIN   │◄─── Height maps + erosion                  │
│  │  & Caves    │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │ STRUCTURES  │◄─── POI placement + constraints            │
│  │  & Dungeons │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │   SPAWN     │◄─── Ecology simulation                     │
│  │  & Resources│                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐                                            │
│  │   UNIQUE    │◄─── Deterministic world                    │
│  │    WORLD    │                                            │
│  └─────────────┘                                            │
│                                                             │
│  [Multi-scale procedural content generation]               │
└─────────────────────────────────────────────────────────────┘
```

## 🎨 Icon Set

### System Icons
- ⚔️ Combat System
- 🧠 AI & Behavior
- 🌍 World Generation
- 📈 Character Progression
- 🔊 Audio & VFX
- 🛠️ Debug Tools
- 📚 Documentation
- 🎮 Player Controls

### Status Icons
- ✅ Completed
- 🔄 In Progress
- ❌ Not Started
- ⭐ Featured
- 🔗 Related
- 📖 Tutorial

## 📈 Performance Charts

### Frame Time Distribution
```
┌─────────────────────────────────────────────────────────────┐
│              FRAME TIME DISTRIBUTION                         │
│                                                             │
│  Performance: Target <16.7ms (60 FPS)                       │
│                                                             │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │                                                         │ │
│  │  ████████████████████████████████████████░░░             │ │
│  │  10ms                    15ms                 20ms       │ │
│  │                                                         │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                             │
│  Average: 12.3ms | 95th Percentile: 14.1ms | Min: 8.9ms     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Memory Usage Breakdown
```
┌─────────────────────────────────────────────────────────────┐
│               MEMORY USAGE BREAKDOWN                         │
│                                                             │
│  Total: 180MB                                               │
│                                                             │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  ████████ Assets (45MB)                                 │ │
│  │  ████████ World Data (40MB)                             │ │
│  │  ██████   AI Systems (30MB)                             │ │
│  │  ████     UI/Textures (25MB)                            │ │
│  │  ████     Audio (20MB)                                  │ │
│  │  ██       Debug Tools (10MB)                            │ │
│  │  ██       Other (10MB)                                  │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                             │
│  [Optimized memory usage with sophisticated pooling]       │
└─────────────────────────────────────────────────────────────┘
```

## 🖼️ Usage Guidelines

### Image Specifications
- **Format**: PNG for screenshots, SVG for diagrams
- **Resolution**: 1920x1080 minimum for screenshots
- **Color Space**: sRGB for web compatibility
- **File Size**: <500KB per image for web performance

### Diagram Standards
- **Colors**: Consistent palette across all diagrams
- **Typography**: Clear, readable fonts (Arial/Sans-serif)
- **Layout**: Left-to-right flow, logical grouping
- **Labels**: Descriptive but concise

### Accessibility
- **Alt Text**: Descriptive text for all images
- **Color Contrast**: Minimum 4.5:1 ratio
- **Text Alternatives**: Text descriptions for complex diagrams
- **Scalability**: Vector formats where possible

## 📁 Asset Organization

### Directory Structure
```
docs/
├── assets/
│   ├── images/
│   │   ├── screenshots/
│   │   ├── diagrams/
│   │   └── icons/
│   ├── videos/
│   └── prototypes/
├── templates/
└── styles/
```

### Naming Convention
- `screenshot_gameplay_main.png`
- `diagram_architecture_overview.svg`
- `icon_system_combat.png`
- `flow_player_progression.svg`

## 🔄 Maintenance

### Update Schedule
- **Screenshots**: Update with each major feature release
- **Diagrams**: Review and update when systems change
- **Performance Charts**: Update monthly with benchmark results
- **Logo**: Version control with git tags

### Quality Assurance
- **Image Optimization**: Compress without quality loss
- **Cross-browser Testing**: Verify display in different browsers
- **Mobile Responsiveness**: Test on various screen sizes
- **Loading Performance**: Monitor page load times

---

*This visual assets guide provides specifications and examples for creating professional documentation. The ASCII art representations serve as placeholders until actual images are created.*

**For implementation details, see the [About the Project](about.md) page or return to the [Main Index](index.md).**
