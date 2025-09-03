@page audio_vfx_tutorial Audio & VFX Tutorial

# Audio & VFX Tutorial

## Overview

The Audio & VFX Pipeline delivers a unified event-driven audio & visual effects system decoupled from gameplay logic. Goals: minimal latency, pooling, layering, deterministic replay, data-driven authoring, dynamic mixing (ducking, side-chain), adaptive effects scaling (performance modes), and robust headless validation of emission ordering.

## Key Components

### Event System
- **EffectEvent Struct**: Type, payload, timing
- **Global Bus Queue**: Double-buffer for deterministic ordering
- **Submission API**: rogue_fx_emit with priority classification

### Audio Subsystem
- **AudioRegistry**: Sound metadata (path, category, volume)
- **Lazy Loading**: On-demand Mix_Chunk loading
- **Channel Mixer**: Gain per category, master mute
- **Voice Pool**: Polyphony limiting
- **Positional Audio**: 2D panning and distance attenuation

### VFX Subsystem
- **VfxRegistry**: Effect metadata (layer, lifetime, emitters)
- **Particle System**: Emitters, spawn rate, lifetime, pooling
- **Layer Ordering**: Background, Mid, Foreground, UI
- **Time Scaling**: Hitstop freeze integration
- **Screen vs World Space**: Camera transform support

### Authoring & Data Format
- **Config Schema**: assets/fx/*.cfg for effects
- **Hot Reload**: File change detection and validation
- **Effect Composition**: Chain and parallel blocks
- **Parameter Overrides**: Color, scale, lifetime variations
- **Random Distributions**: Uniform/normal for variation

### Gameplay Binding
- **Mapping Table**: Event -> effect ID with priorities
- **Damage Hooks**: Per-type layering
- **Skill Cues**: Activation and completion triggers
- **Loot Rarity**: Sparkle effects for drops

### Advanced Audio Features
- **Music State Machine**: Explore, Combat, Boss transitions
- **Cross-fade**: Beat-aligned transitions
- **Side-chain Ducking**: UI/SFX lower music
- **Procedural Layering**: Base + sweeteners with gain scaling
- **Environmental Reverb**: Preset simulation with smooth mixing

### Advanced VFX Features
- **Blend Modes**: Alpha, Add, Multiply
- **Trail Emitters**: Motion vectors with perf scaling
- **Screen Shake**: Priority stacking with decay
- **Post-processing**: Bloom, color LUT
- **Performance Scaling**: Particle count reduction under load
- **Decals**: Ground projection with lifetime

### Performance & Budgeting
- **Frame Profiler**: Spawn counts, active particles
- **Soft/Hard Budgets**: Cull or LOD effects
- **Frame Pacing**: Defer heavy spawns
- **Pool Auditing**: Fragmentation detection

### Determinism & Replay
- **Stable Sort**: Emit frame, priority, id, seq
- **Replay Log**: Event serialization
- **Hashing**: Stream validation
- **Divergence Detector**: Live vs expected comparison

## Usage Example

To emit an effect:

1. Define effect in config with parameters.
2. Map gameplay event to effect ID.
3. Call rogue_fx_emit with position and context.
4. System handles audio mixer and VFX spawner.

## Cross-System Integrations

- **Combat**: Damage events trigger layered cues
- **Skills**: Activation cues and proc triggers
- **Loot**: Rarity-driven visual flourishes
- **AI**: Ambient and boss phase audio

## Best Practices

- Use deterministic seeds for variation.
- Implement performance scaling for low-end hardware.
- Validate asset references in configs.
- Monitor frame budgets for heavy effects.

## Further Reading

Refer to `roadmaps/implementation_plan_audiovfxsystem.txt`.
