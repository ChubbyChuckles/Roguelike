#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file inventory.c
 * @brief Implements the core inventory system for managing item quantities and persistence.
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 *
 * This file provides functions to add, remove, and query item quantities in the inventory,
 * along with serialization support for saving/loading inventory state.
 */

static int g_counts[ROGUE_ITEM_DEF_CAP]; /**< @brief Array of item counts per definition index. */
static int g_distinct = 0; /**< @brief Number of distinct items with non-zero count. */

/**
 * @brief Initializes the inventory system.
 *
 * This function resets all item counts to zero and clears the distinct count.
 */
void rogue_inventory_init(void)
{
    memset(g_counts, 0, sizeof g_counts);
    g_distinct = 0;
}
/**
 * @brief Resets the inventory system.
 *
 * This function is an alias for rogue_inventory_init().
 */
void rogue_inventory_reset(void) { rogue_inventory_init(); }

/**
 * @brief Adds quantity to an item's count in the inventory.
 *
 * @param def_index The item definition index.
 * @param quantity The quantity to add.
 * @return The actual quantity added (may be less due to overflow).
 *
 * This function increases the count for the specified item, capping at INT_MAX.
 */
int rogue_inventory_add(int def_index, int quantity)
{
    if (def_index < 0 || def_index >= ROGUE_ITEM_DEF_CAP || quantity <= 0)
        return 0;
    if (g_counts[def_index] == 0)
        g_distinct++;
    long long before = g_counts[def_index];
    long long after = before + quantity;
    if (after > 2147483647)
        after = 2147483647;
    g_counts[def_index] = (int) after;
    return (int) (after - before);
}

/**
 * @brief Retrieves the count of an item in the inventory.
 *
 * @param def_index The item definition index.
 * @return The current count, or 0 if invalid index.
 */
int rogue_inventory_get_count(int def_index)
{
    if (def_index < 0 || def_index >= ROGUE_ITEM_DEF_CAP)
        return 0;
    return g_counts[def_index];
}
/**
 * @brief Gets the total number of distinct items in the inventory.
 *
 * @return The number of items with non-zero count.
 */
int rogue_inventory_total_distinct(void) { return g_distinct; }

/**
 * @brief Consumes quantity from an item's count in the inventory.
 *
 * @param def_index The item definition index.
 * @param quantity The quantity to consume.
 * @return The actual quantity consumed.
 *
 * This function decreases the count for the specified item, not going below zero.
 */
int rogue_inventory_consume(int def_index, int quantity)
{
    if (def_index < 0 || def_index >= ROGUE_ITEM_DEF_CAP || quantity <= 0)
        return 0;
    int have = g_counts[def_index];
    if (have <= 0)
        return 0;
    int remove = quantity;
    if (remove > have)
        remove = have;
    g_counts[def_index] = have - remove;
    if (g_counts[def_index] == 0)
        g_distinct--;
    return remove;
}

/**
 * @brief Serializes the inventory to a file.
 *
 * @param f The file pointer to write to.
 *
 * This function writes non-zero item counts in key-value format.
 */
void rogue_inventory_serialize(FILE* f)
{
    if (!f)
        return;
    for (int i = 0; i < ROGUE_ITEM_DEF_CAP; i++)
    {
        if (g_counts[i] > 0)
        {
            fprintf(f, "INV%d=%d\n", i, g_counts[i]);
        }
    }
}
/**
 * @brief Attempts to parse a key-value pair for inventory data.
 *
 * This function checks if the key starts with "INV" followed by an index,
 * parses the value as an integer quantity, and updates the inventory count
 * for that item. It also updates the distinct item count if necessary.
 *
 * @param key The key string to parse (expected format: "INV<index>").
 * @param val The value string to parse as an integer quantity.
 * @return 1 if the key-value pair was successfully parsed and applied, 0 otherwise.
 */
int rogue_inventory_try_parse_kv(const char* key, const char* val)
{
    if (strncmp(key, "INV", 3) != 0)
        return 0;
    int idx = atoi(key + 3);
    if (idx < 0 || idx >= ROGUE_ITEM_DEF_CAP)
        return 0;
    int q = atoi(val);
    if (q < 0)
        q = 0;
    if (g_counts[idx] == 0 && q > 0)
        g_distinct++;
    g_counts[idx] = q;
    return 1;
}
