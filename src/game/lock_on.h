#ifndef ROGUE_GAME_LOCK_ON_H
#define ROGUE_GAME_LOCK_ON_H
/* Phase 5.6: Lock-on subsystem (optional target assist) */
#include "entities/player.h"
#include "entities/enemy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize / reset lock-on related player fields (called from player init). */
void rogue_lockon_reset(RoguePlayer* p);
/* Attempt to acquire a lock-on target. Preference order:
   1. Nearest alive enemy within p->lock_on_radius.
   2. Tie-breaker: smallest angle to player facing direction.
   Returns 1 on success, 0 if none found. */
int rogue_lockon_acquire(RoguePlayer* p, RogueEnemy enemies[], int enemy_count);
/* Cycle to next target around player (angular order ascending). direction=+1 next, -1 previous.
   Respects switch cooldown; returns 1 if switched. */
int rogue_lockon_cycle(RoguePlayer* p, RogueEnemy enemies[], int enemy_count, int direction);
/* Per-frame tick to decrement cooldown (dt_ms milliseconds). */
void rogue_lockon_tick(RoguePlayer* p, float dt_ms);
/* Update facing & obtain magnetized direction (dirx,diry) for strikes when locked; returns 1 if lock active & valid. */
int rogue_lockon_get_dir(RoguePlayer* p, RogueEnemy enemies[], int enemy_count, float* out_dx, float* out_dy);
/* Clear if current target invalid. */
void rogue_lockon_validate(RoguePlayer* p, RogueEnemy enemies[], int enemy_count);

#ifdef __cplusplus
}
#endif

#endif
