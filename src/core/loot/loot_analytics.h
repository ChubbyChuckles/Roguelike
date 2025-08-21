/* Loot analytics & telemetry (Phase 18.1/18.2) */
#ifndef ROGUE_LOOT_ANALYTICS_H
#define ROGUE_LOOT_ANALYTICS_H
#ifdef __cplusplus
extern "C"
{
#endif

#define ROGUE_LOOT_ANALYTICS_RING_CAP 512

    typedef struct RogueLootDropEvent
    {
        int def_index;
        int rarity; /* RogueItemRarity */
        double t_seconds;
    } RogueLootDropEvent;

    /* Drift alert & session summary (Phase 18.3/18.4) */
    typedef struct RogueLootSessionSummary
    {
        int total_drops;
        int rarity_counts[5];
        double duration_seconds; /* last_time - first_time (0 if <2 events) */
        double drops_per_min;    /* computed from duration, 0 if duration ~0 */
        int drift_flags[5];      /* 1 if rarity deviates beyond threshold */
        int drift_any;           /* OR of drift_flags */
    } RogueLootSessionSummary;

    void rogue_loot_analytics_reset(void);
    void rogue_loot_analytics_record(int def_index, int rarity, double t_seconds);
    int rogue_loot_analytics_count(void);
    int rogue_loot_analytics_recent(
        int max, RogueLootDropEvent* out); /* returns number copied (most recent first) */
    void rogue_loot_analytics_rarity_counts(
        int out_counts[5]); /* fills rarity counts (assumes 5 tiers) */
    int rogue_loot_analytics_export_json(char* buf, int cap); /* returns 0 on success */

    /* Phase 18.3: Rarity distribution drift detection */
    void rogue_loot_analytics_set_baseline_counts(
        const int counts[5]); /* derives baseline fractions from expected relative counts */
    void rogue_loot_analytics_set_baseline_fractions(
        const float fracs[5]); /* alternative explicit fractions (not required to sum to 1;
                                  auto-normalized) */
    void rogue_loot_analytics_set_drift_threshold(
        float rel_fraction); /* default 0.5 (50% relative deviation) */
    int rogue_loot_analytics_check_drift(int out_flags[5]); /* returns 1 if any drift flag set */

    /* Phase 18.4: Session summary */
    void rogue_loot_analytics_session_summary(RogueLootSessionSummary* out);

/* Phase 18.5: Heatmap of drop positions */
#define ROGUE_LOOT_HEAT_W 32
#define ROGUE_LOOT_HEAT_H 32
    void rogue_loot_analytics_record_pos(
        int def_index, int rarity, double t_seconds, int x,
        int y); /* records event + position (also updates baseline metrics) */
    int rogue_loot_analytics_heat_at(int x, int y); /* returns count (out of bounds -> 0) */
    int rogue_loot_analytics_export_heatmap_csv(
        char* buf, int cap); /* minimal CSV export: first line header, subsequent rows y=0..H-1 */

#ifdef __cplusplus
}
#endif
#endif
