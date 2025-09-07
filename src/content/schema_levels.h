/* schema_levels.h - Phase 2: Level / Map JSON schema */
#ifndef ROGUE_CONTENT_SCHEMA_LEVELS_H
#define ROGUE_CONTENT_SCHEMA_LEVELS_H

#include "../core/integration/json_schema.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Build the canonical Level/Map schema. Returns true on success. */
    bool rogue_levels_build_schema(RogueSchema* out_schema);

    /* Validate a JSON value representing a level/map definition. */
    bool rogue_levels_validate_json(const RogueJsonValue* json,
                                    RogueSchemaValidationResult* result);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CONTENT_SCHEMA_LEVELS_H */
