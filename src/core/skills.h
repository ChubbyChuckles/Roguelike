#ifndef ROGUE_CORE_SKILLS_H
#define ROGUE_CORE_SKILLS_H
#include <stdint.h>

/* Forward declares */
struct RogueSkillDef;
struct RogueSkillState;

/* Skill activation context */
typedef struct RogueSkillCtx {
    double now_ms; /* global time (ms) */
    int player_level;
    int talent_points; /* remaining */
    unsigned int rng_state; /* deterministic per-activation RNG (Phase 1.6) */
    float partial_scalar; /* early cancel scaling factor (1A.4) */
} RogueSkillCtx;

/* Effect callback (optional). Return 1 if activation consumed resources. */
typedef int (*RogueSkillEffectFn)(const struct RogueSkillDef* def, struct RogueSkillState* st, const RogueSkillCtx* ctx);

/* Definition (immutable) */
typedef struct RogueSkillDef {
    int id;                     /* index into registry */
    const char* name;           /* display name */
    const char* icon;           /* icon path (future) */
    int max_rank;               /* maximum rank */
    float base_cooldown_ms;     /* base cooldown at rank 1 */
    float cooldown_reduction_ms_per_rank; /* linear reduction */
    RogueSkillEffectFn on_activate;
    int is_passive;             /* 1 = passive (no activation / cooldown) */
    int tags;                   /* bitfield tags (element, school, etc) (1.1) */
    int synergy_id;             /* -1 none: synergy bucket (passive) */
    int synergy_value_per_rank; /* contribution per rank to synergy bucket */
    /* Phase 1 additions */
    int resource_cost_mana;     /* simple mana cost (legacy) */
    int action_point_cost;      /* AP cost (1.5) */
    int max_charges;            /* 0 = no charges (1.3) */
    float charge_recharge_ms;   /* per-charge regen time (1.3) */
    float cast_time_ms;         /* >0 => cast; 0 => instant; channel when cast_type==2 (1.3) */
    unsigned short input_buffer_ms; /* queue window (1.3 placeholder) */
    unsigned short min_weave_ms;    /* minimum separation for weaving rule (1A.2) */
    unsigned char early_cancel_min_pct; /* minimum % progress to allow early cancel (1A.4) */
    unsigned char cast_type;    /* 0 instant,1 cast,2 channel */
    unsigned char combo_builder;/* flag: grants combo point (1.3 placeholder) */
    unsigned char combo_spender;/* flag: spends combo points */
    unsigned char reserved_u8;  /* padding future */
    int effect_spec_id;          /* unified EffectSpec reference (1.2) */
} RogueSkillDef;

/* Tag bits */
enum {
    ROGUE_SKILL_TAG_NONE      = 0,
    ROGUE_SKILL_TAG_FIRE      = 1<<0,
    ROGUE_SKILL_TAG_FROST     = 1<<1,
    ROGUE_SKILL_TAG_ARCANE    = 1<<2,
    ROGUE_SKILL_TAG_MOVEMENT  = 1<<3,
    ROGUE_SKILL_TAG_DEFENSE   = 1<<4,
    ROGUE_SKILL_TAG_SUPPORT   = 1<<5,
    ROGUE_SKILL_TAG_CONTROL   = 1<<6,
};

/* Player-owned state */
typedef struct RogueSkillState {
    int rank;           /* 0 = locked/unlearned */
    double cooldown_end_ms;
    int uses;           /* total uses lifetime (for tests/metrics) */
    /* Phase 1 state extension */
    int charges_cur;            /* current charges */
    double next_charge_ready_ms;/* when next charge becomes available */
    double last_cast_ms;        /* last successful activation time */
    double cast_progress_ms;    /* accumulated cast time (if casting) */
    double channel_end_ms;      /* channel completion time */
    double queued_until_ms;     /* queued activation window end */
    double queued_trigger_ms;   /* when to attempt buffered activation (1A.1) */
    double channel_next_tick_ms;/* next scheduled channel tick (1A.5) */
    int action_points_spent_session; /* AP metric */
    int combo_points_accum;     /* combo point bank */
    unsigned char casting_active; /* 1 if mid-cast (cast_type==1) */
    unsigned char channel_active; /* 1 if channel running (cast_type==2) */
} RogueSkillState;

/* RNG helper (LCG) for deterministic local stream (1.6) */
static inline unsigned int rogue_skill_rng_next(RogueSkillCtx* ctx){ ctx->rng_state = ctx->rng_state * 1664525u + 1013904223u; return ctx->rng_state; }

/* Lifecycle */
void rogue_skills_init(void);
void rogue_skills_shutdown(void);

/* Registration (call during init before gameplay) */
int rogue_skill_register(const RogueSkillDef* def); /* returns id */

/* Rank management */
int rogue_skill_rank_up(int id); /* returns new rank or -1 */

/* Activation */
int rogue_skill_try_activate(int id, const RogueSkillCtx* ctx); /* 1 success, 0 fail */
int rogue_skill_try_cancel(int id, const RogueSkillCtx* ctx);   /* early cancel attempt for cast; 1 success */

/* Frame update (cooldowns etc) */
void rogue_skills_update(double now_ms);

/* Query */
const RogueSkillDef* rogue_skill_get_def(int id);
const struct RogueSkillState* rogue_skill_get_state(int id);
int rogue_skill_synergy_total(int synergy_id);

#endif
