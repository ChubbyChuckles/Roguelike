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
        ROGUE_EFFECT_STAT_BUFF = 0,
        ROGUE_EFFECT_DOT = 1,              /* Damage over Time (harmful) */
        ROGUE_EFFECT_AURA = 2,             /* Area effect centered on player (radius) */
        ROGUE_EFFECT_HEAL = 3,             /* Healing applied to the player */
        ROGUE_EFFECT_SPAWN_PROJECTILE = 4, /* Spawn a simple projectile from player */
        ROGUE_EFFECT_DAMAGE = 5,           /* Instant single-target damage (enemy) */
        ROGUE_EFFECT_AOE_BLAST = 6,        /* One-shot AoE damage around player */
        ROGUE_EFFECT_TELEPORT = 7,         /* Instantly move player along facing by magnitude */
        ROGUE_EFFECT_SPAWN_ENTITY = 8 /* Spawn one or more simple entities (summons placeholder) */
    } RogueEffectKind;

    /* Target selector for simple conditional gating (Phase 1.2 conditionals) */
    typedef enum RogueEffectTargetType
    {
        ROGUE_TARGET_DEFAULT = 0, /* infer from kind */
        ROGUE_TARGET_SELF = 1,
        ROGUE_TARGET_ENEMY = 2,
        ROGUE_TARGET_AREA = 3
    } RogueEffectTargetType;

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
        unsigned char target;     /* RogueEffectTargetType */
        unsigned char debuff;     /* 1 if harmful debuff (for UI/analytics) */
        unsigned short buff_type; /* maps to RogueBuffType when kind == STAT_BUFF */
        int magnitude;            /* generic magnitude: buff amount, DOT dmg, or AURA dmg */
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
        /* Phase 5: DOT parameters (used when kind == ROGUE_EFFECT_DOT) */
        unsigned char damage_type;     /* RogueDamageType */
        unsigned char crit_mode;       /* 0 = per-tick, 1 = per-application snapshot */
        unsigned char crit_chance_pct; /* used when RNG enabled; tests may force via g_force_crit */
        /* Phase 6: AURA parameters (used when kind == ROGUE_EFFECT_AURA) */
        float aura_radius;            /* tiles radius around player for area effects */
        unsigned int aura_group_mask; /* exclusivity mask (0 = none); reserved */
        /* Phase 6b: SPAWN_PROJECTILE parameters */
        float proj_speed;         /* tiles per second */
        float proj_life_ms;       /* lifetime in ms */
        unsigned char proj_count; /* number of projectiles to spawn (fan not yet supported) */

        /* Phase 1.2 timing controls (node-like per-spec) */
        float delay_ms;             /* if >0, initial application is delayed */
        unsigned char repeat_count; /* number of times to apply, 0/1 = once */
        float repeat_interval_ms;   /* spacing between repeated applications */

        /* Phase 1.2 conditional execution */
        unsigned char caster_health_le_pct; /* if >0, require player health% <= this */
        float max_distance;                 /* if >0 and enemy-targeted, require <= this distance */
        /* Phase 1.2 extension: SPAWN_ENTITY parameters (lightweight placeholder for future
            summoning system). spawn_entity_count spawns that many transient entities (1..8) at the
            player's position with a simple lifetime. Entities have position only and no combat
            behavior yet; used for validating authoring pipeline ahead of full summon system. */
        unsigned char spawn_entity_count; /* number of entities to spawn */
        float spawn_entity_life_ms;       /* lifetime each entity persists before auto-despawn */
    } RogueEffectSpec;

    int rogue_effect_register(const RogueEffectSpec* spec); /* returns id or -1 */
    const RogueEffectSpec* rogue_effect_get(int id);
    /* Public enumeration helper: number of registered EffectSpecs. */
    int rogue_effect_count(void);

    /* Phase 11.3: expose minimal active AURA introspection for debug overlay */
    int rogue_effect_active_aura_count(void);
    /* Returns 1 on success; outputs effect_id and end_ms. Index is 0..count-1. */
    int rogue_effect_active_aura_get(int index, int* effect_id, double* end_ms);
    void rogue_effect_apply(int id, double now_ms);
    /* Schedule a future effect application at the given absolute time (ms). Processed by
        rogue_effects_update(now_ms). No-op if id invalid or queue full. */
    void rogue_effect_schedule_apply(int id, double when_ms);
    /* Phase 3.5: process scheduled effect events (periodic pulses and child chains). */
    void rogue_effects_update(double now_ms);
    void rogue_effect_reset(void); /* free registry for tests */
    /* Query helpers */
    int rogue_effect_spec_is_debuff(int id);

#ifdef __cplusplus
}
#endif
#endif
