/* Advanced rarity systems: spawn effects, despawn modifiers, floor & pity (5.5-5.8) */
#ifndef ROGUE_LOOT_RARITY_ADV_H
#define ROGUE_LOOT_RARITY_ADV_H

/* Reset all advanced rarity state to defaults. */
void rogue_rarity_adv_reset(void);

/* 5.5: Per-rarity spawn sound / VFX hook mapping (stored as string id). */
int rogue_rarity_set_spawn_sound(int rarity, const char* id); /* returns 0 ok */
const char* rogue_rarity_get_spawn_sound(int rarity);
/* Phase 19.1: per-rarity pickup sounds */
int rogue_rarity_set_pickup_sound(int rarity, const char* id);
const char* rogue_rarity_get_pickup_sound(int rarity);

/* 5.6: Per-rarity despawn override (milliseconds). Pass <=0 to revert to default constant. */
int rogue_rarity_set_despawn_ms(int rarity, int ms);
int rogue_rarity_get_despawn_ms(int rarity); /* returns override or default */

/* 5.7: Minimum rarity floor (e.g., progression threshold). -1 disables (default). */
void rogue_rarity_set_min_floor(int rarity_floor);
int rogue_rarity_get_min_floor(void);

/* 5.8: Simple pity system thresholds (counts of consecutive sub-epic drops). */
void rogue_rarity_pity_set_thresholds(int epic_threshold,
                                      int legendary_threshold); /* <=0 disables respective tier */
void rogue_rarity_pity_reset(void);
int rogue_rarity_pity_counter(void); /* current counter */
/* 9.4: Enable/disable acceleration and query effective thresholds after acceleration logic */
void rogue_rarity_pity_set_acceleration(int enabled);
int rogue_rarity_pity_get_effective_epic(void);
int rogue_rarity_pity_get_effective_legendary(void);

/* Internal helpers used by sampler (exposed for tests). */
int rogue_rarity_apply_floor(int rolled, int rmin, int rmax);
int rogue_rarity_apply_pity(int rolled, int rmin, int rmax);

#endif
