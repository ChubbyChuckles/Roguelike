#include "core/loot/loot_stats.h"
#include <string.h>

static int g_rarity_ring[ROGUE_LOOT_STATS_CAP];
static unsigned short g_head = 0; /* next write */
static unsigned short g_size = 0; /* current valid size */
static int g_counts[5];

void rogue_loot_stats_reset(void)
{
    g_head = 0;
    g_size = 0;
    memset(g_counts, 0, sizeof g_counts);
}

void rogue_loot_stats_record_rarity(int rarity)
{
    if (rarity < 0 || rarity > 4)
        return;
    if (g_size < ROGUE_LOOT_STATS_CAP)
    {
        g_rarity_ring[g_head] = rarity;
        g_counts[rarity]++;
        g_head = (g_head + 1) % ROGUE_LOOT_STATS_CAP;
        g_size++;
    }
    else
    {
        int old_index = g_head; /* overwrite oldest */
        int old_r = g_rarity_ring[old_index];
        if (old_r >= 0 && old_r <= 4)
            g_counts[old_r]--;
        g_rarity_ring[old_index] = rarity;
        g_counts[rarity]++;
        g_head = (g_head + 1) % ROGUE_LOOT_STATS_CAP;
    }
}

int rogue_loot_stats_count(int rarity)
{
    if (rarity < 0 || rarity > 4)
        return 0;
    return g_counts[rarity];
}
void rogue_loot_stats_snapshot(int out[5])
{
    if (!out)
        return;
    for (int i = 0; i < 5; i++)
        out[i] = g_counts[i];
}
