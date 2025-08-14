/* Adaptive category weighting (9.2) */
#ifndef ROGUE_LOOT_ADAPTIVE_H
#define ROGUE_LOOT_ADAPTIVE_H

/* Initialize / reset adaptive tracking. */
void rogue_adaptive_reset(void);
/* Record an observed drop for an item definition index (category derived internally). */
void rogue_adaptive_record_item(int item_def_index);
/* Recalculate internal rarity/category adjustment factors (call periodically). */
void rogue_adaptive_recompute(void);
/* Get multiplicative weight factor for a given rarity tier (0..4). */
float rogue_adaptive_get_rarity_factor(int rarity);
/* Get multiplicative weight factor for an item category. */
float rogue_adaptive_get_category_factor(int category);

#endif
