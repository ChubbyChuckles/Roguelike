#ifndef ROGUE_ENEMY_SYSTEM_H
#define ROGUE_ENEMY_SYSTEM_H

/* Enemy spawning & AI update subsystem extracted from app.c to reduce monolith size.
 * All functions operate on global g_app (RogueAppState) for parity with prior design.
 * Behavior must remain identical to pre-extraction version (tests rely on timing & counts).
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "app_state.h"

/* Update spawning (group waves + deterministic fallback) and AI for all enemies.
 * dt_ms: scaled delta milliseconds (after hitstop scaling) matching previous usage in app.c
 */
void rogue_enemy_system_update(float dt_ms);

/* For tests: allow explicit decay / tick if ever needed (currently just an alias). */
static inline void rogue_enemy_system_tick(float dt_ms){ rogue_enemy_system_update(dt_ms); }

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ENEMY_SYSTEM_H */
