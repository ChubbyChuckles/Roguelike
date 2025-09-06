#ifndef ROGUE_CORE_SKILLS_H
#define ROGUE_CORE_SKILLS_H
#include <stdint.h>

/* Forward declares */
struct RogueSkillDef;
struct RogueSkillState;

/* Skill activation context */
typedef struct RogueSkillCtx
{
    double now_ms; /* global time (ms) */
    int player_level;
    int talent_points;      /* remaining */
    unsigned int rng_state; /* deterministic per-activation RNG (Phase 1.6) */
    float partial_scalar;   /* early cancel scaling factor (1A.4) */
    /* Phase 1.3: activation context (positions and affected entities) */
    float cast_pos_x; /* world position where cast originates (defaults to player) */
    float cast_pos_y;
    float target_pos_x; /* explicit target aim/ground position if applicable */
    float target_pos_y;
    int affected_entity_ids[8]; /* small fixed buffer for simple effects */
    unsigned char affected_entity_count;
} RogueSkillCtx;

/* Effect callback (optional). Return 1 if activation consumed resources. */
typedef int (*RogueSkillEffectFn)(const struct RogueSkillDef* def, struct RogueSkillState* st,
                                  const RogueSkillCtx* ctx);

/* Activation outcome flags returned by on_activate (bitmask). Use CONSUMED to indicate
   resources should be considered spent; MISSED/RESISTED enable partial refunds. */
enum
{
    ROGUE_ACT_NONE = 0,
    ROGUE_ACT_CONSUMED = 1 << 0,
    ROGUE_ACT_MISSED = 1 << 1,
    ROGUE_ACT_RESISTED = 1 << 2,
};

/* Definition (immutable) */
/* Skill type enum (Phase 1.1). 0 = UNKNOWN/AUTO for backward compatibility. */
typedef enum RogueSkillType
{
    ROGUE_SKTYPE_UNKNOWN = 0,
    ROGUE_SKTYPE_MELEE = 1,
    ROGUE_SKTYPE_RANGED = 2,
    ROGUE_SKTYPE_AOE_SPELL = 3,
    ROGUE_SKTYPE_BUFF = 4,
    ROGUE_SKTYPE_DEBUFF = 5,
    ROGUE_SKTYPE_HEAL = 6,
    ROGUE_SKTYPE_SUMMON = 7,
    ROGUE_SKTYPE_PASSIVE = 8,
    ROGUE_SKTYPE_ULTIMATE = 9
} RogueSkillType;
typedef struct RogueSkillDef
{
    int id;             /* index into registry */
    const char* name;   /* display name */
    const char* icon;   /* icon file path (PNG 512x512 raw) */
    int max_rank;       /* maximum rank */
    int skill_strength; /* ring constraint for maze layout (0 = any ring; N = must be placed on ring
                           N where outermost ring == total rings) */
    unsigned char skill_type;             /* RogueSkillType; 0 = UNKNOWN (auto/infer) */
    float base_cooldown_ms;               /* base cooldown at rank 1 */
    float cooldown_reduction_ms_per_rank; /* linear reduction */
    RogueSkillEffectFn on_activate;
    int is_passive;             /* 1 = passive (no activation / cooldown) */
    int tags;                   /* bitfield tags (element, school, etc) (1.1) */
    int synergy_id;             /* -1 none: synergy bucket (passive) */
    int synergy_value_per_rank; /* contribution per rank to synergy bucket */
    /* Phase 1 additions */
    int resource_cost_mana;         /* simple mana cost (legacy) */
    int action_point_cost;          /* AP cost (1.5) */
    int max_charges;                /* 0 = no charges (1.3) */
    float charge_recharge_ms;       /* per-charge regen time (1.3) */
    float cast_time_ms;             /* >0 => cast; 0 => instant; channel when cast_type==2 (1.3) */
    unsigned short input_buffer_ms; /* queue window (1.3 placeholder) */
    unsigned short min_weave_ms;    /* minimum separation for weaving rule (1A.2) */
    unsigned char early_cancel_min_pct; /* minimum % progress to allow early cancel (1A.4) */
    unsigned char cast_type;            /* 0 instant,1 cast,2 channel */
    unsigned char combo_builder;        /* flag: grants combo point (1.3 placeholder) */
    unsigned char combo_spender;        /* flag: spends combo points */
    unsigned char reserved_u8;          /* padding future */
    int effect_spec_id;                 /* primary EffectSpec reference (1.2) */
    /* Phase 1.2 – Effect Composition: optional additional effect nodes triggered on activation
       alongside the primary effect. Each node can be delayed and optionally repeated.
       Conditions are intentionally minimal in v1 to avoid target plumbing; only a
       require_player_health_below_pct gate is provided. */
    unsigned char effect_node_count; /* number of additional nodes in array (0..3) */
    struct RogueSkillEffectNode
    {
        int effect_spec_id; /* EffectSpec to apply */
        float delay_ms;     /* delay from trigger time */
        /* If repeat_count > 0, schedule exactly that many repeats after the first at
            repeat_interval_ms. Otherwise, when duration_ms > 0 and repeat_interval_ms > 0,
            schedule repeats at t0 + k*repeat_interval_ms for all k>=1 such that
            k*repeat_interval_ms <= duration_ms. */
        float duration_ms;                             /* window length for implicit repeats */
        int repeat_count;                              /* times after first schedule (0 = none) */
        float repeat_interval_ms;                      /* interval between repeats */
        unsigned char require_player_health_below_pct; /* 0=ignore; else 1..100 */
    } effect_nodes[3];
    /* 1A.3 haste evaluation mode flags (0 = dynamic read). Bits:
        bit0: snapshot haste for casts; bit1: snapshot haste for channels. */
    unsigned char haste_mode_flags;
    /* Phase 2.2 – Cost mapping extensions (optional; fall back to action_point_cost/mana).
        If ap_cost_pct_max>0, AP cost = floor(max_action_points * ap_cost_pct_max/100).
        ap_cost_per_rank adds per-rank delta (rank>=2). Surcharge triggers when current AP is
        below ap_cost_surcharge_threshold. Same semantics for mana_* fields. */
    unsigned char ap_cost_pct_max;       /* 0..100 percent of max AP */
    short ap_cost_per_rank;              /* additive per-rank delta (can be negative) */
    short ap_cost_surcharge_amount;      /* flat extra AP when threshold condition met */
    short ap_cost_surcharge_threshold;   /* AP < threshold -> apply surcharge */
    unsigned char mana_cost_pct_max;     /* 0..100 percent of max mana */
    short mana_cost_per_rank;            /* additive per-rank delta */
    short mana_cost_surcharge_amount;    /* flat extra mana when threshold condition met */
    short mana_cost_surcharge_threshold; /* mana < threshold -> apply surcharge */
    /* Phase 2.3 – Refund mechanics (percentages 0..100) */
    unsigned char refund_on_miss_pct;   /* refund percent of costs if effect indicates MISS */
    unsigned char refund_on_resist_pct; /* refund percent if effect indicates RESIST */
    unsigned char refund_on_cancel_pct; /* refund percent when early-canceled */

    /* Optional visual/audio/animation/AoE/projectile metadata (Phase 1 groundwork). */
    /* Visual sprite assets (paths; may be relative to assets/) */
    const char* cast_sprite_sheet; /* sprite sheet used during cast */
    const char* projectile_sprite; /* projectile sprite or sheet */
    const char* impact_sprite;     /* impact sprite */
    const char* aoe_sprite;        /* ground AoE decal sprite */

    /* Animation parameters (sprite-sheet based) */
    int frame_count;               /* total frames in sheet/animation */
    float frame_duration_ms;       /* per-frame duration */
    unsigned char animation_loops; /* 0 no, 1 yes */
    unsigned short grid_width;     /* sheet grid width (cells), 0 if N/A */
    unsigned short grid_height;    /* sheet grid height (cells), 0 if N/A */

    /* Audio hooks */
    const char* cast_sound_id;   /* sound label/id for cast */
    const char* impact_sound_id; /* sound label/id for impact */
    const char* loop_sound_id;   /* sound label/id for loop while channeling */
    unsigned char sound_volume;  /* 0..100 percent */
    float sound_pitch_variance;  /* +/- semitones variance */

    /* AoE parameters */
    unsigned char aoe_shape; /* 0 none,1 circle,2 cone,3 line,4 poly(reserved) */
    float aoe_radius;        /* radius (or line half-width) */
    float aoe_angle;         /* cone angle (deg) or line length if paired with shape */

    /* Projectile parameters */
    float projectile_velocity;     /* units per second */
    unsigned char trajectory_type; /* 0 linear,1 arc,2 homing,3 scatter */
    unsigned char pierce_count;    /* number of targets it can pierce */
    float homing_strength;         /* homing factor */

    /* --- Phase 1.2 Extension: Experimental EffectNode Tree Structure -----------------------
       Legacy composition used a flat array `effect_nodes[3]` with ad-hoc delay_ms values that
       authors derived from a temporary connection UI. To enable richer sequencing while
       preserving backward compatibility, we introduce an optional small tree (DAG) of effect
       nodes. When `effect_tree_node_count > 0`, the legacy `effect_nodes` array is ignored
       at runtime. Each tree node references an EffectSpec plus timing/gating parameters and
       a parent index. Parent index of -1 denotes a root-level node (scheduled relative to
       skill activation). Delay semantics (v1): node.start_time = parent_start_time + delay_ms
       (roots use activation time). Future phases may add span-based anchoring modes.
       Soft cap: 8 nodes per skill to keep scheduling O(N). */
    struct RogueSkillEffectTreeNode
    {
        int effect_spec_id;       /* EffectSpec to apply */
        float delay_ms;           /* delay from parent start (or activation if root) */
        float duration_ms;        /* repeat window length (same semantics as legacy nodes) */
        int repeat_count;         /* explicit repeat count (times after first) */
        float repeat_interval_ms; /* interval between repeats */
        unsigned char require_player_health_below_pct; /* HP gate (0 ignore) */
        signed char parent_index; /* -1 root, otherwise [0, effect_tree_node_count) */
    } effect_tree_nodes[8];
    unsigned char effect_tree_node_count; /* 0 => unused; else number of tree nodes (1..8) */
} RogueSkillDef;

/* Tag bits */
enum
{
    ROGUE_SKILL_TAG_NONE = 0,
    ROGUE_SKILL_TAG_FIRE = 1 << 0,
    ROGUE_SKILL_TAG_FROST = 1 << 1,
    ROGUE_SKILL_TAG_ARCANE = 1 << 2,
    ROGUE_SKILL_TAG_MOVEMENT = 1 << 3,
    ROGUE_SKILL_TAG_DEFENSE = 1 << 4,
    ROGUE_SKILL_TAG_SUPPORT = 1 << 5,
    ROGUE_SKILL_TAG_CONTROL = 1 << 6,
};

/* Player-owned state */
typedef struct RogueSkillState
{
    int rank; /* 0 = locked/unlearned */
    double cooldown_end_ms;
    int uses; /* total uses lifetime (for tests/metrics) */
    /* Phase 1 state extension */
    int charges_cur;                 /* current charges */
    double next_charge_ready_ms;     /* when next charge becomes available */
    double last_cast_ms;             /* last successful activation time */
    double cast_progress_ms;         /* accumulated cast time (if casting) */
    double channel_end_ms;           /* channel completion time */
    double queued_until_ms;          /* queued activation window end */
    double queued_trigger_ms;        /* when to attempt buffered activation (1A.1) */
    double channel_next_tick_ms;     /* next scheduled channel tick (1A.5) */
    int action_points_spent_session; /* AP metric */
    int combo_points_accum;          /* combo point bank */
    unsigned char casting_active;    /* 1 if mid-cast (cast_type==1) */
    unsigned char channel_active;    /* 1 if channel running (cast_type==2) */
    /* 1A.3 snapshot support & 1A.5 drift correction */
    double haste_factor_cast;        /* >0 when cast haste is snapshotted */
    double haste_factor_channel;     /* >0 when channel haste is snapshotted */
    double channel_start_ms;         /* anchor for drift-corrected ticks */
    double channel_tick_interval_ms; /* >0 when channel tick interval is snapshotted */
    /* Phase 1.3: lightweight profiling timestamps (last activation lifecycle marks) */
    double profile_last_act_start_ms;
    double profile_last_act_end_ms;
    double profile_last_cast_begin_ms;
    double profile_last_cast_end_ms;
    /* Phase 1.3: interruption marker */
    unsigned char interrupted_active; /* 1 if last action was interrupted */
    double last_interrupt_ms;         /* timestamp of last interrupt (ms) */
    /* Phase 1.3 (Advanced State Machine): queued flag indicating a pending activation */
    unsigned char queued_active; /* 1 if enqueued via advanced queue */
} RogueSkillState;

/* Phase 1.3: Minimal execution state reporting enum and accessor */
typedef enum RogueSkillExecState
{
    ROGUE_SKEXEC_IDLE = 0,
    ROGUE_SKEXEC_CASTING = 1,
    ROGUE_SKEXEC_CHANNELING = 2,
    ROGUE_SKEXEC_COOLDOWN = 3,
    ROGUE_SKEXEC_INTERRUPTED = 4,
    ROGUE_SKEXEC_QUEUED = 5,        /* newly added: skill request queued */
    ROGUE_SKEXEC_GLOBAL_LOCKOUT = 6 /* global lockout preventing activation */
} RogueSkillExecState;

/* Return current execution state derived from timers/flags. */
static inline RogueSkillExecState rogue_skill_get_exec_state(int id)
{
    extern int g_skill_count_internal;
    extern struct RogueSkillState* g_skill_states_internal;
    extern struct RogueSkillDef* g_skill_defs_internal;
    if (id < 0 || id >= g_skill_count_internal)
        return ROGUE_SKEXEC_IDLE;
    const RogueSkillState* st = &g_skill_states_internal[id];
    const RogueSkillDef* def = &g_skill_defs_internal[id];
    double now = 0.0; /* conservative; callers that need precise should compare cooldown_end_ms */
    if (st->casting_active && def->cast_type == 1)
        return ROGUE_SKEXEC_CASTING;
    if (st->channel_active && def->cast_type == 2)
        return ROGUE_SKEXEC_CHANNELING;
    if (st->queued_active)
        return ROGUE_SKEXEC_QUEUED;
    if (st->interrupted_active)
        return ROGUE_SKEXEC_INTERRUPTED;
    extern double g_skill_global_lockout_until_ms_internal;
    if (g_skill_global_lockout_until_ms_internal > now)
        return ROGUE_SKEXEC_GLOBAL_LOCKOUT;
    if (st->cooldown_end_ms > now)
        return ROGUE_SKEXEC_COOLDOWN;
    return ROGUE_SKEXEC_IDLE;
}

/* RNG helper (LCG) for deterministic local stream (1.6) */
static inline unsigned int rogue_skill_rng_next(RogueSkillCtx* ctx)
{
    ctx->rng_state = ctx->rng_state * 1664525u + 1013904223u;
    return ctx->rng_state;
}

/* Lifecycle */
void rogue_skills_init(void);
void rogue_skills_shutdown(void);

/* Registration (call during init before gameplay) */
int rogue_skill_register(const RogueSkillDef* def); /* returns id */

/* Rank management */
int rogue_skill_rank_up(int id); /* returns new rank or -1 */

/* Activation */
int rogue_skill_try_activate(int id, const RogueSkillCtx* ctx); /* 1 success, 0 fail */

/* Phase 1.3 Advanced State Machine: request activation with queuing semantics.
    Returns 1 if either activated immediately or successfully queued, 0 on rejection.
    A queued activation transitions the skill state to ROGUE_SKEXEC_QUEUED until fired. */
int rogue_skill_request(int id, const RogueSkillCtx* ctx);

/* Phase 1.3: Interrupt an in-progress cast or channel. Applies optional refunds based on
    def.refund_on_cancel_pct and clears casting/channel flags. Returns 1 if something was
    interrupted, 0 otherwise. */
int rogue_skill_interrupt(int id, const RogueSkillCtx* ctx);
int rogue_skill_try_cancel(int id,
                           const RogueSkillCtx* ctx); /* early cancel attempt for cast; 1 success */

/* Frame update (cooldowns etc) */
void rogue_skills_update(double now_ms);

/* Query */
const RogueSkillDef* rogue_skill_get_def(int id);
const struct RogueSkillState* rogue_skill_get_state(int id);
int rogue_skill_synergy_total(int synergy_id);

/* API: export a deterministic hash of currently active buffs (type,magnitude,end_ms snapshot).
    Used by replay/analytics. Returns FNV-1a 64-bit hash, 0 when no buffs active. */
uint64_t skill_export_active_buffs_hash(double now_ms);

/* API: return an effective damage coefficient scalar for a skill id combining
    mastery and specialization contributions. Baseline 1.0f. */
float skill_get_effective_coefficient(int skill_id);

/* API: simulate a simple priority-based skill rotation for a fixed duration.
     profile_json format (tiny parser, all keys optional unless noted):
         {
             "duration_ms": <number, required>,
             "tick_ms": <number, default 16>,
             "ap_regen_per_sec": <number, default 0>,
             "priority": [<int skill ids>]
         }
     Writes a compact JSON result into out_buf on success, e.g.:
         {"duration_ms":1000,"total_casts":12,"ap_spent":120,
            "casts":[{"id":0,"count":7},{"id":1,"count":5}]}
     Returns 0 on success, <0 on parse/error. Non-reentrant (uses global state). */
int skill_simulate_rotation(const char* profile_json, char* out_buf, int out_cap);

/* Data-driven loading */
int rogue_skills_load_from_cfg(
    const char* path); /* returns number loaded; supports CSV (.cfg) or JSON (.json) */

/* Reload skills from file while preserving player-facing state (skill bar, talent points, etc).
    Frees and rebuilds the registry (defs/states/icon textures). Returns count loaded. */
int rogue_skills_reload_from_cfg(const char* path);

/* Test helper: when enabled (non-zero), skills loading will skip icon texture loading entirely.
    This reduces I/O and SDL work in headless/unit test contexts. Default is 0 (icons load). */
void rogue_skills_set_skip_icon_loads(int enable);

#endif
