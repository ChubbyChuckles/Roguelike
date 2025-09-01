#include "inventory_ui.h"
#include "../app/app_state.h"
#include "../equipment/equipment.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include "../vendor/salvage.h"
#include "inventory.h"
#include "inventory_entries.h" /* use unified entries for quantity/remove */
#include "inventory_tags.h"    /* lock/favorite enforcement */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file inventory_ui.c
 * @brief Provides UI-related functionality for inventory management, including sorting, filtering,
 * and actions like equipping, salvaging, and dropping items.
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 *
 * This file implements the user interface layer for inventory operations, supporting sorting modes,
 * filtering, and integration with equipment, salvage, and loot systems.
 */

/* Extend RogueAppState with sort mode via weak linkage (we can't modify struct layout here without
 * broader ripple; instead we store in static and persist via player component tail extension if
 * integrated). */
/** @brief Global sort mode for inventory UI. */
static RogueInventorySortMode g_sort_mode = ROGUE_INV_SORT_NONE;

RogueInventorySortMode rogue_inventory_ui_sort_mode(void) { return g_sort_mode; }
/**
 * @brief Retrieves the current inventory sort mode.
 *
 * @return The current sort mode.
 */
RogueInventorySortMode rogue_inventory_ui_sort_mode(void) { return g_sort_mode; }
/**
 * @brief Sets the inventory sort mode.
 *
 * @param m The new sort mode.
 */
void rogue_inventory_ui_set_sort_mode(RogueInventorySortMode m) { g_sort_mode = m; }

/**
 * @brief Temporary structure for sorting inventory entries.
 *
 * This struct holds the definition index and count for sorting operations.
 */
typedef struct TmpEntry
{
    int def_index; /**< @brief The item definition index. */
    int count;     /**< @brief The quantity of the item. */
} TmpEntry;

/**
 * @brief Performs case-insensitive string comparison.
 *
 * @param a First string.
 * @param b Second string.
 * @return Negative if a < b, 0 if equal, positive if a > b.
 *
 * This function compares strings ignoring case differences.
 */
static int ci_cmp(const char* a, const char* b)
{
    unsigned char ca, cb;
    while (*a && *b)
    {
        ca = (unsigned char) tolower((unsigned char) *a);
        cb = (unsigned char) tolower((unsigned char) *b);
        if (ca != cb)
            return (int) ca - (int) cb;
        a++;
        b++;
    }
    return (int) (unsigned char) tolower((unsigned char) *a) -
           (int) (unsigned char) tolower((unsigned char) *b);
}
/**
 * @brief Comparator for sorting by item name (case-insensitive).
 *
 * @param a Pointer to first TmpEntry.
 * @param b Pointer to second TmpEntry.
 * @return Comparison result for qsort.
 */
static int cmp_name(const void* a, const void* b)
{
    int ia = ((const TmpEntry*) a)->def_index;
    int ib = ((const TmpEntry*) b)->def_index;
    const RogueItemDef* da = rogue_item_def_at(ia);
    const RogueItemDef* db = rogue_item_def_at(ib);
    if (!da || !db)
        return ia - ib;
    int r = ci_cmp(da->name, db->name);
    if (r == 0)
        return ia - ib;
    return r;
}
/**
 * @brief Comparator for sorting by item rarity (descending).
 *
 * @param a Pointer to first TmpEntry.
 * @param b Pointer to second TmpEntry.
 * @return Comparison result for qsort.
 */
static int cmp_rarity(const void* a, const void* b)
{
    int ia = ((const TmpEntry*) a)->def_index;
    int ib = ((const TmpEntry*) b)->def_index;
    const RogueItemDef* da = rogue_item_def_at(ia);
    const RogueItemDef* db = rogue_item_def_at(ib);
    int ra = da ? da->rarity : 0;
    int rb = db ? db->rarity : 0;
    if (rb != ra)
        return rb - ra;
    return ia - ib;
}
/**
 * @brief Comparator for sorting by item category.
 *
 * @param a Pointer to first TmpEntry.
 * @param b Pointer to second TmpEntry.
 * @return Comparison result for qsort.
 */
static int cmp_category(const void* a, const void* b)
{
    int ia = ((const TmpEntry*) a)->def_index;
    int ib = ((const TmpEntry*) b)->def_index;
    const RogueItemDef* da = rogue_item_def_at(ia);
    const RogueItemDef* db = rogue_item_def_at(ib);
    int ca = da ? da->category : 0;
    int cb = db ? db->category : 0;
    if (ca != cb)
        return ca - cb;
    return ia - ib;
}
/**
 * @brief Comparator for sorting by item count (descending).
 *
 * @param a Pointer to first TmpEntry.
 * @param b Pointer to second TmpEntry.
 * @return Comparison result for qsort.
 */
static int cmp_count(const void* a, const void* b)
{
    int ca = ((const TmpEntry*) a)->count;
    int cb = ((const TmpEntry*) b)->count;
    if (cb != ca)
        return cb - ca;
    return ((const TmpEntry*) a)->def_index - ((const TmpEntry*) b)->def_index;
}

/**
 * @brief Builds a sorted and filtered list of inventory items for UI display.
 *
 * @param out_ids Array to store item definition indices.
 * @param out_counts Array to store corresponding quantities.
 * @param slot_capacity Maximum number of slots to fill.
 * @param sort_mode Sorting mode to apply.
 * @param filter Optional filter to apply.
 * @return Number of items placed in the arrays.
 *
 * This function collects inventory items, applies filtering and sorting, and populates the output
 * arrays.
 */
int rogue_inventory_ui_build(int* out_ids, int* out_counts, int slot_capacity,
                             RogueInventorySortMode sort_mode, const RogueInventoryFilter* filter)
{
    if (!out_ids || !out_counts || slot_capacity <= 0)
        return 0;
    int total = 0;
    int cap = slot_capacity;
    TmpEntry* tmp = (TmpEntry*) malloc(sizeof(TmpEntry) * ROGUE_ITEM_DEF_CAP);
    if (!tmp)
        return 0;
    int tcount = 0;
    for (int i = 0; i < ROGUE_ITEM_DEF_CAP; i++)
    {
        int c = rogue_inventory_get_count(i);
        if (c <= 0)
            continue;
        const RogueItemDef* d = rogue_item_def_at(i);
        if (!d)
            continue;
        if (filter)
        {
            if (filter->category_mask && ((filter->category_mask & (1u << d->category)) == 0))
                continue;
            if (filter->min_rarity >= 0 && d->rarity < filter->min_rarity)
                continue;
        }
        tmp[tcount].def_index = i;
        tmp[tcount].count = c;
        tcount++;
    }
    switch (sort_mode)
    {
    case ROGUE_INV_SORT_NAME:
        qsort(tmp, tcount, sizeof(TmpEntry), cmp_name);
        break;
    case ROGUE_INV_SORT_RARITY:
        qsort(tmp, tcount, sizeof(TmpEntry), cmp_rarity);
        break;
    case ROGUE_INV_SORT_CATEGORY:
        qsort(tmp, tcount, sizeof(TmpEntry), cmp_category);
        break;
    case ROGUE_INV_SORT_COUNT:
        qsort(tmp, tcount, sizeof(TmpEntry), cmp_count);
        break;
    default:
        break; /* insertion order (definition index) */
    }
    if (tcount > cap)
        tcount = cap;
    for (int i = 0; i < cap; i++)
    {
        out_ids[i] = 0;
        out_counts[i] = 0;
    }
    for (int i = 0; i < tcount; i++)
    {
        out_ids[i] = tmp[i].def_index;
        out_counts[i] = tmp[i].count;
        total++;
    }
    free(tmp);
    return total;
}

/**
 * @brief Applies a swap operation between inventory slots (placeholder).
 *
 * @param from_slot Source slot index.
 * @param to_slot Destination slot index.
 * @param ids Array of item IDs.
 * @param counts Array of item counts.
 * @param slot_capacity Total slot capacity.
 * @return 0 (currently a no-op).
 *
 * This function is a placeholder for implementing slot swapping in the UI.
 */
int rogue_inventory_ui_apply_swap(int from_slot, int to_slot, int* ids, int* counts,
                                  int slot_capacity)
{
    (void) from_slot;
    (void) to_slot;
    (void) ids;
    (void) counts;
    (void) slot_capacity;
    return 0;
}

/* Helper: locate first active instance with def index */
/**
 * @brief Finds the first active item instance for a given definition index.
 *
 * @param def_index The item definition index.
 * @return Instance index, or -1 if not found.
 *
 * This function searches the global item instances for an active one matching the def_index.
 */
static int find_instance_for_def(int def_index)
{
    if (!g_app.item_instances)
        return -1;
    for (int i = 0; i < g_app.item_instance_cap; i++)
    {
        if (g_app.item_instances[i].active && g_app.item_instances[i].def_index == def_index)
            return i;
    }
    return -1;
}

/**
 * @brief Attempts to equip an item from the inventory.
 *
 * @param def_index The item definition index.
 * @return 0 on success, negative on failure.
 *
 * This function tries to equip the item, fabricating a temporary instance if necessary.
 */
int rogue_inventory_ui_try_equip_def(int def_index)
{
    const RogueItemDef* d = rogue_item_def_at(def_index);
    if (!d)
        return -1;
    int inst = find_instance_for_def(def_index);
    if (inst < 0)
    { /* fabricate temp instance (non-persistent) */
        inst = rogue_items_spawn(def_index, 1, 0, 0);
        if (inst < 0)
            return -2;
    }
    if (d->category == ROGUE_ITEM_WEAPON)
        return rogue_equip_try(ROGUE_EQUIP_WEAPON, inst);
    if (d->category == ROGUE_ITEM_ARMOR)
        return rogue_equip_try(ROGUE_EQUIP_ARMOR_CHEST, inst);
    return -3;
}

/**
 * @brief Salvages an item from the inventory for resources.
 *
 * @param def_index The item definition index.
 * @return Number of resources produced, or 0 if not salvageable.
 *
 * This function attempts to salvage the item, removing it from inventory and adding resources.
 */
int rogue_inventory_ui_salvage_def(int def_index)
{
    const RogueItemDef* d = rogue_item_def_at(def_index);
    if (!d)
        return 0;
    if (!rogue_inv_tags_can_salvage(def_index))
        return 0;
    int rarity = d->rarity;
    int inst = find_instance_for_def(def_index);
    int produced = 0;
    if (inst >= 0)
    {
        produced = rogue_salvage_item_instance(inst, rogue_inventory_add);
    }
    if (produced <= 0)
    {
        produced = rogue_salvage_item(def_index, rarity, rogue_inventory_add);
    }
    if (produced > 0)
    {
        /* Reflect removal in unified entries to keep persistence/tests consistent. */
        if (rogue_inventory_register_remove(def_index, 1) != 0)
        {
            /* Fallback to legacy consume for older code paths (should not normally happen). */
            rogue_inventory_consume(def_index, 1);
        }
    }
    return produced;
}

/**
 * @brief Drops one instance of an item from the inventory.
 *
 * @param def_index The item definition index.
 * @return Instance index on success, negative on failure.
 *
 * This function spawns an item instance at the player's position and removes it from inventory.
 */
int rogue_inventory_ui_drop_one(int def_index)
{
    /* Use unified entries quantity (tests and persistence rely on this path). */
    if ((int) rogue_inventory_quantity(def_index) <= 0)
        return -2;
    if (!rogue_inv_tags_can_salvage(def_index))
        return -3;
    const RogueItemDef* d = rogue_item_def_at(def_index);
    if (!d)
        return -1;
    float x = g_app.player.base.pos.x;
    float y = g_app.player.base.pos.y;
    int inst = rogue_items_spawn(def_index, 1, x, y);
    if (inst >= 0)
    {
        /* Reflect removal in unified entries; legacy consume as safety fallback. */
        if (rogue_inventory_register_remove(def_index, 1) != 0)
        {
            rogue_inventory_consume(def_index, 1);
        }
    }
    return inst;
}
