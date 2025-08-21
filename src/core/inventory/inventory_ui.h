/* Inventory UI & Management (Roadmap 13.x)
 * Provides higher-level adapter between core inventory counts / item instances and UI grid widget.
 * Features:
 *  - 13.1 Build slot arrays (ids + counts) with stack aggregation (already counts) and capacity
 * clamp
 *  - 13.2 Sorting (by name, rarity, category, count) & basic filter (category mask, min rarity)
 *  - 13.3 Drag-and-drop equip integration helper (translate ui event -> equip try)
 *  - 13.4 Context actions: salvage & drop (spawn ground instance near player)
 *  - 13.5 Persistence of sort mode (stored in g_app persistent field, serialized in player
 * component tail)
 */
#ifndef ROGUE_INVENTORY_UI_H
#define ROGUE_INVENTORY_UI_H

#include <stdint.h>

/* Sort modes */
typedef enum RogueInventorySortMode
{
    ROGUE_INV_SORT_NONE = 0,
    ROGUE_INV_SORT_NAME = 1,
    ROGUE_INV_SORT_RARITY = 2,
    ROGUE_INV_SORT_CATEGORY = 3,
    ROGUE_INV_SORT_COUNT = 4
} RogueInventorySortMode;

typedef struct RogueInventoryFilter
{
    uint32_t category_mask; /* bit per RogueItemCategory (1<<cat); 0 => all */
    int min_rarity;         /* -1 => any */
} RogueInventoryFilter;

/* Build arrays of item definition indices and counts sized to slot_capacity.
 * Returns number of occupied slots (items that passed filter). */
int rogue_inventory_ui_build(int* out_ids, int* out_counts, int slot_capacity,
                             RogueInventorySortMode sort_mode, const RogueInventoryFilter* filter);

/* Apply a UI drag swap (already performed on arrays) to underlying inventory model: no-op because
 * inventory core is aggregated counts. Provided for future per-instance inventories; currently
 * returns 0. */
int rogue_inventory_ui_apply_swap(int from_slot, int to_slot, int* ids, int* counts,
                                  int slot_capacity);

/* Equip helper: attempts to equip first active item instance matching def index (weapon or armor).
 * Returns 0 success else <0. */
int rogue_inventory_ui_try_equip_def(int def_index);

/* Salvage helper: invokes salvage API producing materials added through inventory_add callback.
 * Returns produced material quantity or 0. */
int rogue_inventory_ui_salvage_def(int def_index);

/* Drop helper: spawns a ground item instance at player position (quantity=1) and decrements
 * inventory count. Returns inst index or <0 on failure. */
int rogue_inventory_ui_drop_one(int def_index);

/* Persistence accessor */
void rogue_inventory_ui_set_sort_mode(RogueInventorySortMode m);
RogueInventorySortMode rogue_inventory_ui_sort_mode(void);

#endif /* ROGUE_INVENTORY_UI_H */
