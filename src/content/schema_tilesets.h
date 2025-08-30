#ifndef ROGUE_CONTENT_SCHEMA_TILESETS_H
#define ROGUE_CONTENT_SCHEMA_TILESETS_H

#include "../core/integration/json_schema.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Build the canonical Tilesets schema (fields and constraints). Returns true on success. */
    bool rogue_tilesets_build_schema(RogueSchema* out_schema);

    /* Validate a JSON value that represents a tileset against the schema. */
    bool rogue_tilesets_validate_json(const RogueJsonValue* json,
                                      RogueSchemaValidationResult* result);

    /* Convenience: read a legacy tiles.cfg file, synthesize a JSON tileset, and validate. */
    /* tile_size<=0 defaults to 64. Returns true on success, fills out_count with number of tiles.
     */
    bool rogue_tilesets_validate_cfg_file(const char* path, int tile_size,
                                          RogueSchemaValidationResult* result, int* out_count);

    /* Convenience: try common relative roots for assets/tiles.cfg and validate it. */
    bool rogue_tilesets_validate_assets_default(RogueSchemaValidationResult* result,
                                                int* out_count);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CONTENT_SCHEMA_TILESETS_H */
