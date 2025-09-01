#include "inventory_tag_rules.h"
#include "../loot/loot_item_defs.h" /* for rogue_item_def_at */
#include "inventory_tags.h"
#include <stdlib.h>
#include <string.h>

/**
 * @file inventory_tag_rules.c
 * @brief Implements automatic tagging rules for inventory items based on rarity, category, and
 * other criteria.
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 *
 * This file manages a set of rules that automatically assign tags and accent colors to inventory
 * entries when items are added. It supports persistence for saving/loading rules and integrates
 * with item definitions.
 */

/** @brief Array of tag rules, up to ROGUE_INV_TAG_RULE_MAX entries. */
static RogueInvTagRule g_rules[ROGUE_INV_TAG_RULE_MAX];
/** @brief Current number of active tag rules. */
static int g_rule_count = 0;
/** @brief Cached accent colors per item definition index. */
static uint32_t* g_rule_accent_colors = NULL; /* per def index cached accent */

/**
 * @brief Ensures the accent color cache is allocated.
 *
 * @return 0 on success, -1 on allocation failure.
 *
 * This function allocates the g_rule_accent_colors array if not already present,
 * using calloc to initialize to zero.
 */
static int ensure_accent_cache(void)
{
    if (!g_rule_accent_colors)
    {
        g_rule_accent_colors = (uint32_t*) calloc(ROGUE_ITEM_DEF_CAP, sizeof(uint32_t));
        if (!g_rule_accent_colors)
            return -1;
    }
    return 0;
}

/**
 * @brief Adds a new tag rule to the system.
 *
 * @param min_rarity Minimum rarity level for the rule to apply.
 * @param max_rarity Maximum rarity level (0 for open upper bound).
 * @param category_mask Bitmask of item categories to match.
 * @param tag The tag string to assign.
 * @param accent_color_rgba Accent color in RGBA format.
 * @return 0 on success, -1 on failure (e.g., max rules reached, invalid tag).
 *
 * This function creates a new rule with the specified criteria and adds it to the rules array.
 */
int rogue_inv_tag_rules_add(uint8_t min_rarity, uint8_t max_rarity, uint32_t category_mask,
                            const char* tag, uint32_t accent_color_rgba)
{
    if (g_rule_count >= ROGUE_INV_TAG_RULE_MAX)
        return -1;
    if (!tag || !*tag)
        return -1;
    if (max_rarity == 0)
        max_rarity = 0xFF; /* allow caller to pass 0 for open upper bound */
    RogueInvTagRule* r = &g_rules[g_rule_count];
    r->min_rarity = min_rarity;
    r->max_rarity = max_rarity;
    r->category_mask = category_mask;
    r->accent_color_rgba = accent_color_rgba;
    memset(r->tag, 0, sizeof r->tag);
    size_t len = strlen(tag);
    if (len >= sizeof(r->tag))
        len = sizeof(r->tag) - 1;
    memcpy(r->tag, tag, len);
    g_rule_count++;
    return 0;
}

/**
 * @brief Retrieves the current number of tag rules.
 *
 * @return The number of rules.
 */
int rogue_inv_tag_rules_count(void) { return g_rule_count; }
/**
 * @brief Retrieves a tag rule by index.
 *
 * @param index The index of the rule to retrieve.
 * @return Pointer to the rule, or NULL if index is invalid.
 */
const RogueInvTagRule* rogue_inv_tag_rules_get(int index)
{
    if (index < 0 || index >= g_rule_count)
        return NULL;
    return &g_rules[index];
}
/**
 * @brief Clears all tag rules.
 *
 * This function resets the rule count to zero, effectively removing all rules.
 */
void rogue_inv_tag_rules_clear(void) { g_rule_count = 0; }

/**
 * @brief Applies all matching tag rules to a single item definition.
 *
 * @param def_index The item definition index to apply rules to.
 *
 * This function evaluates each rule against the item's properties and applies tags
 * and accent colors if the criteria match. It handles cases where item definitions
 * are not loaded by applying universal rules.
 */
static void apply_rules_one(int def_index)
{
    const RogueItemDef* d = rogue_item_def_at(def_index);
    ensure_accent_cache();
    for (int i = 0; i < g_rule_count; i++)
    {
        const RogueInvTagRule* r = &g_rules[i];
        int match = 0;
        if (d)
        {
            if (d->rarity < r->min_rarity)
                continue;
            if (r->max_rarity != 0xFF && d->rarity > r->max_rarity)
                continue;
            if (r->category_mask && ((r->category_mask & (1u << d->category)) == 0))
                continue;
            match = 1;
        }
        else
        {
            /* If item defs aren't loaded, still apply rules that are universally applicable
             * (no category mask and open rarity range). This preserves determinism in tests
             * that expect broad rules to tag entries regardless of def metadata. */
            if (r->category_mask == 0 && r->min_rarity == 0 && r->max_rarity == 0xFF)
                match = 1;
        }
        if (!match)
            continue;
        if (r->tag[0])
            rogue_inv_tags_add_tag(def_index, r->tag);
        if (r->accent_color_rgba && g_rule_accent_colors)
        {
            if (g_rule_accent_colors[def_index] == 0)
                g_rule_accent_colors[def_index] = r->accent_color_rgba;
        }
    }
}

/**
 * @brief Applies tag rules to a specific item definition.
 *
 * @param def_index The item definition index.
 *
 * This function is a public interface to apply all matching rules to the given item.
 */
void rogue_inv_tag_rules_apply_def(int def_index) { apply_rules_one(def_index); }

/**
 * @brief Retrieves the cached accent color for an item definition.
 *
 * @param def_index The item definition index.
 * @return The accent color in RGBA format, or 0 if not set or invalid.
 *
 * This function returns the accent color assigned by the first matching rule.
 */
uint32_t rogue_inv_tag_rules_accent_color(int def_index)
{
    if (!g_rule_accent_colors)
        return 0;
    if (def_index < 0 || def_index >= ROGUE_ITEM_DEF_CAP)
        return 0;
    return g_rule_accent_colors[def_index];
}

/* Persistence format (component id TBD):
 * uint16 rule_count
 * For each rule:
 *  uint8 min_rarity
 *  uint8 max_rarity
 *  uint32 category_mask
 *  uint32 accent_color_rgba
 *  uint8 tag_len
 *  bytes tag (tag_len, no null)
 */
/**
 * @brief Writes the tag rules to a file for persistence.
 *
 * @param f The file pointer to write to.
 * @return 0 on success, -1 on failure.
 *
 * This function serializes the current rules into the specified file using the defined persistence
 * format.
 */
int rogue_inv_tag_rules_write(FILE* f)
{
    if (!f)
        return -1;
    uint16_t rc = (uint16_t) g_rule_count;
    if (fwrite(&rc, sizeof(rc), 1, f) != 1)
        return -1;
    for (int i = 0; i < g_rule_count; i++)
    {
        const RogueInvTagRule* r = &g_rules[i];
        uint8_t tag_len = (uint8_t) strlen(r->tag);
        if (fwrite(&r->min_rarity, 1, 1, f) != 1)
            return -1;
        if (fwrite(&r->max_rarity, 1, 1, f) != 1)
            return -1;
        if (fwrite(&r->category_mask, sizeof(r->category_mask), 1, f) != 1)
            return -1;
        if (fwrite(&r->accent_color_rgba, sizeof(r->accent_color_rgba), 1, f) != 1)
            return -1;
        if (fwrite(&tag_len, 1, 1, f) != 1)
            return -1;
        if (tag_len > 0)
        {
            if (fwrite(r->tag, 1, tag_len, f) != tag_len)
                return -1;
        }
    }
    return 0;
}

/**
 * @brief Reads tag rules from a file for persistence.
 *
 * @param f The file pointer to read from.
 * @param size The size of the data (unused).
 * @return 0 on success, -1 on failure.
 *
 * This function deserializes rules from the file and clears/recomputes the accent color cache.
 */
int rogue_inv_tag_rules_read(FILE* f, size_t size)
{
    (void) size;
    if (!f)
        return -1;
    g_rule_count = 0;
    uint16_t rc = 0;
    if (fread(&rc, sizeof(rc), 1, f) != 1)
        return -1;
    if (rc > ROGUE_INV_TAG_RULE_MAX)
        rc = ROGUE_INV_TAG_RULE_MAX;
    for (int i = 0; i < (int) rc; i++)
    {
        RogueInvTagRule r;
        memset(&r, 0, sizeof r);
        uint8_t tag_len = 0;
        if (fread(&r.min_rarity, 1, 1, f) != 1)
            return -1;
        if (fread(&r.max_rarity, 1, 1, f) != 1)
            return -1;
        if (fread(&r.category_mask, sizeof(r.category_mask), 1, f) != 1)
            return -1;
        if (fread(&r.accent_color_rgba, sizeof(r.accent_color_rgba), 1, f) != 1)
            return -1;
        if (fread(&tag_len, 1, 1, f) != 1)
            return -1;
        if (tag_len >= sizeof(r.tag))
            tag_len = sizeof(r.tag) - 1;
        if (tag_len > 0)
        {
            if (fread(r.tag, 1, tag_len, f) != tag_len)
                return -1;
            r.tag[tag_len] = '\0';
        }
        g_rules[g_rule_count++] = r;
    }
    /* accent colors need recompute if cache present */
    if (g_rule_accent_colors)
    {
        memset(g_rule_accent_colors, 0, ROGUE_ITEM_DEF_CAP * sizeof(uint32_t));
    }
    return 0;
}
