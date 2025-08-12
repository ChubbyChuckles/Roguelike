#ifndef ROGUE_WORLD_GEN_H
#define ROGUE_WORLD_GEN_H

#include "world/tilemap.h"
#include <stdbool.h>

typedef struct RogueWorldGenConfig {
    unsigned int seed;          /* RNG seed */
    int width;                  /* map width */
    int height;                 /* map height */
    int biome_regions;          /* number of voronoi biome seeds */
    int cave_iterations;        /* cellular automata smoothing iterations */
    double cave_fill_chance;    /* initial fill probability for caves */
    int river_attempts;         /* how many rivers to carve */
    int small_island_max_size;  /* max component size to collapse (salt/pepper) */
    int small_island_passes;    /* how many passes to run small-island smoothing */
    int shore_fill_passes;      /* how many passes to expand water to remove tiny land notches */
    int advanced_terrain;       /* enable advanced noise-based generation */
    double water_level;         /* base water elevation threshold (0-1), if 0 use default */
} RogueWorldGenConfig;

bool rogue_world_generate(RogueTileMap* out_map, const RogueWorldGenConfig* cfg);

#endif
