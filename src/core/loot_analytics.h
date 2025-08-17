/* Loot analytics & telemetry (Phase 18.1/18.2) */
#ifndef ROGUE_LOOT_ANALYTICS_H
#define ROGUE_LOOT_ANALYTICS_H
#ifdef __cplusplus
extern "C" {
#endif

#define ROGUE_LOOT_ANALYTICS_RING_CAP 512

typedef struct RogueLootDropEvent {
    int def_index;
    int rarity; /* RogueItemRarity */
    double t_seconds;
} RogueLootDropEvent;

void rogue_loot_analytics_reset(void);
void rogue_loot_analytics_record(int def_index, int rarity, double t_seconds);
int  rogue_loot_analytics_count(void);
int  rogue_loot_analytics_recent(int max, RogueLootDropEvent* out); /* returns number copied (most recent first) */
void rogue_loot_analytics_rarity_counts(int out_counts[5]); /* fills rarity counts (assumes 5 tiers) */
int  rogue_loot_analytics_export_json(char* buf, int cap); /* returns 0 on success */

#ifdef __cplusplus
}
#endif
#endif
