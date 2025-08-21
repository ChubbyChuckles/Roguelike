/* Derived Stat Caching (14.3) */
#ifndef ROGUE_STAT_CACHE_H
#define ROGUE_STAT_CACHE_H

#include "entities/player.h"

typedef struct RogueStatCache
{
    /* Layered attribute model (Phase 2.1) */
    /* Layer order: base -> implicit -> unique -> set -> runeword -> affix -> passives -> buffs ->
     * total */
    int base_strength, implicit_strength, unique_strength, set_strength, runeword_strength,
        affix_strength, passive_strength, buff_strength, total_strength;
    int base_dexterity, implicit_dexterity, unique_dexterity, set_dexterity, runeword_dexterity,
        affix_dexterity, passive_dexterity, buff_dexterity, total_dexterity;
    int base_vitality, implicit_vitality, unique_vitality, set_vitality, runeword_vitality,
        affix_vitality, passive_vitality, buff_vitality, total_vitality;
    int base_intelligence, implicit_intelligence, unique_intelligence, set_intelligence,
        runeword_intelligence, affix_intelligence, passive_intelligence, buff_intelligence,
        total_intelligence;
    /* Additional layered defensive contributions */
    int affix_armor_flat; /* aggregated from armor_flat affixes (Phase 2.1 extension) */
    /* Derived metrics (Phase 2.2 early) */
    int dps_estimate;                                /* simple heuristic */
    int ehp_estimate;                                /* effective HP proxy */
    int toughness_index;                             /* alias of ehp for now until block/DR added */
    int mobility_index;                              /* dexterity scaled heuristic */
    int sustain_index;                               /* placeholder (life steal etc. not present) */
    int rating_crit, rating_haste, rating_avoidance; /* raw ratings */
    int rating_crit_eff_pct, rating_haste_eff_pct, rating_avoidance_eff_pct; /* effective % */
    /* Resist breakdown (Phase 2.3) expressed as integer percentages (0-100 cap). */
    int resist_physical; /* mitigates direct physical damage */
    int resist_fire;
    int resist_cold;
    int resist_lightning;
    int resist_poison; /* generic dot */
    int resist_status; /* generic status buildup (bleed/frost/etc.) */
    /* Phase 7 Defensive Extensions */
    int block_chance;         /* percent 0-100 chance to block (passive) */
    int block_value;          /* flat damage reduction on successful block */
    int phys_conv_fire_pct;   /* percent of incoming physical converted to fire */
    int phys_conv_frost_pct;  /* percent of incoming physical converted to frost */
    int phys_conv_arcane_pct; /* percent of incoming physical converted to arcane */
    int guard_recovery_pct;   /* increases guard meter regen %, reduces drain % */
    int thorns_percent;       /* percent of post-mitigation damage reflected */
    int thorns_cap;           /* max reflect per hit */
    /* Hash / fingerprint (Phase 2.5) */
    unsigned long long fingerprint;
    int dirty;               /* non-zero when cache invalid */
    unsigned int dirty_bits; /* bitmask: 1=attributes,2=passives,4=buffs,8=equipment (Phase 11.1) */
    unsigned int recompute_count;               /* total updates performed */
    unsigned int heavy_passive_recompute_count; /* times passive aggregation recomputed */
} RogueStatCache;

extern RogueStatCache g_player_stat_cache;

/* Phase 11: Equipment analytics export & histograms */
/* Serialize current aggregated equipment stats to JSON (subset) into buffer; returns bytes written
 * (excl NUL) or -1 on error. */
int rogue_equipment_stats_export_json(char* buf, int cap);
/* Record current DPS/EHP into rolling histograms keyed by rarity & slot for later analysis. */
void rogue_equipment_histogram_record(void);
/* Dump histograms as JSON object string into buffer. Returns bytes written or -1. */
int rogue_equipment_histograms_export_json(char* buf, int cap);
/* Simple outlier detector using mean + MAD; returns 1 if current DPS considered outlier for
 * player's weapon rarity. */
int rogue_equipment_dps_outlier_flag(void);
/* Phase 11.3: Set & unique usage tracking */
/* Record current equipped set ids and unique base item occurrences. */
void rogue_equipment_usage_record(void);
/* Export usage metrics as JSON: {"set_<id>":count,"unique_<base_def_index>":count,...} */
int rogue_equipment_usage_export_json(char* buf, int cap);

void rogue_stat_cache_mark_dirty(void);
void rogue_stat_cache_mark_attr_dirty(void);
void rogue_stat_cache_mark_passive_dirty(void);
void rogue_stat_cache_mark_buff_dirty(void);
void rogue_stat_cache_mark_equipment_dirty(void);
/* Export instrumentation counters (Phase 11 tests) */
unsigned int rogue_stat_cache_heavy_passive_recompute_count(void);
size_t rogue_stat_cache_sizeof(void);
void rogue_stat_cache_update(const RoguePlayer* p);       /* no-op if not dirty */
void rogue_stat_cache_force_update(const RoguePlayer* p); /* always recompute */
unsigned long long rogue_stat_cache_fingerprint(void);
/* Soft cap helper (Phase 2.4): applies diminishing returns; cap>0, softness>0 (higher softness ->
 * slower approach) */
float rogue_soft_cap_apply(float value, float cap, float softness);

#endif
