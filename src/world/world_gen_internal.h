/* Internal interface for world generation split modules (not public API). */
#ifndef ROGUE_WORLD_GEN_INTERNAL_H
#define ROGUE_WORLD_GEN_INTERNAL_H

#include "world_gen.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* RNG helpers */
    void rng_seed(unsigned int s);
    unsigned int rng_u32(void);
    double rng_norm(void);
    int rng_range(int lo, int hi);

    /* Noise helpers */
    double value_noise(double x, double y);
    double fbm(double x, double y, int octaves, double lacunarity, double gain);

    /* Phase functions */
    void wg_generate_base(RogueTileMap* map, const RogueWorldGenConfig* cfg);
    void wg_generate_caves(RogueTileMap* map, const RogueWorldGenConfig* cfg);
    void wg_carve_rivers(RogueTileMap* map, const RogueWorldGenConfig* cfg);
    void wg_apply_erosion(RogueTileMap* map);
    void wg_smooth_small_islands(RogueTileMap* map, int max_island_size, int target_tile,
                                 int replacement_bias);
    void wg_thicken_shores(RogueTileMap* map);
    void wg_advanced_post(RogueTileMap* map, const RogueWorldGenConfig* cfg);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_INTERNAL_H */
