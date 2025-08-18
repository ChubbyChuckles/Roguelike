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

/* ---- Phase 7: Dungeon Generator (Graph-based rooms & corridors) ---- */
/* Dungeon room tag bitmask values (Phase 7.3 thematic tagging) */
#define ROGUE_DUNGEON_ROOM_TREASURE 0x1
#define ROGUE_DUNGEON_ROOM_ELITE    0x2
#define ROGUE_DUNGEON_ROOM_PUZZLE   0x4
typedef struct RogueDungeonRoom {
    int id;              /* room id */
    int x, y, w, h;      /* axis-aligned rectangle */
    int tag;             /* thematic tag bitmask (ROGUE_DUNGEON_ROOM_*) */
    int secret;          /* 1 if secret room */
} RogueDungeonRoom;

typedef struct RogueDungeonEdge { int a; int b; int loop; } RogueDungeonEdge; /* loop=1 if extra loop connection */

typedef struct RogueDungeonGraph {
    RogueDungeonRoom* rooms;
    int room_count;
    RogueDungeonEdge* edges;
    int edge_count;
} RogueDungeonGraph;

/* Generate a dungeon graph (rooms + corridors) honoring loop percentage target (0..100) */
bool rogue_dungeon_generate_graph(RogueWorldGenContext* ctx, int target_rooms, int loop_percent,
                                  RogueDungeonGraph* out_graph);
void rogue_dungeon_free_graph(RogueDungeonGraph* g);

/* Carve dungeon into tilemap region starting at (ox,oy) with given bounds (w,h). Returns carved floor count. */
int rogue_dungeon_carve_into_map(RogueWorldGenContext* ctx, RogueTileMap* io_map, const RogueDungeonGraph* graph,
                                 int ox, int oy, int w, int h);

/* Place keys and locked doors to enforce progression ordering; returns number of locked doors placed */
int rogue_dungeon_place_keys_and_locks(RogueWorldGenContext* ctx, RogueTileMap* io_map, const RogueDungeonGraph* graph);

/* Place traps & secret rooms; returns count of traps */
int rogue_dungeon_place_traps_and_secrets(RogueWorldGenContext* ctx, RogueTileMap* io_map, const RogueDungeonGraph* graph,
                                          int target_traps, double secret_room_chance);

/* Validation helpers */
int rogue_dungeon_validate_reachability(const RogueDungeonGraph* graph);
double rogue_dungeon_loop_ratio(const RogueDungeonGraph* graph);
int rogue_dungeon_secret_room_count(const RogueDungeonGraph* graph);

/* ---- Phase 8: Fauna & Spawn Ecology Layer ---- */
typedef struct RogueSpawnEntry {
    char id[32];      /* creature identifier */
    int weight;       /* common weight */
    int rare_weight;  /* weight used when rare roll triggers */
} RogueSpawnEntry;

typedef struct RogueSpawnTable {
    int biome_tile;                 /* representative base tile (e.g., GRASS / FOREST / DUNGEON_FLOOR) */
    int rare_chance_bp;             /* rare encounter chance in basis points (0..10000) */
    RogueSpawnEntry entries[16];
    int entry_count;
} RogueSpawnTable;

/* Density map influenced by elevation proxies & water proximity */
typedef struct RogueSpawnDensityMap {
    int width, height;
    float* density; /* length width*height */
} RogueSpawnDensityMap;

void rogue_spawn_clear_tables(void);
int  rogue_spawn_register_table(const RogueSpawnTable* table); /* returns index or -1 */
const RogueSpawnTable* rogue_spawn_get_table_for_tile(int tile_type);
/* Build density map (0..>=) using current tilemap */
bool rogue_spawn_build_density(const RogueTileMap* map, RogueSpawnDensityMap* out_dm);
void rogue_spawn_free_density(RogueSpawnDensityMap* dm);
/* Apply hub suppression (zero density inside radius, smooth falloff outside) */
void rogue_spawn_apply_hub_suppression(RogueSpawnDensityMap* dm, int hub_x, int hub_y, int radius);
/* Sample spawn id for tile position (x,y) using density + biome mapping; returns 1 on success */
int rogue_spawn_sample(RogueWorldGenContext* ctx, const RogueSpawnDensityMap* dm, const RogueTileMap* map,
                       int x, int y, char* out_id, size_t id_cap, int* out_is_rare);

/* ---- Phase 9: Lootable & Resource Nodes ---- */
typedef struct RogueResourceNodeDesc {
    char id[24];
    int rarity;          /* 0=common,1=uncommon,2=rare */
    int tool_tier;       /* required tool tier */
    int yield_min;
    int yield_max;
    unsigned int biome_mask; /* allowed biome bits */
} RogueResourceNodeDesc;

typedef struct RogueResourceNodePlacement {
    int x, y;
    int desc_index;
    int yield;
    int upgraded; /* 1 if upgraded variant */
} RogueResourceNodePlacement;

int  rogue_resource_register(const RogueResourceNodeDesc* d); /* returns index or -1 */
void rogue_resource_clear_registry(void);
int  rogue_resource_registry_count(void);
/* Generate clustered placements; returns count */
int  rogue_resource_generate(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, const RogueTileMap* map,
                              RogueResourceNodePlacement* out_array, int max_out, int cluster_attempts,
                              int cluster_radius, int base_clusters);
/* Count upgrades in an existing placement set */
int  rogue_resource_upgrade_count(const RogueResourceNodePlacement* nodes, int count);

/* ---- Phase 10: Weather & Environmental Simulation ---- */
typedef struct RogueWeatherPatternDesc {
    char id[24];            /* identifier */
    int min_duration_ticks; /* inclusive */
    int max_duration_ticks; /* inclusive */
    float intensity_min;    /* 0..1 */
    float intensity_max;    /* 0..1 */
    unsigned int biome_mask;/* allowed biome bits (reuses biome ids) */
    float base_weight;      /* probability weight (pre-biome scaling) */
} RogueWeatherPatternDesc;

typedef struct RogueActiveWeather {
    int pattern_index;      /* index into registry */
    int remaining_ticks;    /* ticks left in current pattern */
    float intensity;        /* current intensity (may lerp) */
    float target_intensity; /* target intensity for transition */
} RogueActiveWeather;

/* Register a weather pattern; returns index or -1 on failure */
int  rogue_weather_register(const RogueWeatherPatternDesc* d);
/* Clear registry (tests) */
void rogue_weather_clear_registry(void);
/* Count registry entries */
int  rogue_weather_registry_count(void);
/* Initialize scheduler state; selects first pattern deterministically (returns 1 on success) */
int  rogue_weather_init(RogueWorldGenContext* ctx, RogueActiveWeather* state);
/* Advance weather by ticks; may transition patterns when duration elapses; returns new pattern index if changed else -1 */
int  rogue_weather_update(RogueWorldGenContext* ctx, RogueActiveWeather* state, int ticks, int biome_id);
/* Sample current lighting tint modification (simple desaturation or color shift based on intensity) */
void rogue_weather_sample_lighting(const RogueActiveWeather* state, unsigned char* r, unsigned char* g, unsigned char* b);
/* Movement debuff factor (1.0 = none, <1 reduce speed) */
float rogue_weather_movement_factor(const RogueActiveWeather* state);


#endif
