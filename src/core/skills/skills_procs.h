#ifndef ROGUE_CORE_SKILLS_PROCS_H
#define ROGUE_CORE_SKILLS_PROCS_H

#include "../integration/event_bus.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Proc definition (Phase 7.2 + 7.3) */
    typedef struct RogueSkillProcDef
    {
        RogueEventTypeId event_type;   /* event type to listen to */
        int effect_spec_id;            /* EffectSpec to apply when proc triggers */
        double icd_global_ms;          /* global internal cooldown (ms); 0 = none */
        double icd_per_target_ms;      /* per-target internal cooldown (ms); 0 = none */
        RogueEventPredicate predicate; /* optional condition; NULL = always */
        const char* name;              /* debug label (optional) */
        /* Phase 7.3: probability weighting */
        int chance_pct;    /* 0..100; 100 = always trigger; 0 = never */
        int use_smoothing; /* 1 enables accumulator smoothing (pity) */
    } RogueProcDef;

    /* API (skills proc engine; prefixed to avoid collision with equipment proc engine) */
    void rogue_skills_procs_init(void);
    void rogue_skills_procs_shutdown(void);
    void rogue_skills_procs_reset(void);

    /* Register returns proc id (>=0) or -1 on failure. */
    int rogue_skills_proc_register(const RogueProcDef* def);

    /* Introspection helpers (Phase 10.3 validation): retrieve registered procs. */
    int rogue_skills_proc_count(void);
    /* Returns 1 on success filling out, 0 if index invalid. */
    int rogue_skills_proc_get_def(int index, RogueProcDef* out);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_SKILLS_PROCS_H */
