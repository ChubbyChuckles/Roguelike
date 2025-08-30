#ifndef ROGUE_CONTENT_SCHEMA_ENTITIES_H
#define ROGUE_CONTENT_SCHEMA_ENTITIES_H

#include "../core/integration/json_schema.h"
#include "../entities/enemy.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Build the canonical Entities schema (fields and constraints). Returns true on success. */
    bool rogue_entities_build_schema(RogueSchema* out_schema);

    /* Validate an array of RogueEnemyTypeDef against the schema. Aggregates errors into result. */
    bool rogue_entities_validate_types(const RogueEnemyTypeDef* types, int count,
                                       RogueSchemaValidationResult* result);

    /* Convenience: load types from assets/enemies (searching a few relative roots) and validate. */
    bool rogue_entities_validate_assets_default(RogueSchemaValidationResult* result,
                                                int* out_count);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CONTENT_SCHEMA_ENTITIES_H */
