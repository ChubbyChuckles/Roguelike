#ifndef ROGUE_SCHEMA_ITEMS_H
#define ROGUE_SCHEMA_ITEMS_H

#include "../core/integration/json_schema.h"
#include "../core/loot/loot_item_defs.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Build the schema that describes a single item definition (not an array). */
    bool rogue_items_build_schema(RogueSchema* out_schema);

    /* Validate a single JSON object against the items schema. Root must be an object. */
    bool rogue_items_validate_item_json(const RogueJsonValue* json,
                                        RogueSchemaValidationResult* result);

    /* Convenience: validate an array of RogueItemDef entries by converting them to JSON
       objects and validating each against the schema. Returns true only if all pass. */
    bool rogue_items_validate_defs(const RogueItemDef* defs, int count,
                                   RogueSchemaValidationResult* result);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_SCHEMA_ITEMS_H */
