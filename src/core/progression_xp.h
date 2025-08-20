/* Infinite XP & Level Core (Phase 1)
 * Provides data-driven leveling curve without hard cap, catch-up multiplier, and 64-bit accumulation helpers.
 */
#ifndef ROGUE_PROGRESSION_XP_H
#define ROGUE_PROGRESSION_XP_H

#include <limits.h>

/* Returns total cumulative XP required to reach the given level (level>=1). Level 1 => 0. */
unsigned long long rogue_xp_total_required_for_level(int level);

/* Returns XP required to advance from level to level+1 (level>=1). */
unsigned int rogue_xp_to_next_for_level(int level);

/* Catch-up multiplier for players below median level. Median acts as moving target. */
float rogue_xp_catchup_multiplier(int player_level, int median_level);

/* Safe add to 64-bit XP accumulator with saturation; returns 0 ok, -1 if saturated. */
int rogue_xp_safe_add(unsigned long long* total, unsigned long long add);

#endif
