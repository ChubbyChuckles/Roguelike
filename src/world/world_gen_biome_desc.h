/**
 * @file world_gen_biome_desc.h
 * @brief Biome descriptors and registry API for world generation.
 *
 * Phase 3: Biome Partition & Data-Driven Descriptors (Descriptor & Registry API)
 *
 * Provides a lightweight CFG descriptor parser (extensible to JSON later) for biome metadata:
 * - Tile palette weights (mapped to existing RogueTileType values)
 * - Vegetation & decoration densities
 * - Ambient color (r,g,b)
 * - Music track id (string token)
 * - Feature flags (allow_structures, allow_weather)
 *
 * Parsing focuses on deterministic normalization & validation.
 *
 * @ingroup WorldGen
 */
#ifndef ROGUE_WORLD_GEN_BIOME_DESC_H
#define ROGUE_WORLD_GEN_BIOME_DESC_H

#include "tilemap.h"
#include <stddef.h>

#define ROGUE_BIOME_NAME_MAX 31
#define ROGUE_BIOME_MUSIC_MAX 31

/**
 * @brief Data-driven biome descriptor.
 *
 * Represents a single biome's parameters used during world generation,
 * including palette weights and environmental settings.
 */
typedef struct RogueBiomeDescriptor
{
    char name[ROGUE_BIOME_NAME_MAX + 1]; /**< Biome display name (null-terminated). */
    int id; /**< Registry index assigned on add (>=0) or -1 when unset. */
    float tile_weights[ROGUE_TILE_MAX]; /**< Normalized palette weights per RogueTileType (sum to 1
                                           when valid). */
    int tile_weight_count;              /**< Number of entries with weight > 0. */
    float vegetation_density;           /**< Vegetation density in [0,1]. */
    float decoration_density;           /**< Decoration density in [0,1]. */
    unsigned char ambient_color[3];     /**< Ambient RGB color components (0..255). */
    char music_track[ROGUE_BIOME_MUSIC_MAX + 1]; /**< Music track token (null-terminated). */
    unsigned char allow_structures;              /**< Whether structures may spawn (boolean 0/1). */
    unsigned char allow_weather; /**< Whether weather effects may appear (boolean 0/1). */
} RogueBiomeDescriptor;

/**
 * @brief Collection of biome descriptors.
 *
 * Owns a dynamically sized array of `RogueBiomeDescriptor` objects.
 */
typedef struct RogueBiomeRegistry
{
    RogueBiomeDescriptor* biomes; /**< Owned array of descriptors (heap-allocated). */
    int count;                    /**< Number of valid descriptors in the array. */
    int capacity;                 /**< Allocated capacity of the array. */
} RogueBiomeRegistry;

/** @name Registry lifecycle */
/**@{*/
/**
 * @brief Initializes an empty registry.
 * @param[out] reg Registry to initialize (must not be NULL).
 */
void rogue_biome_registry_init(RogueBiomeRegistry* reg);
/**
 * @brief Frees resources owned by the registry and resets it to empty.
 * @param[in,out] reg Registry to free (safe to call with NULL).
 */
void rogue_biome_registry_free(RogueBiomeRegistry* reg);
/**@}*/

/**
 * @brief Parses a single biome descriptor from CFG text.
 *
 * Recognized keys include: name, music/music_track, vegetation_density,
 * decoration_density, ambient_color (r,g,b), allow_structures,
 * allow_weather, and tile_* or tile_weight_* entries matching
 * `RogueTileType` names.
 *
 * @param[in] text Null-terminated CFG text.
 * @param[out] out_desc Populated descriptor on success.
 * @param[out] err Optional buffer for error message.
 * @param[in] err_sz Size of error buffer.
 * @return 1 on success, 0 on parse/validation error.
 */
int rogue_biome_descriptor_parse_cfg(const char* text, RogueBiomeDescriptor* out_desc, char* err,
                                     size_t err_sz);

/**
 * @brief Adds a descriptor to the registry (copy), assigning its ID.
 * @param[in,out] reg Target registry.
 * @param[in] desc Descriptor to copy.
 * @return Assigned index (>=0) on success, -1 on failure.
 */
int rogue_biome_registry_add(RogueBiomeRegistry* reg, const RogueBiomeDescriptor* desc);

/**
 * @brief Loads all `*.biome.cfg` files from a directory (shallow).
 * @param[in,out] reg Target registry to populate.
 * @param[in] dir_path Directory path to scan.
 * @param[out] err Optional buffer for error details.
 * @param[in] err_sz Size of error buffer.
 * @return Number of loaded files on success, -1 on IO error.
 */
int rogue_biome_registry_load_dir(RogueBiomeRegistry* reg, const char* dir_path, char* err,
                                  size_t err_sz);

/**
 * @brief Blends two biome palettes linearly into `out_weights`.
 * @param[in] a First biome descriptor.
 * @param[in] b Second biome descriptor.
 * @param[in] t Interpolation factor in [0,1].
 * @param[out] out_weights Output array of size `ROGUE_TILE_MAX`.
 */
void rogue_biome_blend_palettes(const RogueBiomeDescriptor* a, const RogueBiomeDescriptor* b,
                                float t, float* out_weights);

#endif
