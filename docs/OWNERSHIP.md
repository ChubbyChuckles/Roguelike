# Module Ownership

| Path | Owner | Notes |
|------|-------|-------|
| `src/core` | Core Team | Game loop, combat, loot, persistence, skills. |
| `src/world` | World Gen | Tilemap, procedural generation, vegetation. |
| `src/graphics` | Rendering | Renderer, sprites, tile sprites, fonts. |
| `src/entities` | Gameplay | Player & enemy logic and data. |
| `src/input` | Input | Input mapping & events. |
| `src/util` | Shared | Logging, math helpers (pure utility / no upward deps). |
| `src/platform` | Platform | SDL2 abstraction & platform glue. |
| `tests` | QA | Unit & integration test harness. |

Ownership implies code review responsibility and enforcement of dependency boundaries defined in `DEPENDENCY_BOUNDARIES.md`.
