#ifndef ROGUE_WORLD_GEN_BIOME_ASSETS_VALIDATE_H
#define ROGUE_WORLD_GEN_BIOME_ASSETS_VALIDATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Validate a JSON object mapping biome names to an array of tile overrides objects
     * with keys: name (tile token), image (path), tx, ty. Ensures referenced image files exist.
     * Returns 1 on success, 0 on error and writes a message to err. */
    int rogue_biome_assets_validate_from_json(const char* json_text, char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_BIOME_ASSETS_VALIDATE_H */
