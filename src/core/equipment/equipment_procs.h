/* Equipment Phase 6: Conditional Effects & Proc Engine */
#ifndef ROGUE_EQUIPMENT_PROCS_H
#define ROGUE_EQUIPMENT_PROCS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RogueProcTrigger
    {
        ROGUE_PROC_ON_HIT = 0,
        ROGUE_PROC_ON_CRIT,
        ROGUE_PROC_ON_KILL,
        ROGUE_PROC_ON_BLOCK,
        ROGUE_PROC_ON_DODGE,
        ROGUE_PROC_WHEN_LOW_HP,
        ROGUE_PROC__TRIGGER_COUNT
    } RogueProcTrigger;

    typedef enum RogueProcStackRule
    {
        ROGUE_PROC_STACK_REFRESH = 0,
        ROGUE_PROC_STACK_STACK,
        ROGUE_PROC_STACK_IGNORE
    } RogueProcStackRule;

    typedef struct RogueProcDef
    {
        int id; /* assigned on register */
        RogueProcTrigger trigger;
        int icd_ms;      /* internal cooldown in ms */
        int duration_ms; /* buff duration ms (0 => instant) */
        int magnitude;   /* abstract effect scalar for tests */
        int max_stacks;  /* for STACK rule */
        RogueProcStackRule stack_rule;
        int param;     /* extra (e.g. low HP threshold %) */
        char name[32]; /* Phase 16.3: authoring/display name (optional, empty if legacy-registered)
                        */
    } RogueProcDef;

    int rogue_proc_register(const RogueProcDef* def); /* returns id or <0 */
    void rogue_procs_reset(void);
    void rogue_procs_update(int dt_ms, int hp_cur, int hp_max); /* advance timers */
    void rogue_procs_event_hit(int was_crit);
    void rogue_procs_event_kill(void);
    void rogue_procs_event_block(void);
    void rogue_procs_event_dodge(void);

    /* Telemetry */
    int rogue_proc_trigger_count(int id);
    int rogue_proc_active_stacks(int id);
    float rogue_proc_uptime_ratio(int id);         /* uptime_ms / total_time_ms */
    void rogue_proc_set_rate_cap_per_sec(int cap); /* global trigger cap */
    float rogue_proc_triggers_per_min(int id);     /* triggers normalized to per-minute */
    int rogue_proc_last_trigger_sequence(int id);  /* deterministic ordering sequence number */
    /* Phase 15.4 auditor helpers */
    int rogue_proc_count(void);                 /* number of registered procs */
    const RogueProcDef* rogue_proc_def(int id); /* readonly access to definition */

    /* Phase 7.4 Reactive Shield helper API: treat any active proc with trigger ON_BLOCK as
     * temporary absorb pool. */
    int rogue_procs_absorb_pool(
        void); /* sum of remaining absorb magnitude across active shield procs */
    int rogue_procs_consume_absorb(
        int amount); /* consume amount from active shields (returns actually consumed) */
    /* Test hook: force activate a proc id with given stacks & duration (bypasses trigger/ICD). */
    int rogue_proc_force_activate(int id, int stacks, int duration_ms);

    /* Phase 16.3: External proc designer JSON tooling
        Schema: [ { "name":"BurningAegis", "trigger":"ON_BLOCK", "icd_ms":5000, "duration_ms":3000,
       "magnitude":50, "max_stacks":3, "stack_rule":"STACK", "param":0 }, ... ] trigger strings:
       ON_HIT, ON_CRIT, ON_KILL, ON_BLOCK, ON_DODGE, WHEN_LOW_HP stack_rule strings: REFRESH, STACK,
       IGNORE */
    int rogue_procs_load_from_json(const char* path); /* returns procs added or <0 */
    int rogue_procs_export_json(char* buf, int cap);  /* returns chars written or -1 */

    /* Validation helper (public for tests): ensures enum ranges & value sanity */
    int rogue_proc_validate(const RogueProcDef* def);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_EQUIPMENT_PROCS_H */
