/* Dynamic rarity weighting adjustments (5.4) */
#ifndef ROGUE_LOOT_DYNAMIC_WEIGHTS_H
#define ROGUE_LOOT_DYNAMIC_WEIGHTS_H

/* Base factors default =1.0 (no change). Order: common..legendary */
void rogue_loot_dyn_reset(void);
/* Apply multiplicative factor to rarity (0-4). factor<=0 treated as minimal epsilon. */
void rogue_loot_dyn_set_factor(int rarity, float factor);
float rogue_loot_dyn_get_factor(int rarity);
/* Adjust raw rarity weights in-place (length>=5). */
void rogue_loot_dyn_apply(int weights[5]);

#endif
