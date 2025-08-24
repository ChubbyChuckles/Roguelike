# Architecture Layers and Modules

This document tracks the evolving layered architecture and module targets introduced by the codebase optimization roadmap.

Layer order (top depends on below; never upward):
- UI
- Integration Bridges
- Gameplay (Combat/Skills/Buffs)
- Systems (Audio/VFX, Loot, Equipment, Progression, Vendor, Crafting, Worldgen, AI)
- Engine Core (EventBus, Snapshot/Tx/Rollback, Persistence, Resource, Math/Geom, Platform)

Initial concrete targets:
- rogue_core: legacy aggregate target, gradually slimming down
- rogue_audio_vfx (NEW): audio/vfx minimal pipeline extracted from core

Enforcement:
- Layering guard script: tools/code_audit/check_layering.py (wired to `ctest` as `code_audit_check_layering`).

Next candidates for extraction:
- rogue_systems_loot, rogue_systems_equipment
- rogue_ui (core UI primitives)
- rogue_math_geom (vec2/rect, distributions)

Notes:
- Targets use PUBLIC/PRIVATE include dirs to avoid transitive header leaks.
- SDL remains isolated behind thin adapters and is linked where needed.
