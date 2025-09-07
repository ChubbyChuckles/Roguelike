/* schema_sprites.h - Phase 2: Sprite sheet + animation schema */
#ifndef ROGUE_CONTENT_SCHEMA_SPRITES_H
#define ROGUE_CONTENT_SCHEMA_SPRITES_H

#include "../core/integration/json_schema.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Build the schema describing a sprite sheet definition object. */
    bool rogue_sprites_build_schema(RogueSchema* out_schema);

    /* Validate a JSON value representing a sprite sheet definition. */
    bool rogue_sprites_validate_json(const RogueJsonValue* json,
                                     RogueSchemaValidationResult* result);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CONTENT_SCHEMA_SPRITES_H */
