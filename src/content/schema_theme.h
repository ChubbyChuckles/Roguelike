/* schema_theme.h - Phase 2: UI Theme/Layout JSON schema (Asset Structure Plan)
   Provides validation for migrating .cfg based theme definitions into a structured JSON form.
   Initial fields mirror existing key=value ui_theme_default.cfg. Future expansion allows layout. */
#ifndef ROGUE_CONTENT_SCHEMA_THEME_H
#define ROGUE_CONTENT_SCHEMA_THEME_H

#include "../core/integration/json_schema.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Build the UI theme schema. */
    bool rogue_theme_build_schema(RogueSchema* out_schema);
    /* Validate a JSON value representing a UI theme definition. */
    bool rogue_theme_validate_json(const RogueJsonValue* json, RogueSchemaValidationResult* result);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CONTENT_SCHEMA_THEME_H */
