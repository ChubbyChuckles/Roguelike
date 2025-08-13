#ifndef ROGUE_GAME_COMBAT_H
#define ROGUE_GAME_COMBAT_H
#include "entities/player.h"
#include "entities/enemy.h"
#include "world/tilemap.h"

typedef enum RogueAttackPhase { ROGUE_ATTACK_IDLE=0, ROGUE_ATTACK_WINDUP, ROGUE_ATTACK_STRIKE, ROGUE_ATTACK_RECOVER } RogueAttackPhase;

typedef struct RoguePlayerCombat {
    RogueAttackPhase phase;
    float timer; /* time in current phase (ms) */
    int combo; /* simple combo counter */
    float stamina; /* 0..100 */
    float stamina_regen_delay;
    /* --- Advanced combat state (added) --- */
    int buffered_attack;      /* 1 if player pressed attack during non-idle phase */
    int hit_confirmed;        /* 1 if current strike has connected (enables early cancel) */
    float strike_time_ms;     /* time spent in STRIKE phase (for early cancel window) */
} RoguePlayerCombat;

void rogue_combat_init(RoguePlayerCombat* pc);
void rogue_combat_update_player(RoguePlayerCombat* pc, float dt_ms, int attack_pressed);
/* Attempt to apply strike damage to enemies during STRIKE phase. Returns kills this frame */
int rogue_combat_player_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy enemies[], int enemy_count);

/* Exposed global (defined in app.c) for stamina regen stat scaling */
extern RoguePlayer g_exposed_player_for_stats;
int rogue_get_current_attack_frame(void);
/* Testing aid: when non-zero, strike ignores animation frame gating (always active) */
extern int rogue_force_attack_active;

#endif
