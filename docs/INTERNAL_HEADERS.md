# Internal Header Compliance

Internal (private) headers should be suffixed with `_internal.h` and only included by `.c` files within the same module directory tree. Current snapshot:

| Internal Header | Module | Status |
|-----------------|--------|--------|
| `skills_internal.h` | core/ (skills) | OK |
| `vegetation_internal.h` | core/ (vegetation) | OK |
| `enemy_system_internal.h` | core/ (enemy_system) | OK |
| `world_gen_internal.h` | world/ | OK |
| `tile_sprites_internal.h` | graphics/ | OK |
| `persistence_internal.h` | core/ (persistence) | OK |
| `projectiles_internal.h` | core/ (projectiles) | OK |

Future internal headers MUST be documented here when added or removed.
