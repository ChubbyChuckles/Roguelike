@page debug_overlay_asset_creation_tutorial Debug Overlay Asset Creation Tutorial

# Debug Overlay Asset Creation Tutorial

## Overview

The Debug Overlay provides a comprehensive in-game interface for creating, editing, and testing game assets without restarting the application. This tutorial covers step-by-step procedures for adding new skills, items, map tiles, dialogues, and other assets using the overlay's panels.

## Accessing the Debug Overlay

1. **Launch the Game**: Start the roguelike application in debug mode
2. **Toggle Overlay**: Press **F1** to show/hide the debug overlay
3. **Navigation**: Use **Alt + Number** keys to switch between panels:
   - Alt+1: System
   - Alt+2: Items
   - Alt+3: Skills
   - Alt+4: Map
   - Alt+5: Audio/VFX
   - Alt+6: Entities
   - Alt+7: Content Graph
   - Alt+8: Validation
   - Alt+9: Dialogue

## Creating New Skills

### Step 1: Open Skills Panel
- Press **F1** to open overlay
- Press **Alt+3** or click "Skills" tab
- The panel shows Overview, Effects, Visuals, Audio, Testing tabs

### Step 2: Choose Skill Type
- In Overview tab, select skill type from dropdown:
  - MELEE: Physical attacks
  - RANGED: Projectile-based attacks
  - AOE_SPELL: Area-of-effect magic
  - BUFF: Beneficial status effects
  - DEBUFF: Harmful status effects
  - HEAL: Health restoration
  - SUMMON: Entity creation
  - PASSIVE: Always-active bonuses
  - ULTIMATE: Powerful special abilities

### Step 3: Configure Basic Properties
- **Skill ID**: Enter unique identifier (e.g., "fireball_spell")
- **Display Name**: Human-readable name
- **Max Rank**: Maximum skill level (1-10)
- **Passive Flag**: Check if always-active
- **Timing**: Set cast time, cooldown, channel duration

### Step 4: Add Effects (Effects Tab)
- Click **"Assign Primary"** to select main effect
- Choose from EffectSpec palette (filter by category)
- Configure effect parameters:
  - Damage: Base damage value
  - Healing: HP restoration amount
  - Duration: Effect length in seconds
  - Range: Effect radius
  - Cooldown: Time between uses

### Step 5: Add Visual Effects (Visuals Tab)
- **Cast Sprite**: Select animation for casting
- **Projectile Sprite**: Choose projectile appearance
- **Impact Sprite**: Set hit effect animation
- **AoE Sprite**: Define area indicator
- **Animation Params**: Set frame count, duration, loops

### Step 6: Configure Audio (Audio Tab)
- **Cast Sound**: Select casting audio cue
- **Impact Sound**: Choose hit sound effect
- **Loop Sound**: Set continuous audio (for channels)
- **Volume/Pitch**: Adjust audio parameters

### Step 7: Test the Skill (Testing Tab)
- Click **"Simulate"** to test skill execution
- View timeline showing cast/channel/cooldown phases
- Check damage numbers and effect application
- Adjust parameters and re-test

### Step 8: Save Changes
- Click **"Save Overrides JSON"** button
- Changes are saved to `build/skills_overrides.json`
- Use **"Load Overrides JSON"** to reload changes

## Creating New Items

### Step 1: Open Items Panel
- Press **F1** → **Alt+2** or click "Items" tab
- Panel shows item list with search/filter options

### Step 2: Create New Item
- Click **"Create New Item"** button
- Fill required fields:
  - **ID**: Unique identifier (e.g., "iron_sword")
  - **Name**: Display name
  - **Category**: Weapon, Armor, Consumable, Material, etc.

### Step 3: Configure Item Properties
- **Rarity**: Common, Uncommon, Rare, Epic, Legendary
- **Level**: Required character level
- **Stack Size**: Maximum stack count
- **Value**: Base gold value

### Step 4: Add Stats (Weapons/Armor)
- **Damage Range**: Min-Max physical damage (weapons)
- **Armor Value**: Defense rating (armor)
- **Sockets**: Number of gem slots (0-6)
- **Durability**: Maximum durability points

### Step 5: Configure Appearance
- **Sprite**: Select item icon
- **Color Tint**: Optional color modification
- **Size**: Sprite dimensions

### Step 6: Add Special Properties
- **Affix Slots**: Number of random modifiers
- **Quality**: Item quality multiplier
- **Tags**: Special flags (e.g., "two-handed", "magic")

### Step 7: Save and Test
- Click **"Create"** to add item to registry
- Item appears in list for immediate testing
- Use **"Save JSON"** to persist changes

## Editing Map Tiles

### Step 1: Open Map Panel
- Press **F1** → **Alt+4** or click "Map" tab
- Panel shows tile palette and brush controls

### Step 2: Select Tile Type
- Choose from tile categories:
  - Terrain: Grass, Dirt, Stone, Water
  - Structures: Walls, Doors, Chests
  - Decorations: Trees, Rocks, Furniture
  - Special: Spawners, Triggers, Portals

### Step 3: Configure Brush Settings
- **Brush Mode**: Single tile, Rectangle fill, Circle
- **Brush Size**: 1x1 to 5x5 area
- **Tile Variant**: Different appearances of same type

### Step 4: Paint Tiles
- **Click and Drag**: Paint tiles on map
- **Right-Click**: Pick tile from map
- **Shift+Click**: Rectangle selection
- **Ctrl+Click**: Fill connected area

### Step 5: Advanced Features
- **Layers**: Switch between background/foreground
- **Autotiling**: Enable automatic tile connections
- **Collision**: Toggle walkable/solid properties
- **Preview**: See brush ghost before placing

### Step 6: Save Changes
- Click **"Save JSON"** to export map
- Changes saved to `build/map.json`
- Use **"Load JSON"** to reload

## Creating New Dialogues

### Step 1: Open Dialogue Panel
- Press **F1** → **Alt+9** or click "Dialogue" tab
- Panel shows conversation controls

### Step 2: Create Dialogue Script
- Click **"New Script"** button
- Enter script properties:
  - **ID**: Unique identifier (e.g., "merchant_greeting")
  - **Speaker**: NPC name or "Player"
  - **Text**: Dialogue line content

### Step 3: Add Dialogue Lines
- Click **"Add Line"** for each conversation step
- Configure each line:
  - **Speaker**: Who is speaking
  - **Text**: What they say
  - **Effects**: Optional triggers (give item, set flag)
  - **Choices**: Branching options (if supported)

### Step 4: Configure Effects
- Add line effects:
  - **SET_FLAG**: Set game state flag
  - **GIVE_ITEM**: Grant item to player
  - **TELEPORT**: Move to location
  - **QUEST_START**: Begin quest

### Step 5: Test Conversation
- Click **"Start"** to begin dialogue
- Use **"Advance"** to progress through lines
- View conversation flow and effects
- Click **"Reset"** to restart

### Step 6: Style Configuration
- Adjust text appearance:
  - **Font Size**: Text scaling
  - **Color**: Text color
  - **Speed**: Typewriter effect speed
  - **Layout**: Box positioning

### Step 7: Save Dialogue
- Click **"Save"** to persist script
- Dialogue saved to internal registry
- Test in-game with appropriate triggers

## Creating Entities (Enemies/NPCs)

### Step 1: Open Entities Panel
- Press **F1** → **Alt+6** or click "Entities" tab
- Panel shows entity list and controls

### Step 2: Create New Entity
- Click **"Create New"** button
- Select entity type:
  - **Enemy**: Combat creatures
  - **NPC**: Non-combat characters
  - **Summon**: Player-controlled allies

### Step 3: Configure Basic Properties
- **ID**: Unique identifier
- **Name**: Display name
- **Type**: Enemy archetype (melee, ranged, magic)
- **Level**: Base level
- **Faction**: Ally, Enemy, Neutral

### Step 4: Set Stats and Behavior
- **Health**: HP pool
- **Damage**: Attack power
- **Speed**: Movement speed
- **AI Type**: Patrol, Aggressive, Defensive
- **Loot Table**: Drop chances

### Step 5: Configure Appearance
- **Sprite Sheet**: Animation sprites
- **Size**: Entity dimensions
- **Color**: Visual tint
- **Effects**: Particle attachments

### Step 6: Add Behaviors
- **Patrol Path**: Movement waypoints
- **Aggro Range**: Detection distance
- **Abilities**: Special attacks or spells
- **Dialogue**: NPC conversation scripts

### Step 7: Spawn and Test
- Click **"Spawn at Player"** to create instance
- Test behavior and interactions
- Use **"Kill"** or **"Teleport"** for testing
- Click **"Save"** to persist changes

## Audio/VFX Asset Creation

### Step 1: Open Audio/VFX Panel
- Press **F1** → **Alt+5** or click "Audio/VFX" tab
- Panel shows effect controls and previews

### Step 2: Create Audio Effect
- Click **"New Audio Effect"**
- Configure properties:
  - **ID**: Unique identifier
  - **File**: Audio file path
  - **Category**: SFX, Music, Ambient
  - **Volume**: Base volume level
  - **Loop**: Continuous playback

### Step 3: Create Visual Effect
- Click **"New VFX Effect"**
- Set basic parameters:
  - **ID**: Unique identifier
  - **Type**: Particle, Sprite, Screen Effect
  - **Duration**: Effect lifetime
  - **Layer**: Rendering layer

### Step 4: Configure Effect Details
- **Particles**: Emitter settings, count, speed
- **Sprites**: Animation frames, timing
- **Colors**: Tint and blending modes
- **Sounds**: Associated audio cues

### Step 5: Test Effects
- Click **"Play/Spawn"** buttons
- Adjust parameters in real-time
- View performance metrics
- Test at different distances

### Step 6: Save Configuration
- Click **"Save Config"**
- Effects saved to `assets/fx/` configs
- Hot-reload for immediate testing

## Validation and Error Handling

### Step 1: Open Validation Panel
- Press **F1** → **Alt+8** or click "Validation" tab
- Panel shows validation status

### Step 2: Run Validation
- Click **"Run Validation"** or **"Validate All"**
- Check for errors in:
  - Asset references
  - Parameter ranges
  - Cross-system dependencies
  - File existence

### Step 3: Fix Issues
- Click error messages for details
- Use **"Fix It"** buttons where available
- Manually correct invalid values
- Re-run validation to confirm fixes

### Step 4: Content Graph Analysis
- Press **F1** → **Alt+7** for Content Graph
- View dependency relationships
- Identify missing or orphaned assets
- Export analysis reports

## Best Practices

### General Tips
- **Save Frequently**: Use Save buttons to persist changes
- **Test Iteratively**: Test changes immediately after saving
- **Use Templates**: Duplicate existing assets as starting points
- **Validate Regularly**: Run validation to catch issues early

### Performance Considerations
- **Batch Changes**: Make multiple related changes before saving
- **Monitor FPS**: Watch for performance impact of new assets
- **Optimize Assets**: Compress images, reduce particle counts
- **Test on Target Hardware**: Validate on lower-end systems

### Organization
- **Naming Conventions**: Use consistent ID prefixes (e.g., "sword_iron")
- **Categorization**: Group related assets logically
- **Documentation**: Add comments to complex configurations
- **Version Control**: Track changes in external files

### Troubleshooting
- **Reload Issues**: Use Load buttons to refresh from disk
- **Validation Errors**: Check file paths and parameter ranges
- **Missing Assets**: Verify files exist in expected locations
- **Performance Problems**: Reduce complexity or enable LOD

## Keyboard Shortcuts

- **F1**: Toggle debug overlay
- **Alt+1-9**: Switch panels
- **Ctrl+S**: Save current panel
- **Ctrl+Z/Y**: Undo/redo (Map panel)
- **Ctrl+K**: Global search
- **Ctrl+Shift+P**: Command palette
- **Esc**: Clear focus/close modals
- **Enter/Space**: Activate buttons
- **Tab/Shift+Tab**: Navigate focus

## Advanced Features

### Hot Reload
- Changes to JSON files are automatically detected
- No restart required for most asset changes
- Use **"Auto-Reload"** toggles for continuous updates

### Templates and Presets
- Save custom templates for reuse
- Use **"Duplicate"** for similar assets
- Apply presets for common configurations

### Batch Operations
- Create multiple similar items at once
- Use pattern-based naming for series
- Export/import asset sets

### Real-time Preview
- See changes immediately in-game
- Test skills and effects without restarting
- Adjust parameters with live feedback

This tutorial covers the core workflows for asset creation using the debug overlay. For more advanced features, refer to the individual subsystem tutorials and roadmap documentation.
