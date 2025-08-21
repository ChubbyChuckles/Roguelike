/* Equipment Phase 11.5-11.6: Balance analytics flags & A/B harness */
#ifndef ROGUE_EQUIPMENT_BALANCE_H
#define ROGUE_EQUIPMENT_BALANCE_H
#ifdef __cplusplus
extern "C"
{
#endif

    /* Proc / DR saturation analytics (11.5) */
    void rogue_equipment_analytics_reset(void);
    void rogue_equipment_analytics_record_proc_trigger(
        int magnitude); /* magnitude optional (unused for now) */
    void rogue_equipment_analytics_record_dr_source(
        float reduction_pct);                              /* 0..100 (percent reduction) */
    void rogue_equipment_analytics_analyze(void);          /* recompute flags */
    int rogue_equipment_analytics_flag_proc_oversat(void); /* 1 if oversaturated */
    int rogue_equipment_analytics_flag_dr_chain(void);     /* 1 if stacked DR chain excessive */
    int
    rogue_equipment_analytics_export_json(char* buf,
                                          int cap); /* {"proc_oversaturation":0/1,"dr_chain":0/1} */

/* A/B balance parameter harness (11.6) */
#define ROGUE_BALANCE_VARIANT_CAP 8
    typedef struct RogueBalanceParams
    {
        int id_hash;                /* internal id hash */
        char id[32];                /* human-readable id */
        int outlier_mad_mult;       /* replaces hard-coded MAD multiplier (default 5) */
        int proc_oversat_threshold; /* proc triggers threshold before flag */
        float
            dr_chain_floor; /* minimum post-DR damage fraction; lower triggers flag (default 0.2) */
    } RogueBalanceParams;

    int rogue_balance_register(const RogueBalanceParams* p); /* returns index or <0 */
    int rogue_balance_variant_count(void);
    int rogue_balance_select_deterministic(unsigned int seed); /* selects variant (seed hashed) */
    const RogueBalanceParams* rogue_balance_current(void);
    /* Convenience: configure defaults if no variants registered */
    void rogue_balance_ensure_default(void);

#ifdef __cplusplus
}
#endif
#endif
