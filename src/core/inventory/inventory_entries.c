#include "inventory_entries.h"
#include "../persistence/save_manager.h" /* mark component dirty for incremental saves */
#include "inventory_query.h"             /* Phase 4.6 cache invalidation */
#include "inventory_tag_rules.h"         /* Phase 3.3 auto-tag rules */
#include <stdlib.h>
#include <string.h>

/**
 * @file inventory_entries.c
 * @brief Manages the core inventory entries for the roguelike game, handling item quantities,
 * labels, and dirty tracking for persistence.
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 *
 * This file implements the inventory entry system, which tracks unique item definitions and their
 * quantities. It supports operations like adding/removing items, capacity management, and
 * integration with save persistence. Key features include dirty tracking for incremental saves and
 * label metadata for categorization.
 */

/**
 * @brief Represents a single inventory entry for a unique item definition.
 *
 * This struct holds the essential data for an item in the inventory: its definition index,
 * quantity, and associated labels for metadata.
 */
typedef struct InvEntry
{
    int def_index;   /**< @brief The item definition index, uniquely identifying the item type. */
    uint64_t qty;    /**< @brief The quantity of this item in the inventory. */
    unsigned labels; /**< @brief Metadata labels for categorization (Phase 1.3). */
} InvEntry;          /* labels metadata (Phase 1.3) */

/** @brief Dynamic array of inventory entries, storing all unique items. */
static InvEntry* g_entries = NULL;
/** @brief Current number of entries in the inventory. */
static unsigned g_entry_count = 0;
/** @brief Soft capacity limit for unique entries (Phase 1.2). */
static unsigned g_entry_cap_soft = 1024; /* default soft cap (Phase 1.2) */
/** @brief Allocated capacity for the entries array. */
static unsigned g_entry_capacity = 0; /* allocated */
/** @brief Handler function for capacity overflow mitigation (Phase 1.7). */
static RogueInventoryCapHandler g_cap_handler = NULL; /* Phase 1.7 */
/* Dirty tracking (Phase 1.6): when an entry's quantity changes or is created/removed we record its
 * def_index in a bitmap-like sparse list (dedup). */
/** @brief Dynamic array of dirty item definition indices for incremental saves. */
static int* g_dirty_indices = NULL; /* dynamic array of def_index */
/** @brief Number of dirty indices currently tracked. */
static unsigned g_dirty_count = 0;
/** @brief Allocated capacity for the dirty indices array. */
static unsigned g_dirty_cap = 0;

/**
 * @brief Clears the dirty tracking state, freeing allocated memory.
 *
 * This function resets the dirty indices array, releasing any allocated memory
 * and resetting counters to zero.
 */
static void dirty_clear(void)
{
    if (g_dirty_indices)
    {
        free(g_dirty_indices);
        g_dirty_indices = NULL;
    }
    g_dirty_count = 0;
    g_dirty_cap = 0;
}
/**
 * @brief Marks an item definition index as dirty for incremental saves.
 *
 * @param def_index The item definition index to mark as dirty.
 *
 * This function adds the given def_index to the dirty list if not already present,
 * ensuring deduplication. It also marks the inventory component as dirty for persistence.
 * @note Assumes def_index >= 0; negative values are ignored.
 */
static void dirty_mark(int def_index)
{
    if (def_index < 0)
        return; /* dedup linear (entry count small typical); could optimize with hash later */
    for (unsigned i = 0; i < g_dirty_count; i++)
        if (g_dirty_indices[i] == def_index)
            return;
    if (g_dirty_count == g_dirty_cap)
    {
        unsigned nc = g_dirty_cap ? g_dirty_cap * 2 : 64;
        int* nr = (int*) realloc(g_dirty_indices, sizeof(int) * nc);
        if (!nr)
            return;
        g_dirty_indices = nr;
        g_dirty_cap = nc;
    }
    g_dirty_indices[g_dirty_count++] = def_index;
    /* Mark save component dirty (Phase 1.6 persistence integration) */
    rogue_save_mark_component_dirty(ROGUE_SAVE_COMP_INV_ENTRIES);
}

/**
 * @brief Initializes the inventory entries system.
 *
 * @return 0 on success, or a negative value on failure.
 *
 * This function resets the inventory to an empty state, freeing any allocated memory
 * and clearing all global variables to their default values.
 */
int rogue_inventory_entries_init(void)
{
    free(g_entries);
    g_entries = NULL;
    g_entry_count = 0;
    g_entry_capacity = 0;
    g_entry_cap_soft = 1024;
    g_cap_handler = NULL;
    dirty_clear();
    return 0;
}
/**
 * @brief Sets the soft capacity limit for unique inventory entries.
 *
 * @param cap The new soft capacity limit.
 * @return 0 on success.
 *
 * This function updates the soft cap for the number of unique items allowed in the inventory.
 */
int rogue_inventory_set_unique_cap(unsigned cap)
{
    g_entry_cap_soft = cap;
    return 0;
}
/**
 * @brief Retrieves the current soft capacity limit for unique inventory entries.
 *
 * @return The soft capacity limit.
 */
unsigned rogue_inventory_unique_cap(void) { return g_entry_cap_soft; }
/**
 * @brief Retrieves the current number of unique inventory entries.
 *
 * @return The number of unique entries.
 */
unsigned rogue_inventory_unique_count(void) { return g_entry_count; }

/**
 * @brief Ensures the entries array has sufficient capacity.
 *
 * @param need The minimum capacity required.
 * @return 0 on success, -1 on failure (allocation error).
 *
 * This function expands the g_entries array if necessary to accommodate at least 'need' entries,
 * doubling the capacity or setting it to 'need' if larger, up to ROGUE_INV_MAX_ENTRIES.
 */
static int ensure_capacity(unsigned need)
{
    if (need <= g_entry_capacity)
        return 0;
    unsigned new_cap = g_entry_capacity ? g_entry_capacity * 2 : 64;
    if (new_cap < need)
        new_cap = need;
    if (new_cap > ROGUE_INV_MAX_ENTRIES)
        new_cap = ROGUE_INV_MAX_ENTRIES;
    InvEntry* nr = (InvEntry*) realloc(g_entries, sizeof(InvEntry) * new_cap);
    if (!nr)
        return -1;
    g_entries = nr;
    g_entry_capacity = new_cap;
    return 0;
}

/**
 * @brief Finds the index of an entry by its definition index.
 *
 * @param def_index The item definition index to search for.
 * @return The index of the entry if found, -1 otherwise.
 *
 * This function performs a linear search through the entries array to locate the entry
 * with the specified def_index.
 */
static int find_entry(int def_index)
{
    for (unsigned i = 0; i < g_entry_count; i++)
        if (g_entries[i].def_index == def_index)
            return (int) i;
    return -1;
}

/**
 * @brief Retrieves the quantity of a specific item in the inventory.
 *
 * @param def_index The item definition index.
 * @return The quantity of the item, or 0 if not present.
 */
uint64_t rogue_inventory_quantity(int def_index)
{
    int idx = find_entry(def_index);
    return (idx >= 0) ? g_entries[idx].qty : 0;
}

/**
 * @brief Calculates the pressure on the inventory capacity.
 *
 * @return A value between 0.0 and 1.0 indicating capacity usage.
 *
 * This function returns the ratio of current unique entries to the soft capacity limit,
 * or 0.0 if the limit is 0 or there are no entries.
 */
double rogue_inventory_entry_pressure(void)
{
    if (g_entry_cap_soft == 0 || g_entry_count == 0)
        return 0.0;
    if (g_entry_count >= g_entry_cap_soft)
        return 1.0;
    return (double) g_entry_count / (double) g_entry_cap_soft;
}

/**
 * @brief Checks if the inventory can accept a quantity of an item.
 *
 * @param def_index The item definition index.
 * @param add_qty The quantity to add.
 * @return 0 if acceptable, or an error code (e.g., ROGUE_INV_ERR_UNIQUE_CAP,
 * ROGUE_INV_ERR_OVERFLOW).
 *
 * This function verifies whether adding add_qty of the item would exceed capacity limits
 * or cause overflow, considering both unique entry limits and quantity limits.
 */
int rogue_inventory_can_accept(int def_index, uint64_t add_qty)
{
    if (add_qty == 0)
        return 0;
    int idx = find_entry(def_index);
    if (idx < 0)
    { /* would create new distinct entry */
        if (g_entry_count >= ROGUE_INV_MAX_ENTRIES)
            return ROGUE_INV_ERR_UNIQUE_CAP;
        if (g_entry_count >= g_entry_cap_soft)
            return ROGUE_INV_ERR_UNIQUE_CAP;
    }
    uint64_t cur = (idx >= 0) ? g_entries[idx].qty : 0;
    if (add_qty > UINT64_MAX - cur)
        return ROGUE_INV_ERR_OVERFLOW;
    return 0;
}

/**
 * @brief Registers the pickup of an item into the inventory.
 *
 * @param def_index The item definition index.
 * @param add_qty The quantity to add.
 * @return 0 on success, or an error code if the pickup cannot be accepted.
 *
 * This function attempts to add the specified quantity of the item to the inventory.
 * If capacity is exceeded, it may invoke the capacity handler for mitigation.
 * Marks the entry as dirty and invalidates caches on success.
 */
int rogue_inventory_register_pickup(int def_index, uint64_t add_qty)
{
    if (add_qty == 0)
        return 0;
    int rc = rogue_inventory_can_accept(def_index, add_qty);
    if (rc == ROGUE_INV_ERR_UNIQUE_CAP && g_cap_handler)
    { /* attempt mitigation */
        int h = g_cap_handler(def_index, add_qty);
        if (h == 0)
        {
            rc = rogue_inventory_can_accept(def_index, add_qty);
        }
    }
    if (rc != 0)
        return rc;
    int idx = find_entry(def_index);
    if (idx < 0)
    {
        if (ensure_capacity(g_entry_count + 1) != 0)
            return -1;
        g_entries[g_entry_count++] = (InvEntry){def_index, add_qty, 0u};
        dirty_mark(def_index);
        rogue_inv_tag_rules_apply_def(def_index);
        rogue_inventory_query_cache_invalidate_all();
        return 0;
    }
    uint64_t before = g_entries[idx].qty;
    g_entries[idx].qty += add_qty;
    if (g_entries[idx].qty != before)
    {
        dirty_mark(def_index);
        rogue_inventory_query_cache_invalidate_all();
    }
    return 0;
}

/**
 * @brief Registers the removal of an item from the inventory.
 *
 * @param def_index The item definition index.
 * @param remove_qty The quantity to remove.
 * @return 0 on success, -1 if the item is not present or insufficient quantity.
 *
 * This function attempts to remove the specified quantity of the item.
 * If the quantity reaches zero, the entry is removed from the array.
 * Marks the entry as dirty and invalidates caches on success.
 */
int rogue_inventory_register_remove(int def_index, uint64_t remove_qty)
{
    if (remove_qty == 0)
        return 0;
    int idx = find_entry(def_index);
    if (idx < 0)
        return -1;
    if (remove_qty > g_entries[idx].qty)
        return -1;
    uint64_t before = g_entries[idx].qty;
    g_entries[idx].qty -= remove_qty;
    if (g_entries[idx].qty != before)
    {
        dirty_mark(def_index);
        rogue_inventory_query_cache_invalidate_all();
    }
    if (g_entries[idx].qty == 0)
    {
        unsigned last = g_entry_count - 1;
        if (idx != (int) last)
            g_entries[idx] = g_entries[last];
        g_entry_count--;
        dirty_mark(def_index);
        rogue_inventory_query_cache_invalidate_all();
    }
    return 0;
}

/**
 * @brief Sets the labels for a specific inventory entry.
 *
 * @param def_index The item definition index.
 * @param labels The new labels value.
 * @return 0 on success, -1 if the entry is not found.
 *
 * This function updates the metadata labels for the specified item entry.
 */
int rogue_inventory_entry_set_labels(int def_index, unsigned labels)
{
    int idx = find_entry(def_index);
    if (idx < 0)
        return -1;
    g_entries[idx].labels = labels;
    return 0;
}
/**
 * @brief Retrieves the labels for a specific inventory entry.
 *
 * @param def_index The item definition index.
 * @return The labels value, or 0 if the entry is not found.
 */
unsigned rogue_inventory_entry_labels(int def_index)
{
    int idx = find_entry(def_index);
    if (idx < 0)
        return 0;
    return g_entries[idx].labels;
}
/**
 * @brief Sets the capacity overflow handler function.
 *
 * @param fn The handler function pointer.
 * @return 0 on success.
 *
 * This function assigns a callback to handle cases where adding an item would exceed capacity.
 */
int rogue_inventory_set_cap_handler(RogueInventoryCapHandler fn)
{
    g_cap_handler = fn;
    return 0;
}
/**
 * @brief Retrieves pairs of dirty item indices and their quantities.
 *
 * @param out_def_indices Array to store definition indices.
 * @param out_quantities Array to store corresponding quantities.
 * @param cap Maximum number of pairs to retrieve.
 * @return The number of pairs retrieved.
 *
 * This function enumerates the dirty entries since the last call, storing up to 'cap' pairs
 * in the provided arrays. If arrays are NULL, it resets the dirty state.
 */
unsigned rogue_inventory_entries_dirty_pairs(int* out_def_indices, uint64_t* out_quantities,
                                             unsigned cap)
{
    if (!out_def_indices || !out_quantities)
    { /* reset baseline */
        dirty_clear();
        return 0;
    }
    unsigned n = (g_dirty_count < cap) ? g_dirty_count : cap;
    for (unsigned i = 0; i < n; i++)
    {
        int def_index = g_dirty_indices[i];
        out_def_indices[i] = def_index; /* find quantity (may be absent -> 0) */
        int idx = find_entry(def_index);
        out_quantities[i] = (idx >= 0) ? g_entries[idx].qty : 0ull;
    }
    /* After enumeration we clear to treat current state as baseline. */ dirty_clear();
    return n;
}
/**
 * @brief Clears the dirty tracking state.
 *
 * This function resets the dirty indices, effectively clearing the list of changed entries.
 */
void rogue_inventory_entries_clear_dirty(void) { dirty_clear(); }
