@page ai_system_tutorial AI System Tutorial

# AI System Tutorial

## Overview

The AI & Behaviour System provides a deterministic, modular enemy (and potential ally) behaviour framework enabling composable decision logic, perception, tactical coordination, and difficulty scaling. It emphasizes reproducibility (seeded), debug introspection (behavior trace), and future multiplayer server authority compatibility.

## Key Components

### Behavior Tree Architecture
- **Selector and Sequence Composites**: Basic control flow for decision trees.
- **Blackboard**: Typed memory for transient data like last seen player position.
- **Utility AI**: Dynamic scorer for leaf nodes.

### Perception System
- **LOS Raycast**: Tile-based line-of-sight with blocking predicates.
- **Vision Cone**: Angular and distance-based detection.
- **Hearing Events**: Ring buffer for sound-based alerts.
- **Threat Accumulation**: Gain and decay mechanics for alert states.

### Movement & Pathing
- **Flow Field Precomputation**: Dijkstra-based navigation with cost maps.
- **Local Avoidance**: Steering to avoid immediate collisions.
- **Path Smoothing**: Corner cutting for natural movement.
- **Stuck Detection**: Recovery actions for blocked paths.

### Combat Behaviors
- **Ranged Attack Integration**: Projectile spawning with gating.
- **Reaction Windows**: Parry and dodge with timers.
- **Opportunistic Attacks**: Conditional strikes during player recovery.
- **Kiting Logic**: Distance band maintenance.
- **Focus Fire Coordination**: Threat leader broadcasts.

### Group Tactics
- **Squad Formation**: Shared blackboard for coordinated actions.
- **Role Assignment**: Emergent roles via utility scoring.
- **Surround and Regroup**: Geometric positioning and movement.
- **Chain Attacks**: Staggered entry for multi-enemy assaults.

### Performance & Scaling
- **Tick Budgeting**: Per-agent time limits with staggering.
- **Incremental Evaluation**: Spread heavy computations.
- **LOD Behavior**: Reduced detail for off-screen agents.
- **Agent Pooling**: Reusable blackboard instances.

### Debugging & Tooling
- **Behavior Trace**: Ring buffer for execution history.
- **Perception Overlay**: Visual cones and LOS rays.
- **Blackboard Inspector**: Console commands for state inspection.
- **Determinism Verifier**: Dual-run hash comparison.

## Usage Example

To integrate AI into an enemy:

1. Initialize the AI context with a seeded RNG.
2. Build a behavior tree using the provided nodes.
3. Set up perception parameters (vision cone, hearing radius).
4. Register the tree with the enemy AI system.
5. Update the AI each frame with player state.

## Cross-System Integrations

- **Combat**: Provides damage events for threat updates.
- **Progression**: Difficulty modifiers adjust aggression and reaction times.
- **World Generation**: Biome tags influence utility scores for cover seeking.
- **Skills**: Cooldown states exposed for AI decision gating.

## Best Practices

- Use seeded RNG for reproducible behavior in testing.
- Keep behavior trees modular for easy composition.
- Monitor performance budgets to avoid frame drops.
- Leverage the trace system for debugging complex behaviors.

## Further Reading

Refer to the roadmap file `roadmaps/implementation_plan_aisystem.txt` for detailed implementation phases and status.
