/* EffectSpec system (Phase 1.2 partial -> Phase 3 pipeline additions) */
#ifndef ROGUE_CORE_EFFECT_SPEC_H
#define ROGUE_CORE_EFFECT_SPEC_H
#ifdef __cplusplus
extern "C"
{
#endif

    /* Effect kinds (expanded later) */
    typedef enum RogueEffectKind
    {
        ROGUE_EFFECT_STAT_BUFF = 0
    } RogueEffectKind;

    /* Forward-declare buff stacking rule for specs */
    typedef enum RogueBuffStackRule RogueBuffStackRule;

    /* Child link for simple effect graph composition (Phase 3.6) */
    typedef struct RogueEffectChild
    {
        int child_effect_id; /* effect id to schedule */
        float delay_ms;      /* delay from parent apply time */
    } RogueEffectChild;

    typedef struct RogueEffectSpec
    {
        int id;                   /* registry id (assigned on register) */
        unsigned char kind;       /* RogueEffectKind */
        unsigned char target;     /* reserved for target selection (self, enemy, area) */
        unsigned short buff_type; /* maps to RogueBuffType when kind == STAT_BUFF */
        int magnitude;            /* buff magnitude (stacking additive with existing) */
        float duration_ms;        /* applied buff duration */
        /* Phase 3.3 stacking rules + 3.4 snapshot flag */
        unsigned char stack_rule; /* RogueBuffStackRule */
        unsigned char snapshot;   /* 1 = snapshot magnitude */
        /* Phase 3.4: per-attribute granularity (magnitude scaling).
            If scale_by_buff_type != 0xFFFF, effective magnitude is:
                magnitude * (100 + scale_pct_per_point * total(scale_by_buff_type)) / 100
            If snapshot_scale != 0, pulses schedule with the multiplier captured at apply time;
            otherwise pulses recompute using live totals at tick time. */
        unsigned short scale_by_buff_type; /* RogueBuffType or 0xFFFF for none */
        int scale_pct_per_point;      /* percent per point of referenced buff (e.g., 10 => +10% per
                                         point) */
        unsigned char snapshot_scale; /* 1 = snapshot the scale multiplier at apply */
        /* Phase 3.2 preconditions (simple gating before apply). Use 0xFFFF to mean "no
         * requirement". */
        unsigned short require_buff_type; /* RogueBuffType required to be active (total >= min).
                                             0xFFFF = none */
        int require_buff_min; /* Minimum total required for require_buff_type (defaults to 1 when
                                 type set) */
        /* Phase 3.5 periodic tick scheduler (optional) */
        float pulse_period_ms; /* if >0, re-apply every period until duration bound */
        /* Phase 3.6 simple effect graph (up to 4 children) */
        unsigned char child_count;    /* number of children in array */
        RogueEffectChild children[4]; /* child descriptors */
    } RogueEffectSpec;

    int rogue_effect_register(const RogueEffectSpec* spec); /* returns id or -1 */
    const RogueEffectSpec* rogue_effect_get(int id);
    void rogue_effect_apply(int id, double now_ms);
    /* Phase 3.5: process scheduled effect events (periodic pulses and child chains). */
    void rogue_effects_update(double now_ms);
    void rogue_effect_reset(void); /* free registry for tests */

#ifdef __cplusplus
}
#endif
#endif
