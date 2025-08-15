/* Internal shared state for vegetation system (not part of public API). */
#ifndef ROGUE_VEGETATION_INTERNAL_H
#define ROGUE_VEGETATION_INTERNAL_H

#include "core/vegetation.h"

#ifdef __cplusplus
extern "C" { 
#endif

#define ROGUE_MAX_VEG_DEFS 256
#define ROGUE_MAX_VEG_INSTANCES 4096

extern RogueVegetationDef g_defs[ROGUE_MAX_VEG_DEFS];
extern int g_def_count;
extern RogueVegetationInstance g_instances[ROGUE_MAX_VEG_INSTANCES];
extern int g_instance_count;
extern int g_trunk_collision_enabled;
extern int g_canopy_tile_blocking_enabled;
extern float g_target_tree_cover;
extern unsigned int g_last_seed;
extern unsigned int vrng_state; /* rng state */

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VEGETATION_INTERNAL_H */
