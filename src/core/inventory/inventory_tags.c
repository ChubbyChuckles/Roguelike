#include "inventory_tags.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file inventory_tags.c
 * @brief Manages tags and flags for inventory item definitions, supporting addition, removal, and
 * serialization.
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 *
 * This file implements a tag system for inventory items, allowing metadata like flags and string
 * tags to be associated with item definitions. It supports persistence through serialization.
 */

/**
 * @brief Represents the tags and flags for a single item definition.
 *
 * This struct holds metadata for an inventory item, including flags and a list of string tags.
 */
typedef struct InvTagRec
{
    unsigned flags;          /**< @brief Bitmask of flags (e.g., locked, favorite). */
    unsigned char tag_count; /**< @brief Number of tags currently assigned. */
    char tags[ROGUE_INV_TAG_MAX_TAGS_PER_DEF]
             [ROGUE_INV_TAG_SHORT_LEN]; /**< @brief Array of tag strings. */
} InvTagRec;
static InvTagRec* g_tag_table = NULL; /* allocated on demand size ROGUE_INV_TAG_MAX_DEFS */

/** @brief Global table of tag records for all item definitions. */

/**
 * @brief Initializes the tag system.
 *
 * @return 0 on success, -1 on allocation failure.
 *
 * This function allocates or resets the global tag table for all item definitions.
 */
int rogue_inv_tags_init(void)
{
    if (!g_tag_table)
    {
        g_tag_table = (InvTagRec*) calloc(ROGUE_INV_TAG_MAX_DEFS, sizeof(InvTagRec));
        if (!g_tag_table)
            return -1;
    }
    else
    {
        memset(g_tag_table, 0, sizeof(InvTagRec) * ROGUE_INV_TAG_MAX_DEFS);
    }
    return 0;
}
/**
 * @brief Validates if a definition index is within bounds.
 *
 * @param d The definition index to check.
 * @return 1 if valid, 0 otherwise.
 *
 * This function checks if the given index is non-negative and less than the maximum allowed.
 */
static int valid_def(int d) { return d >= 0 && d < ROGUE_INV_TAG_MAX_DEFS; }
/**
 * @brief Sets the flags for a specific item definition.
 *
 * @param def_index The item definition index.
 * @param flags The flags to set.
 * @return 0 on success, -1 if invalid index or uninitialized table.
 */
int rogue_inv_tags_set_flags(int def_index, unsigned flags)
{
    if (!valid_def(def_index) || !g_tag_table)
        return -1;
    g_tag_table[def_index].flags = flags;
    return 0;
}
/**
 * @brief Retrieves the flags for a specific item definition.
 *
 * @param def_index The item definition index.
 * @return The flags, or 0 if invalid index or uninitialized table.
 */
unsigned rogue_inv_tags_get_flags(int def_index)
{
    if (!valid_def(def_index) || !g_tag_table)
        return 0;
    return g_tag_table[def_index].flags;
}
/**
 * @brief Finds the index of a tag in the list for a definition.
 *
 * @param def_index The item definition index.
 * @param tag The tag string to search for.
 * @return The index of the tag, or -1 if not found or invalid inputs.
 *
 * This function performs a linear search for the tag in the definition's tag list.
 */
static int find_tag(int def_index, const char* tag)
{
    if (!valid_def(def_index) || !g_tag_table || !tag)
        return -1;
    InvTagRec* r = &g_tag_table[def_index];
    for (int i = 0; i < r->tag_count; i++)
    {
        if (strncmp(r->tags[i], tag, ROGUE_INV_TAG_SHORT_LEN) == 0)
            return i;
    }
    return -1;
}
/**
 * @brief Adds a tag to a specific item definition.
 *
 * @param def_index The item definition index.
 * @param tag The tag string to add.
 * @return 0 on success, -1 if invalid inputs, already exists, or capacity exceeded.
 *
 * This function adds the tag if it doesn't already exist and there's space.
 */
int rogue_inv_tags_add_tag(int def_index, const char* tag)
{
    if (!valid_def(def_index) || !g_tag_table || !tag || !*tag)
        return -1;
    InvTagRec* r = &g_tag_table[def_index];
    if (r->tag_count >= ROGUE_INV_TAG_MAX_TAGS_PER_DEF)
        return -1;
    if (find_tag(def_index, tag) >= 0)
        return 0;
    size_t len = strlen(tag);
    if (len >= ROGUE_INV_TAG_SHORT_LEN)
        len = ROGUE_INV_TAG_SHORT_LEN - 1;
    memset(r->tags[r->tag_count], 0, ROGUE_INV_TAG_SHORT_LEN);
    memcpy(r->tags[r->tag_count], tag, len);
    r->tag_count++;
    return 0;
}
/**
 * @brief Removes a tag from a specific item definition.
 *
 * @param def_index The item definition index.
 * @param tag The tag string to remove.
 * @return 0 on success, -1 if tag not found.
 *
 * This function removes the tag if it exists, swapping with the last tag for efficiency.
 */
int rogue_inv_tags_remove_tag(int def_index, const char* tag)
{
    int idx = find_tag(def_index, tag);
    if (idx < 0)
        return -1;
    InvTagRec* r = &g_tag_table[def_index];
    int last = r->tag_count - 1;
    if (idx != last)
    {
        memcpy(r->tags[idx], r->tags[last], ROGUE_INV_TAG_SHORT_LEN);
    }
    r->tag_count--;
    return 0;
}
/**
 * @brief Lists the tags for a specific item definition.
 *
 * @param def_index The item definition index.
 * @param out_tags Array to store tag pointers.
 * @param cap Maximum number of tags to copy.
 * @return The total number of tags for the definition.
 *
 * This function copies up to 'cap' tag pointers into out_tags and returns the total count.
 */
int rogue_inv_tags_list(int def_index, const char** out_tags, int cap)
{
    if (!valid_def(def_index) || !g_tag_table)
        return 0;
    InvTagRec* r = &g_tag_table[def_index];
    int total = r->tag_count;
    if (out_tags && cap > 0)
    {
        int n = total;
        if (n > cap)
            n = cap;
        for (int i = 0; i < n; i++)
        {
            out_tags[i] = r->tags[i];
        }
    }
    return total;
}
/**
 * @brief Checks if a specific item definition has a given tag.
 *
 * @param def_index The item definition index.
 * @param tag The tag string to check.
 * @return 1 if the tag exists, 0 otherwise.
 */
int rogue_inv_tags_has(int def_index, const char* tag) { return find_tag(def_index, tag) >= 0; }
/**
 * @brief Checks if an item can be salvaged based on its flags.
 *
 * @param def_index The item definition index.
 * @return 1 if salvageable, 0 if locked or favorite.
 *
 * This function determines salvage eligibility by checking for locked or favorite flags.
 */
int rogue_inv_tags_can_salvage(int def_index)
{
    unsigned f = rogue_inv_tags_get_flags(def_index);
    if (f & (ROGUE_INV_FLAG_LOCKED | ROGUE_INV_FLAG_FAVORITE))
        return 0;
    return 1;
}
/**
 * @brief Serializes the tag data to a file in text format.
 *
 * @param f The file pointer to write to.
 * @return 0 on success, -1 on failure.
 *
 * This function writes tag records in a debug text format for persistence or logging.
 */
int rogue_inv_tags_serialize(FILE* f)
{
    if (!f || !g_tag_table)
        return -1;
    for (int i = 0; i < ROGUE_INV_TAG_MAX_DEFS; i++)
    {
        InvTagRec* r = &g_tag_table[i];
        if (r->flags || r->tag_count)
        {
            fprintf(f, "IT%d=%u", i, r->flags);
            for (int t = 0; t < r->tag_count; t++)
            {
                fprintf(f, ",%s", r->tags[t]);
            }
            fputc('\n', f);
        }
    }
    return 0;
}
