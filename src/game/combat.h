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
} RoguePlayerCombat;

/* Event type ids (extend later): */
#define ROGUE_COMBAT_EVENT_BEGIN_WINDOW 1  /* data = window index */
#define ROGUE_COMBAT_EVENT_END_WINDOW   2  /* data = window index */

void rogue_combat_init(RoguePlayerCombat* pc);
void rogue_combat_update_player(RoguePlayerCombat* pc, float dt_ms, int attack_pressed);
/* Attempt to apply strike damage to enemies during STRIKE phase. Returns kills this frame */
int rogue_combat_player_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy enemies[], int enemy_count);
/* Pop (consume) queued combat events into caller-provided buffer; returns number copied. */
int rogue_combat_consume_events(RoguePlayerCombat* pc, struct RogueCombatEvent* out_events, int max_events);

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
