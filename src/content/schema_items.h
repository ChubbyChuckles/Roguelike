#ifndef ROGUE_SCHEMA_ITEMS_H
#define ROGUE_SCHEMA_ITEMS_H

#include "../core/integration/json_schema.h"
#include "../core/loot/loot_item_defs.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Current JSON schema version for item definitions. Increment when fields/semantics evolve. */
#ifndef ROGUE_ITEMS_SCHEMA_VERSION_CURRENT
#define ROGUE_ITEMS_SCHEMA_VERSION_CURRENT 1u
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

    /* Migration hook: optional callback to migrate defs from an older schema-version in place. */
    typedef void (*RogueItemsMigrateFn)(RogueItemDef* defs, int count, unsigned from_version,
                                        unsigned to_version);

    /* Set a global migration hook. Pass NULL to clear. Thread-unsafe; call during init. */
    void rogue_items_set_migration_hook(RogueItemsMigrateFn fn);

    /* Invoke migration (no-op if no hook set). Safe to call with from==to. */
    void rogue_items_migrate(RogueItemDef* defs, int count, unsigned from_version,
                             unsigned to_version);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_SCHEMA_ITEMS_H */
