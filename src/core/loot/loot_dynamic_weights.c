/**
 * @file loot_dynamic_weights.c
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 *
 * This file implements dynamic adjustment factors for loot rarity weights.
 * Maintains an array of 5 float factors (one per rarity 0-4, default 1.0).
 * Provides reset to neutral, set/get for specific rarity (clamps factor >0 to 0.0001, invalid
 * returns 1.0), and apply to multiply an input weights array (clamps adjusted >=1 if original >0).
 * Used for runtime balancing of drop probabilities based on game state or commands.
 */

#include "loot_dynamic_weights.h"
#include <string.h>

static float g_factors[5];

/**
 * @brief Resets all dynamic rarity factors to neutral (1.0).
 *
 * Sets g_factors[0-4] to 1.0f.
 */
void rogue_loot_dyn_reset(void)
{
    for (int i = 0; i < 5; i++)
        g_factors[i] = 1.0f;
}
/**
 * @brief Sets the dynamic factor for a specific rarity.
 *
 * Ignores invalid rarity (0-4 only), clamps factor <=0 to 0.0001f.
 *
 * @param rarity The rarity index (0-4).
 * @param factor The new factor value (clamped >0).
 */
void rogue_loot_dyn_set_factor(int rarity, float factor)
{
    if (rarity < 0 || rarity > 4)
        return;
    if (factor <= 0.0f)
        factor = 0.0001f;
    g_factors[rarity] = factor;
}
/**
 * @brief Retrieves the dynamic factor for a specific rarity.
 *
 * Returns 1.0f for invalid rarity.
 *
 * @param rarity The rarity index (0-4).
 * @return The factor for the rarity (default 1.0 if invalid).
 */
float rogue_loot_dyn_get_factor(int rarity)
{
    if (rarity < 0 || rarity > 4)
        return 1.0f;
    return g_factors[rarity];
}
/**
 * @brief Applies dynamic factors to an array of 5 rarity weights in place.
 *
 * For each weight >0, multiplies by factor, casts to int, clamps >=1.
 *
 * @param weights Array of 5 int weights to adjust (modified in place).
 */
void rogue_loot_dyn_apply(int weights[5])
{
    if (!weights)
        return;
    for (int i = 0; i < 5; i++)
    {
        if (weights[i] > 0)
        {
            float f = g_factors[i];
            int adj = (int) (weights[i] * f);
            if (adj < 1)
                adj = 1;
            weights[i] = adj;
        }
    }
}
