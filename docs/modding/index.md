@page modding_guide Modding Guide

# Modding Guide

Welcome to the comprehensive modding guide for our roguelike engine. This guide provides everything you need to create, customize, and extend the game through modding, from simple configuration changes to complex content creation.

## 🎮 Getting Started with Modding

### Modding Prerequisites

#### System Requirements
```
Minimum Requirements:
├── Game Version: Latest stable release
├── Operating System: Windows 10+, Linux, macOS
├── Disk Space: 500MB free space for mods
├── Text Editor: Any code editor (VS Code recommended)
├── Image Editor: GIMP, Photoshop, or similar
├── Audio Editor: Audacity or similar
└── Basic Programming Knowledge: JSON, basic scripting concepts
```

#### Installation and Setup
```
1. Install the base game
2. Create mod directory structure
3. Enable developer mode (if available)
4. Install modding tools (optional)
5. Test with sample mod
```

### Mod Directory Structure
```
Your Mod Folder/
├── mod.json              # Mod metadata and configuration
├── assets/               # Custom assets (images, sounds, etc.)
│   ├── textures/         # Sprite sheets and textures
│   ├── sounds/           # Audio files
│   ├── music/            # Background music
│   └── fonts/            # Custom fonts
├── data/                 # Game data files
│   ├── items.json        # Custom items
│   ├── enemies.json      # Custom enemies
│   ├── skills.json       # Custom skills
│   ├── biomes.json       # Custom biomes
│   └── dialogues.json    # Custom dialogue
├── scripts/              # Custom scripts (if supported)
└── patches/              # Modification patches
```

## 📁 Configuration File Formats

### Mod Metadata (mod.json)

#### Basic Mod Structure
```json
{
  "$schema": "mod",
  "name": "My Awesome Mod",
  "version": "1.0.0",
  "author": "Your Name",
  "description": "A brief description of what this mod does",
  "website": "https://your-mod-website.com",
  "dependencies": [],
  "incompatibilities": [],
  "tags": ["content", "balance", "visual"],
  "target_game_version": "1.0.0",
  "load_priority": 100
}
```

#### Advanced Mod Configuration
```json
{
  "$schema": "mod",
  "name": "Complex Mod Example",
  "version": "2.1.3",
  "author": "Mod Creator",
  "description": "Comprehensive gameplay overhaul",
  "dependencies": [
    {
      "name": "Base Content Pack",
      "version": ">=1.0.0"
    }
  ],
  "incompatibilities": [
    "Conflicting Balance Mod"
  ],
  "tags": ["overhaul", "content", "balance"],
  "target_game_version": "1.0.0",
  "load_priority": 50,
  "hooks": {
    "on_game_start": "scripts/init.lua",
    "on_level_load": "scripts/level_init.lua",
    "on_player_death": "scripts/death_handler.lua"
  },
  "assets": {
    "preload": true,
    "compression": "lz4",
    "fallback_path": "assets/fallback/"
  }
}
```

### Item Configuration (items.json)

#### Basic Item Definition
```json
{
  "$schema": "items",
  "version": 1,
  "items": [
    {
      "id": "my_custom_sword",
      "name": "Mythril Sword",
      "category": "weapon",
      "rarity": "rare",
      "icon": "items/mythril_sword.png",
      "stack_max": 1,
      "value": 1500,
      "stats": {
        "damage": 45,
        "attack_speed": 1.2,
        "durability": 200
      },
      "requirements": {
        "level": 15,
        "strength": 20
      },
      "description": "A finely crafted sword made of mythril"
    }
  ]
}
```

#### Advanced Item with Affixes
```json
{
  "id": "legendary_crimson_blade",
  "name": "Crimson Blade of Flames",
  "category": "weapon",
  "rarity": "legendary",
  "icon": "items/crimson_blade.png",
  "value": 50000,
  "stats": {
    "damage": 120,
    "fire_damage": 80,
    "attack_speed": 1.5,
    "critical_chance": 25,
    "durability": 500
  },
  "affixes": [
    {
      "type": "prefix",
      "name": "Crimson",
      "effects": [
        {
          "type": "stat_bonus",
          "stat": "fire_damage",
          "value": 50
        }
      ]
    },
    {
      "type": "suffix",
      "name": "of Flames",
      "effects": [
        {
          "type": "proc",
          "trigger": "on_hit",
          "effect": "burn_damage",
          "chance": 30,
          "value": 25
        }
      ]
    }
  ],
  "requirements": {
    "level": 50,
    "strength": 75,
    "intelligence": 50
  },
  "flavor_text": "Forged in the fires of Mount Doom, this blade hungers for dragon blood."
}
```

### Enemy Configuration (enemies.json)

#### Basic Enemy Definition
```json
{
  "$schema": "enemies",
  "version": 1,
  "enemies": [
    {
      "id": "goblin_warrior",
      "name": "Goblin Warrior",
      "description": "A fierce goblin fighter",
      "sprite": "enemies/goblin_warrior.png",
      "stats": {
        "health": 80,
        "damage": 15,
        "defense": 5,
        "speed": 1.2
      },
      "ai": {
        "type": "melee",
        "aggression": 0.7,
        "patrol_radius": 10
      },
      "loot": {
        "gold": [5, 15],
        "items": [
          {
            "item": "rusty_sword",
            "chance": 25
          }
        ]
      },
      "abilities": ["slash", "block"]
    }
  ]
}
```

#### Complex Enemy with Behaviors
```json
{
  "id": "ancient_dragon",
  "name": "Ancient Red Dragon",
  "description": "A massive, fire-breathing dragon of immense power",
  "sprite": "enemies/ancient_dragon.png",
  "stats": {
    "health": 5000,
    "damage": 150,
    "fire_damage": 200,
    "defense": 50,
    "speed": 0.8,
    "size": 3.0
  },
  "ai": {
    "type": "boss",
    "phases": [
      {
        "name": "Ground Phase",
        "health_threshold": 75,
        "abilities": ["fire_breath", "tail_sweep", "bite"],
        "movement": "grounded"
      },
      {
        "name": "Flight Phase",
        "health_threshold": 50,
        "abilities": ["dive_bomb", "fire_storm", "wing_buffet"],
        "movement": "flying"
      },
      {
        "name": "Rage Phase",
        "health_threshold": 25,
        "abilities": ["nova_blast", "meteor_shower", "dragon_rage"],
        "movement": "erratic"
      }
    ],
    "aggro_range": 20,
    "deaggro_range": 50
  },
  "loot": {
    "gold": [1000, 5000],
    "items": [
      {
        "item": "dragon_scale_armor",
        "chance": 100
      },
      {
        "item": "dragon_heart",
        "chance": 50
      }
    ]
  },
  "dialogue": {
    "on_combat_start": "dialogue/dragon_combat_start",
    "on_phase_change": "dialogue/dragon_phase_change",
    "on_death": "dialogue/dragon_death"
  }
}
```

## 🎨 Asset Creation Guidelines

### Texture and Sprite Creation

#### Sprite Sheet Standards
```
Technical Requirements:
├── Format: PNG with transparency
├── Color Depth: 32-bit RGBA
├── Maximum Size: 2048×2048 pixels
├── Minimum Size: 16×16 pixels
├── Power-of-2 dimensions recommended
└── Square aspect ratio preferred

Naming Convention:
├── Characters: {name}_{action}_{frame}.png
├── Items: item_{name}.png
├── UI: ui_{element}_{state}.png
├── Effects: fx_{name}_{frame}.png
└── Tiles: tile_{biome}_{type}_{variant}.png
```

#### Example Sprite Sheet Layout
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SPRITE SHEET LAYOUT                                │
│                                                                             │
│ Character: Hero                                                            │
│ Sheet Size: 512×256 pixels                                                 │
│ Frame Size: 64×64 pixels                                                   │
│ Frames per Row: 8                                                          │
│                                                                             │
│ Row 1: Idle animation (frames 0-3)                                         │
│ [🧑][🧑][🧑][🧑][  ][  ][  ][  ]                                           │
│                                                                             │
│ Row 2: Walk cycle (frames 4-11)                                            │
│ [🚶][🚶][🚶][🚶][🚶][🚶][🚶][🚶]                                           │
│                                                                             │
│ Row 3: Attack animation (frames 12-15)                                      │
│ [⚔️][⚔️][⚔️][⚔️][  ][  ][  ][  ]                                           │
│                                                                             │
│ Row 4: Hurt/Death animation (frames 16-19)                                 │
│ [😵][💀][  ][  ][  ][  ][  ][  ]                                           │
│                                                                             │
│ Metadata: hero_sprite.json                                                  │
│ {                                                                           │
│   "name": "hero",                                                           │
│   "frame_width": 64,                                                        │
│   "frame_height": 64,                                                        │
│   "animations": {                                                           │
│     "idle": {"start": 0, "count": 4, "speed": 0.2},                        │
│     "walk": {"start": 4, "count": 8, "speed": 0.15},                       │
│     "attack": {"start": 12, "count": 4, "speed": 0.1},                     │
│     "hurt": {"start": 16, "count": 2, "speed": 0.3}                        │
│   }                                                                         │
│ }                                                                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Audio Asset Guidelines

#### Sound Effect Standards
```
Technical Specifications:
├── Format: WAV or OGG
├── Sample Rate: 44.1kHz or 48kHz
├── Bit Depth: 16-bit or 24-bit
├── Channels: Mono or Stereo
├── Maximum Length: 10 seconds
└── Normalized Peak: -6dB to 0dB

Organization:
├── UI Sounds: ui_*.wav
├── Combat: combat_*.wav
├── Environment: env_*.wav
├── Character: char_*.wav
└── Music: music_*.ogg
```

#### Audio Metadata
```json
{
  "name": "sword_swing",
  "file": "sounds/combat/sword_swing.wav",
  "category": "combat",
  "volume": 0.8,
  "pitch_variation": 0.1,
  "loop": false,
  "fade_in": 0.0,
  "fade_out": 0.0,
  "tags": ["weapon", "melee", "impact"]
}
```

## 🔧 Scripting Capabilities

### Script Integration Points

#### Event-Driven Scripting
```lua
-- Example: Custom enemy behavior
function on_enemy_spawn(enemy)
    -- Custom spawn logic
    enemy.health = enemy.health * 1.2
    enemy:add_buff("spawn_protection", 5.0)
end

function on_enemy_update(enemy, delta_time)
    -- Custom AI logic
    if enemy.health < enemy.max_health * 0.3 then
        enemy:enter_rage_mode()
    end
end

function on_enemy_death(enemy, killer)
    -- Custom death effects
    spawn_particles("death_explosion", enemy.position)
    drop_loot(enemy, killer)
end
```

#### Item Script Hooks
```lua
-- Custom item behavior
function on_item_equip(item, player)
    if item.id == "ring_of_power" then
        player:add_stat_modifier("strength", 10)
        player:add_stat_modifier("intelligence", 5)
    end
end

function on_item_unequip(item, player)
    if item.id == "ring_of_power" then
        player:remove_stat_modifier("strength", 10)
        player:remove_stat_modifier("intelligence", 5)
    end
end

function on_item_use(item, player, target)
    if item.id == "health_potion" then
        player:heal(50)
        item:consume()
    end
end
```

### Advanced Scripting Features

#### Coroutine-Based Behaviors
```lua
-- Complex enemy behavior using coroutines
function dragon_flight_pattern(dragon)
    while dragon.health > 0 do
        -- Take off
        dragon:play_animation("takeoff")
        dragon:set_movement_mode("flying")
        coroutine.yield(2.0) -- Wait 2 seconds

        -- Fly in circles
        for i = 1, 3 do
            dragon:move_in_circle(10.0, 5.0)
            coroutine.yield(3.0)
        end

        -- Dive attack
        dragon:play_animation("dive")
        dragon:dive_attack()
        coroutine.yield(1.5)

        -- Land and recover
        dragon:set_movement_mode("grounded")
        dragon:play_animation("land")
        coroutine.yield(3.0)
    end
end
```

## 🔄 Hot-Reload System

### Live Asset Reloading

#### Supported File Types
```
Automatically Reloaded:
├── JSON configuration files
├── Texture and sprite assets
├── Audio files and sound banks
├── Font files
├── Shader programs
└── Localization files

Manual Reload Required:
├── Core engine binaries
├── Large structural changes
├── Database schema changes
└── Network protocol changes
```

#### Hot-Reload Workflow
```
1. Modify asset file
2. Save changes
3. Game detects file change
4. Validates new asset
5. Applies changes in-game
6. Logs reload status
7. Continues gameplay
```

### Development Tools

#### Debug Console Commands
```
Asset Management:
├── reload_assets - Force reload all assets
├── reload_textures - Reload texture cache
├── reload_sounds - Reload audio cache
├── validate_assets - Check asset integrity
└── list_assets - Show loaded assets

Mod Management:
├── load_mod <name> - Load specific mod
├── unload_mod <name> - Unload specific mod
├── list_mods - Show active mods
├── mod_info <name> - Show mod details
└── check_dependencies - Validate mod dependencies
```

## 📦 Creating and Distributing Mods

### Mod Packaging

#### Directory Structure for Distribution
```
MyMod_v1.0.0/
├── mod.json
├── README.md
├── CHANGELOG.md
├── LICENSE
├── assets/
│   ├── textures/
│   ├── sounds/
│   └── fonts/
├── data/
│   ├── items.json
│   ├── enemies.json
│   └── biomes.json
├── scripts/
│   └── custom_logic.lua
└── docs/
    ├── installation.md
    └── changelog.md
```

#### Mod Metadata for Distribution
```json
{
  "name": "Epic Fantasy Overhaul",
  "version": "1.0.0",
  "author": "Fantasy Mod Team",
  "description": "Complete fantasy-themed content overhaul",
  "tags": ["content", "overhaul", "fantasy"],
  "website": "https://epicfantasy.com",
  "repository": "https://github.com/fantasymod/epic-fantasy",
  "license": "MIT",
  "target_game_version": "1.0.0",
  "min_game_version": "1.0.0",
  "max_game_version": "1.9.9",
  "dependencies": [],
  "incompatibilities": ["Realistic Combat Mod"],
  "recommended_mods": ["Enhanced Graphics", "Better UI"]
}
```

### Distribution Channels

#### Official Mod Repository
```
Submission Process:
1. Create mod package
2. Test on multiple platforms
3. Write comprehensive documentation
4. Submit through mod portal
5. Await review and approval
6. Publish to mod repository
```

#### Community Distribution
```
Alternative Channels:
├── Personal websites
├── Mod hosting platforms
├── GitHub releases
├── Discord communities
├── Steam Workshop (if applicable)
└── Direct downloads
```

## 🛠️ Advanced Modding Techniques

### Patching Existing Content

#### JSON Patch Format
```json
{
  "patches": [
    {
      "target": "items/sword_of_fire",
      "operation": "add",
      "path": "/stats/fire_damage",
      "value": 25
    },
    {
      "target": "enemies/goblin",
      "operation": "replace",
      "path": "/stats/health",
      "value": 120
    },
    {
      "target": "skills/fireball",
      "operation": "merge",
      "value": {
        "damage": 80,
        "mana_cost": 25
      }
    }
  ]
}
```

### Custom Game Modes

#### Mode Configuration
```json
{
  "game_mode": {
    "id": "hardcore_permadeath",
    "name": "Hardcore Permadeath",
    "description": "One life, no saves, maximum challenge",
    "rules": {
      "permadeath": true,
      "no_saves": true,
      "increased_difficulty": 2.0,
      "special_events": true
    },
    "starting_conditions": {
      "gold": 0,
      "items": ["basic_sword", "basic_armor"],
      "level": 1
    },
    "victory_conditions": {
      "defeat_final_boss": true,
      "time_limit": null,
      "score_threshold": null
    }
  }
}
```

## 🔍 Troubleshooting and Support

### Common Issues

#### Asset Loading Problems
```
Problem: Textures not loading
Solutions:
├── Check file format (PNG required)
├── Verify file path in mod.json
├── Ensure power-of-2 dimensions
├── Check file permissions
└── Validate JSON syntax
```

#### Script Errors
```
Problem: Lua script not executing
Solutions:
├── Check syntax with Lua interpreter
├── Verify hook registration
├── Ensure correct function signatures
├── Check for missing dependencies
└── Review error logs
```

#### Compatibility Issues
```
Problem: Mod conflicts
Solutions:
├── Check mod load order
├── Review incompatibility lists
├── Test with minimal mod set
├── Update conflicting mods
└── Report to mod authors
```

### Debug Tools

#### Mod Debugging Console
```
Available Commands:
├── mod_debug on/off - Enable debug logging
├── mod_list - Show loaded mods with status
├── mod_validate - Check mod integrity
├── asset_list - Show loaded assets by mod
├── script_reload - Reload all scripts
└── error_log - Show recent errors
```

## 📚 Tutorials and Examples

### Beginner Tutorials
- **[Creating Your First Item](tutorials/first_item.md)** - Basic item creation
- **[Simple Enemy Mod](tutorials/simple_enemy.md)** - Enemy customization
- **[Texture Replacement](tutorials/texture_mod.md)** - Visual modifications

### Advanced Tutorials
- **[Custom Game Mode](tutorials/custom_gamemode.md)** - Complete game mode creation
- **[Scripted Behaviors](tutorials/scripted_ai.md)** - Advanced enemy AI
- **[Multi-Mod Compatibility](tutorials/mod_compatibility.md)** - Working with other mods

### Example Mods
- **Sample Content Pack** - Basic examples of all mod types
- **Balance Overhaul** - Complete gameplay modification
- **Visual Enhancement** - Graphics and UI improvements
- **Sound Pack** - Audio customization examples

---

## 🤝 Community Resources

### Getting Help
- **Official Forums** - Modding discussion and support
- **Discord Community** - Real-time help and collaboration
- **GitHub Issues** - Bug reports and feature requests
- **Wiki** - Community-maintained documentation

### Contributing to Modding Tools
- **Report Tool Issues** - Help improve modding infrastructure
- **Suggest Improvements** - Propose new modding features
- **Share Mods** - Contribute to the modding community
- **Document Techniques** - Write tutorials and guides

---

**Start your modding journey today! Whether you're creating simple texture replacements or complex gameplay overhauls, our comprehensive modding system provides the tools and documentation you need to bring your vision to life.**

*Modding documentation is continuously updated with new features and community contributions. Last updated: September 2025*
