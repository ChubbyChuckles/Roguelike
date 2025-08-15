#ifndef ROGUE_GAME_COMBAT_H
#define ROGUE_GAME_COMBAT_H
#include "entities/player.h"
#include "entities/enemy.h"
#include "world/tilemap.h"
#include "game/combat_attacks.h"

typedef enum RogueAttackPhase { ROGUE_ATTACK_IDLE=0, ROGUE_ATTACK_WINDUP, ROGUE_ATTACK_STRIKE, ROGUE_ATTACK_RECOVER } RogueAttackPhase;

typedef struct RoguePlayerCombat {
    RogueAttackPhase phase;
    float timer; /* time in current phase (ms) */
    double precise_accum_ms; /* high precision accumulator for drift correction */
    int combo; /* simple combo counter */
    float stamina; /* 0..100 */
    float stamina_regen_delay;
    /* --- Advanced combat state (added) --- */
    int buffered_attack;      /* 1 if player pressed attack during non-idle phase */
    int hit_confirmed;        /* 1 if current strike has connected (enables early cancel) */
    float strike_time_ms;     /* time spent in STRIKE phase (for early cancel window) */
    /* Attack data-driven extensions */
    RogueWeaponArchetype archetype; /* current chain archetype */
    int chain_index;                /* current position in archetype chain */
    RogueWeaponArchetype queued_branch_archetype; /* pending one-shot branch override */
    int queued_branch_pending;      /* flag: apply branch on next attack start */
    int blocked_this_strike;        /* set if current strike was blocked (enables block cancel) */
    int recovered_recently;  /* we just exited recovery -> idle (eligible for late chain) */
    float idle_since_recover_ms; /* time spent idle since recovery ended */
    unsigned int processed_window_mask; /* bitmask of strike windows already applied (multi-hit) */
    /* --- Animation / timeline events (Phase 1A.5) --- */
    unsigned int emitted_events_mask; /* bitmask of emitted per-window begin events */
    /* Simple ring buffer for transient combat events (fx, sfx) */
    int event_count; struct RogueCombatEvent { unsigned short type; unsigned short data; float t_ms; } events[8];
    unsigned short current_window_flags; /* active window flags (includes hyper armor) */
} RoguePlayerCombat;

/* Event type ids (extend later): */
#define ROGUE_COMBAT_EVENT_BEGIN_WINDOW 1  /* data = window index */
#define ROGUE_COMBAT_EVENT_END_WINDOW   2  /* data = window index */
/* Phase 3.3/3.4 additional events */
#define ROGUE_COMBAT_EVENT_STAGGER_ENEMY 3 /* data = enemy index */

/* Phase 2.7: Damage Event (minimal for now) */
typedef struct RogueDamageEvent {
    unsigned short attack_id;   /* from RogueAttackDef->id */
    unsigned char damage_type;  /* RogueDamageType */
    unsigned char crit;         /* 1 if crit */
    int raw_damage;             /* pre-mitigation */
    int mitigated;              /* final applied */
    int overkill;               /* amount beyond remaining health */
    unsigned char execution;    /* 1 if execution trigger (Phase 2.6) */
} RogueDamageEvent;

/* Simple global ring buffer for recent damage events (Phase 2.7) */
#define ROGUE_DAMAGE_EVENT_CAP 64
extern RogueDamageEvent g_damage_events[ROGUE_DAMAGE_EVENT_CAP];
extern int g_damage_event_head; /* next write index */
extern int g_damage_event_total; /* total events recorded (unbounded) */
void rogue_damage_event_record(unsigned short attack_id, unsigned char dmg_type, unsigned char crit, int raw, int mitig, int overkill, unsigned char execution);
/* Consumer APIs (Phase 2.7): snapshot recent events in chronological order (oldest->newest). Returns count copied. */
int rogue_damage_events_snapshot(RogueDamageEvent* out, int max_events);
/* Clear all stored events (useful for tests). */
void rogue_damage_events_clear(void);
/* Crit layering mode (Phase 2.4): 0=crit multiplier applied pre-mitigation (default), 1=post-mitigation */
extern int g_crit_layering_mode;
/* Execution trigger thresholds (tunable). If enemy health prior to hit <= EXEC_HEALTH_PCT of max AND killing blow, or overkill exceeds OVERKILL_PCT of max health => execution. */
#define ROGUE_EXEC_HEALTH_PCT 0.15f
#define ROGUE_EXEC_OVERKILL_PCT 0.25f
/* Phase 3.6: Defensive weight soft cap tuning constants */
#define ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD 0.65f  /* fraction of combined physical reduction above which soft cap applies */
#define ROGUE_DEF_SOFTCAP_SLOPE 0.45f                /* slope multiplier for excess reduction beyond threshold */
#define ROGUE_DEF_SOFTCAP_MAX_REDUCTION 0.85f        /* absolute ceiling on combined reduction */
#define ROGUE_DEF_SOFTCAP_MIN_RAW 100                /* skip soft cap for trivial hits (raw<100) to preserve low-hit behavior */

/* Apply mitigation (Phase 2 pipeline). Returns final damage >=1 unless health already <=0. */
int rogue_apply_mitigation_enemy(struct RogueEnemy* e, int raw, unsigned char dmg_type, int *out_overkill);

void rogue_combat_init(RoguePlayerCombat* pc);
void rogue_combat_update_player(RoguePlayerCombat* pc, float dt_ms, int attack_pressed);
/* Attempt to apply strike damage to enemies during STRIKE phase. Returns kills this frame */
int rogue_combat_player_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy enemies[], int enemy_count);
/* Pop (consume) queued combat events into caller-provided buffer; returns number copied. */
int rogue_combat_consume_events(RoguePlayerCombat* pc, struct RogueCombatEvent* out_events, int max_events);
/* --- Phase 3.8 Guard / 3.9 Perfect Guard / 3.10 Poise Regen Curve APIs --- */
/* Begin guarding (directional block). Passing guard_dir (0..3 same as facing) caches facing; returns 1 if guard started. */
int rogue_player_begin_guard(RoguePlayer* p, int guard_dir);
/* Update guard state each frame (dt_ms). Should be called from main loop. Returns chip damage blocked this frame (for tests). */
int rogue_player_update_guard(RoguePlayer* p, float dt_ms);
/* Attempt to apply incoming enemy melee damage with guard / perfect guard logic. Returns final applied damage to health. */
/* Apply incoming melee with optional poise damage component. poise_damage applies if not blocked (or reduced if normal block). */
int rogue_player_apply_incoming_melee(RoguePlayer* p, float raw_damage, float attack_dir_x, float attack_dir_y, int poise_damage, int *out_blocked, int *out_perfect);
/* Poise regeneration tick (Phase 3.10) separate to allow tests to drive deterministically. */
void rogue_player_poise_regen_tick(RoguePlayer* p, float dt_ms);
/* --- Phase 4 Reactions & I-Frames --- */
void rogue_player_update_reactions(RoguePlayer* p, float dt_ms);
/* Apply a reaction given cumulative poise damage or direct thresholds. This sets reaction_type & timers. */
void rogue_player_apply_reaction(RoguePlayer* p, int reaction_type);
/* Phase 4.5: Attempt early cancel of current reaction (returns 1 if canceled). */
int rogue_player_try_reaction_cancel(RoguePlayer* p);
/* Phase 4.5: Apply directional influence during an active reaction (dx,dy in -1..1). */
void rogue_player_apply_reaction_di(RoguePlayer* p, float dx, float dy);
/* Phase 4.6: Grant i-frames (overlap protection: takes max, not additive). */
void rogue_player_add_iframes(RoguePlayer* p, float ms);
/* Tunables (could move to config later) */
#define ROGUE_GUARD_CONE_DOT 0.25f            /* minimum dot(facing, incoming) to count as frontal */
#define ROGUE_GUARD_CHIP_PCT 0.20f            /* chip damage percent of mitigated damage (min 1) */
#define ROGUE_GUARD_METER_DRAIN_ON_BLOCK 50.0f/* guard meter drain per successful block */
#define ROGUE_GUARD_METER_DRAIN_HOLD_PER_MS 0.045f /* passive drain while holding guard */
#define ROGUE_GUARD_METER_RECOVER_PER_MS 0.030f    /* regen when not guarding */
#define ROGUE_PERFECT_GUARD_REFUND 35.0f      /* guard meter refunded on perfect guard */
#define ROGUE_PERFECT_GUARD_POISE_BONUS 20.0f /* poise restored on perfect */
#define ROGUE_POISE_REGEN_BASE_PER_MS 0.015f  /* base poise regen after delay */
#define ROGUE_POISE_REGEN_DELAY_AFTER_HIT 650.0f /* ms delay after poise damage */
#define ROGUE_GUARD_BLOCK_POISE_SCALE 0.40f   /* percent of incoming poise damage applied on normal block */
#define ROGUE_HYPER_ARMOR_POISE_IGNORE 1      /* flag controlling poise ignore during hyper armor */

/* Hyper armor global toggle (updated internally by combat strike evaluation). */
void rogue_player_set_hyper_armor_active(int active);

/* Data-driven attack helpers */
void rogue_combat_set_archetype(RoguePlayerCombat* pc, RogueWeaponArchetype arch);
RogueWeaponArchetype rogue_combat_current_archetype(const RoguePlayerCombat* pc);
int rogue_combat_current_chain_index(const RoguePlayerCombat* pc);
/* Queue a branch (different archetype) to be consumed by the next started attack (distinct from simple buffered next). */
void rogue_combat_queue_branch(RoguePlayerCombat* pc, RogueWeaponArchetype branch_arch);
/* Notify combat system that current outgoing attack was blocked (defender blocked). */
void rogue_combat_notify_blocked(RoguePlayerCombat* pc);

/* Exposed global (defined in app.c) for stamina regen stat scaling */
extern RoguePlayer g_exposed_player_for_stats;
int rogue_get_current_attack_frame(void);
/* Testing aid: when non-zero, strike ignores animation frame gating (always active) */
extern int rogue_force_attack_active;
/* Test support: when >=0, forces current attack animation frame for strike logic */
extern int g_attack_frame_override;
void rogue_combat_test_force_strike(RoguePlayerCombat* pc, float strike_time_ms);

#endif
