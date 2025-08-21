#include "core/loot/loot_dynamic_weights.h"
#include <string.h>

static float g_factors[5];

void rogue_loot_dyn_reset(void)
{
    for (int i = 0; i < 5; i++)
        g_factors[i] = 1.0f;
}
void rogue_loot_dyn_set_factor(int rarity, float factor)
{
    if (rarity < 0 || rarity > 4)
        return;
    if (factor <= 0.0f)
        factor = 0.0001f;
    g_factors[rarity] = factor;
}
float rogue_loot_dyn_get_factor(int rarity)
{
    if (rarity < 0 || rarity > 4)
        return 1.0f;
    return g_factors[rarity];
}
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
