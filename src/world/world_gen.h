#ifndef ROGUE_WORLD_GEN_H
#define ROGUE_WORLD_GEN_H

#include "world/tilemap.h"
#include <stdbool.h>

typedef struct RogueWorldGenConfig {
    unsigned int seed;          /* RNG seed */
    int width;                  /* map width */
    int height;                 /* map height */
    int biome_regions;          /* number of voronoi biome seeds */
    int continent_count;        /* target number of continental landmasses (macro layout) */
    unsigned int biome_seed_offset; /* extra offset applied when seeding biome channel to decorrelate */
    int cave_iterations;        /* cellular automata smoothing iterations */
    double cave_fill_chance;    /* initial fill probability for caves */
    int river_attempts;         /* how many rivers to carve */
    int small_island_max_size;  /* max component size to collapse (salt/pepper) */
    int small_island_passes;    /* how many passes to run small-island smoothing */
    int shore_fill_passes;      /* how many passes to expand water to remove tiny land notches */
    int advanced_terrain;       /* enable advanced noise-based generation */
    double water_level;         /* base water elevation threshold (0-1), if 0 use default */
    int noise_octaves;          /* fbm octaves for elevation */
    double noise_gain;          /* fbm gain */
    double noise_lacunarity;    /* fbm lacunarity */
    int river_sources;          /* number of downhill-traced rivers (advanced) */
    int river_max_length;       /* max steps per river */
    double cave_mountain_elev_thresh; /* elevation above which caves may form (advanced) */
} RogueWorldGenConfig;

bool rogue_world_generate(RogueTileMap* out_map, const RogueWorldGenConfig* cfg);

/* ---- Phase 1 foundational types & APIs ---- */
#define ROGUE_WORLD_CHUNK_SIZE 32 /* tiles per dimension in a world chunk */
typedef struct RogueChunkCoord { int cx; int cy; } RogueChunkCoord;

/* Map a tile coordinate to its chunk coordinate */
RogueChunkCoord rogue_world_chunk_from_tile(int tx, int ty);
/* Get top-left origin tile of a chunk */
void rogue_world_chunk_origin(RogueChunkCoord cc, int* out_tx, int* out_ty);
/* Compute linear chunk index (row-major) given world tile width */
int rogue_world_chunk_index(RogueChunkCoord cc, int world_width_tiles);

/* Deterministic multi-channel RNG (macro/biome/micro) */
typedef struct RogueRngChannel { unsigned int state; } RogueRngChannel;
typedef struct RogueWorldGenContext {
    const RogueWorldGenConfig* cfg;    /* reference only */
    RogueRngChannel macro_rng;
    RogueRngChannel biome_rng;
    RogueRngChannel micro_rng;
} RogueWorldGenContext;

void rogue_worldgen_context_init(RogueWorldGenContext* ctx, const RogueWorldGenConfig* cfg);
void rogue_worldgen_context_shutdown(RogueWorldGenContext* ctx);
unsigned int rogue_worldgen_rand_u32(RogueRngChannel* ch);
double rogue_worldgen_rand_norm(RogueRngChannel* ch);

/* Hash utility for deterministic snapshot comparisons */
unsigned long long rogue_world_hash_tilemap(const RogueTileMap* map);

#endif
