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

/* ---- Phase 2: Macro Layout / Biome Classification ---- */
typedef enum RogueBiomeId {
    ROGUE_BIOME_OCEAN = 0,
    ROGUE_BIOME_PLAINS,
    ROGUE_BIOME_FOREST_BIOME,
    ROGUE_BIOME_MOUNTAIN_BIOME,
    ROGUE_BIOME_SNOW_BIOME,
    ROGUE_BIOME_SWAMP_BIOME,
    ROGUE_BIOME_MAX
} RogueBiomeId;

/* Generate macro-scale world layout & classify biomes.
 * Populates out_map tiles (water / land biome tiles + rivers) and returns biome histogram & continent count if provided.
 * Returns false on allocation failure or invalid args. Deterministic for (cfg->seed).
 */
bool rogue_world_generate_macro_layout(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* out_map,
                                       int* out_biome_histogram, int* out_continent_count);

/* ---- Phase 4: Local Terrain & Caves (new APIs) ---- */
typedef struct RoguePassabilityMap {
    int width;
    int height;
    unsigned char* walkable; /* 1 = walkable, 0 = blocked */
} RoguePassabilityMap;

bool rogue_world_generate_local_terrain(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map);
bool rogue_world_generate_caves_layer(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map);
bool rogue_world_place_lava_and_liquids(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map, int target_pockets);
bool rogue_world_place_ore_veins(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map, int target_veins, int vein_len);
bool rogue_world_build_passability(const RogueWorldGenConfig* cfg, const RogueTileMap* map, RoguePassabilityMap* out_pass);
void rogue_world_passability_free(RoguePassabilityMap* pass);

/* ---- Phase 5: Rivers & Erosion Detailing ---- */
bool rogue_world_refine_rivers(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map);
bool rogue_world_apply_erosion(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map, int thermal_passes, int hydraulic_passes);
int  rogue_world_mark_bridge_hints(const RogueWorldGenConfig* cfg, const RogueTileMap* map, int min_gap, int max_gap);
/* Utility to compute steepness variance for tests */
double rogue_world_compute_steepness_metric(const RogueTileMap* before, const RogueTileMap* after);

/* ---- Phase 6: Structures & Points of Interest ---- */
typedef struct RogueStructureDesc {
    const char* id;              /* unique identifier */
    int width;                   /* footprint width */
    int height;                  /* footprint height */
    unsigned int biome_mask;     /* bitmask of allowed biomes */
    double rarity;               /* probability weight */
    int min_elevation;           /* min elevation heuristic (0-3) */
    int max_elevation;           /* max elevation heuristic (0-3) */
    int allow_rotation;          /* whether rotations permitted */
} RogueStructureDesc;

typedef struct RogueStructurePlacement {
    int x; int y;                /* top-left tile */
    int w; int h;                /* final placed size (post-rotation) */
    int rotation;                /* 0,1,2,3 quarter turns */
    const RogueStructureDesc* desc; /* descriptor reference */
} RogueStructurePlacement;

/* Register built-in baseline structures (hardcoded for now; later data-driven) */
int  rogue_world_register_default_structures(void);
/* Returns number of registered structure descriptors */
int  rogue_world_structure_desc_count(void);
/* Get descriptor by index */
const RogueStructureDesc* rogue_world_get_structure_desc(int index);
/* Clear structure registry (used in tests) */
void rogue_world_clear_structure_registry(void);

/* Placement: runs Poisson-disk style sampling with overlap & biome constraints */
int rogue_world_place_structures(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map,
                                 RogueStructurePlacement* out_array, int max_out, int min_spacing);

/* Dungeon entrance linking: mark N entrances near structure centers or biome transitions */
int rogue_world_place_dungeon_entrances(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map,
                                       const RogueStructurePlacement* placements, int placement_count, int max_entrances);



#endif
