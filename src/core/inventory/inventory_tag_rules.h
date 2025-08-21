#ifndef ROGUE_INVENTORY_TAG_RULES_H
#define ROGUE_INVENTORY_TAG_RULES_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h> /* FILE */
#ifdef __cplusplus
extern "C"
{
#endif

    /* Phase 3.3 / 3.4 / 3.5: Auto-tag rules + rule expression persistence + accent color mapping.
     * A rule assigns a tag (and optional accent color) to any item definition whose static
     * properties satisfy the rule predicate (currently rarity and category mask). Additional
     * expression operators can be appended in later phases (affix weight, quality, etc.).
     * On pickup (inventory register), matching rules are evaluated in declaration order.
     * Tag insertion: all matching rule tags are added (deduplicated). Accent color precedence:
     * first matching rule that specifies a non-zero color sets the per-definition accent color.
     * Determinism: rules array is linear; iteration order is stable; no randomness.
     */

#define ROGUE_INV_TAG_RULE_MAX 32

    typedef struct RogueInvTagRule
    {
        uint8_t min_rarity;         /* inclusive */
        uint8_t max_rarity;         /* inclusive (0xFF => no upper bound) */
        uint32_t category_mask;     /* bit per RogueItemCategory (1<<cat); 0 => any */
        uint32_t accent_color_rgba; /* 0 => no accent color contributed */
        char tag[24];               /* short tag string (<=23 chars) */
    } RogueInvTagRule;

    int rogue_inv_tag_rules_add(uint8_t min_rarity, uint8_t max_rarity, uint32_t category_mask,
                                const char* tag,
                                uint32_t accent_color_rgba); /* returns 0 or -1 full/invalid */
    int rogue_inv_tag_rules_count(void);
    const RogueInvTagRule* rogue_inv_tag_rules_get(int index);
    void rogue_inv_tag_rules_clear(void);

    /* Applies rules for a definition index (looks up item definition for rarity/category). */
    void rogue_inv_tag_rules_apply_def(int def_index);

    /* Returns accent color (RGBA 0xRRGGBBAA) for def or 0 if none. */
    uint32_t rogue_inv_tag_rules_accent_color(int def_index);

    /* Persistence (component id ROGUE_SAVE_COMP_INV_TAG_RULES) */
    int rogue_inv_tag_rules_write(FILE* f);             /* internal */
    int rogue_inv_tag_rules_read(FILE* f, size_t size); /* internal */

#ifdef __cplusplus
}
#endif
#endif
