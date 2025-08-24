/* Phase 3: Biome Partition & Data-Driven Descriptors (Descriptor & Registry API)
 * Provides a lightweight CFG descriptor parser (extensible to JSON later) for biome metadata:
 * - Tile palette weights (mapped to existing RogueTileType values)
 * - Vegetation & decoration densities
 * - Ambient color (r,g,b)
 * - Music track id (string token)
 * - Feature flags (allow_structures, allow_weather)
 * Parsing focuses on deterministic normalization & validation.
 */
#ifndef ROGUE_WORLD_GEN_BIOME_DESC_H
#define ROGUE_WORLD_GEN_BIOME_DESC_H

#include "tilemap.h"
#include <stddef.h>

#define ROGUE_BIOME_NAME_MAX 31
#define ROGUE_BIOME_MUSIC_MAX 31

typedef struct RogueBiomeDescriptor
{
    char name[ROGUE_BIOME_NAME_MAX + 1];
    int id;                             /* index in registry */
    float tile_weights[ROGUE_TILE_MAX]; /* normalized 0..1 */
    int tile_weight_count;              /* number of non-zero entries */
    float vegetation_density;           /* 0..1 */
    float decoration_density;           /* 0..1 */
    unsigned char ambient_color[3];
    char music_track[ROGUE_BIOME_MUSIC_MAX + 1];
    unsigned char allow_structures; /* boolean */
    unsigned char allow_weather;    /* boolean */
} RogueBiomeDescriptor;

typedef struct RogueBiomeRegistry
{
    RogueBiomeDescriptor* biomes;
    int count;
    int capacity;
} RogueBiomeRegistry;

/* Initialize empty registry */
void rogue_biome_registry_init(RogueBiomeRegistry* reg);
/* Free registry resources */
void rogue_biome_registry_free(RogueBiomeRegistry* reg);
/* Parse a single biome descriptor from CFG text (in-memory). Returns 1 on success, 0 on failure. */
int rogue_biome_descriptor_parse_cfg(const char* text, RogueBiomeDescriptor* out_desc, char* err,
                                     size_t err_sz);
/* Add descriptor to registry (makes a copy). Returns index or -1 on failure. */
int rogue_biome_registry_add(RogueBiomeRegistry* reg, const RogueBiomeDescriptor* desc);
/* Load all *.biome.cfg files from a directory (shallow). Returns number loaded or -1 on error. */
int rogue_biome_registry_load_dir(RogueBiomeRegistry* reg, const char* dir_path, char* err,
                                  size_t err_sz);
/* Blend two biome palettes linearly (t in [0,1]) into out_weights (size ROGUE_TILE_MAX). */
void rogue_biome_blend_palettes(const RogueBiomeDescriptor* a, const RogueBiomeDescriptor* b,
                                float t, float* out_weights);

#endif
