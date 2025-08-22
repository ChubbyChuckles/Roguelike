#ifndef ROGUE_WORLD_GEN_BIOME_JSON_H
#define ROGUE_WORLD_GEN_BIOME_JSON_H

#include "world/world_gen_biome_desc.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Parse a JSON array of biome objects and append to registry. Returns number added or -1 on
     * error. */
    int rogue_biome_registry_load_json_text(RogueBiomeRegistry* reg, const char* json_text,
                                            char* err, size_t err_cap);

    /* Basic balance validation across registry: checks densities in range and non-zero tile
     * weights. */
    int rogue_biome_registry_validate_balance(const RogueBiomeRegistry* reg, float veg_min,
                                              float veg_max, float deco_min, float deco_max,
                                              char* err, size_t err_cap);

    /* Build a compatibility/transition matrix from a JSON object mapping biome names to allowed
     * neighbors. The output matrix is reg->count x reg->count bytes (0/1) flattened row-major into
     * out_matrix with stride = reg->count. Returns 1 on success, 0 on error.
     */
    int rogue_biome_build_transition_matrix(const RogueBiomeRegistry* reg, const char* json_text,
                                            unsigned char* out_matrix, int out_cap, char* err,
                                            size_t err_cap);

    /* Validate encounter tables JSON (object: name -> array of strings). Ensures each registered
     * biome has a non-empty array. */
    int rogue_biome_validate_encounter_tables(const RogueBiomeRegistry* reg, const char* json_text,
                                              char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_BIOME_JSON_H */
