#ifndef ROGUE_CORE_LOOT_ITEM_DEBUG_H
#define ROGUE_CORE_LOOT_ITEM_DEBUG_H

#include "loot_item_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Headless-safe debug helpers for the overlay and tests. */

    /* Number of loaded item base definitions. */
    int rogue_item_debug_count(void);

    /* Get const pointer to item def by index; NULL if OOB. */
    const RogueItemDef* rogue_item_debug_get(int index);

    /* Set common integer fields by name. Returns 0 on success. */
    int rogue_item_debug_set_int(int index, const char* field, int value);

    /* Set display name (clamped to buffer). Returns 0 on success. */
    int rogue_item_debug_set_name(int index, const char* name);

    /* Persist current registry to JSON at path using atomic write. Returns 0 on success. */
    int rogue_item_debug_save_json(const char* path);

    /* Load/merge item defs from JSON at path (updates existing IDs, appends new). Returns added
     * count or <0 on error. */
    int rogue_item_debug_load_json(const char* path);

    /* Create a new item definition with the provided fields. On success, returns the new index.
        Required: non-empty unique id and name. Category must be within RogueItemCategory range.
        Numeric fields are sanitized (stack_max>=1, sockets clamped 0..6, max>=min). */
    int rogue_item_debug_create(const char* id, const char* name, RogueItemCategory category,
                                int level_req, int stack_max, int base_value, int base_dmg_min,
                                int base_dmg_max, int base_armor, int rarity, int socket_min,
                                int socket_max);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_LOOT_ITEM_DEBUG_H */
