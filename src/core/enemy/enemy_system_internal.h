/* Internal helpers for enemy system split. */
#ifndef ROGUE_ENEMY_SYSTEM_INTERNAL_H
#define ROGUE_ENEMY_SYSTEM_INTERNAL_H
#include "../../entities/enemy.h"
#include "../../entities/player.h"
#include "../app/app_state.h"
void rogue_enemy_spawn_update(float dt_ms);
void rogue_enemy_ai_update(float dt_ms);
void rogue_enemy_separation_pass(void);
#endif
