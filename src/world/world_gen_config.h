/* World generation config construction helpers (extracted from scattered initializers)
 * Provides unified builder to reduce duplication in app/input code. */
#ifndef ROGUE_WORLD_GEN_CONFIG_HELPERS_H
#define ROGUE_WORLD_GEN_CONFIG_HELPERS_H

#include "world_gen.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Build a RogueWorldGenConfig.
     * seed: RNG seed to use.
     * use_app_params: if non-zero, pull tunable params (water level, noise, rivers, cave thresh)
     * from g_app.* (persisted values). if zero, use default baseline constants (same as persistence
     * defaults / start screen quick-gen path). apply_scale: if non-zero, apply legacy scaling
     * (width*=10, height*=10, biome_regions=1000).
     */
    RogueWorldGenConfig rogue_world_gen_config_build(unsigned int seed, int use_app_params,
                                                     int apply_scale);

#ifdef __cplusplus
}
#endif

#endif
