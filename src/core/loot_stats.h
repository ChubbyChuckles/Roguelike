/* Rolling window statistics for loot rarities (6.3) */
#ifndef ROGUE_LOOT_STATS_H
#define ROGUE_LOOT_STATS_H

#ifndef ROGUE_LOOT_STATS_CAP
#define ROGUE_LOOT_STATS_CAP 128
#endif

void rogue_loot_stats_reset(void);
/* Record a rarity (0-4). Invalid rarities ignored. */
void rogue_loot_stats_record_rarity(int rarity);
/* Return count of rarity occurrences currently in window. */
int rogue_loot_stats_count(int rarity);
/* Copy all rarity counts into out[5]. */
void rogue_loot_stats_snapshot(int out[5]);

#endif
