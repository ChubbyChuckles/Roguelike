#include <math.h>
#include <stdlib.h>

#include "enemy_system.h"
#include "enemy_system_internal.h"

/* Orchestrator: preserve original update ordering exactly. */
void rogue_enemy_system_update(float dt_ms){
    rogue_enemy_spawn_update(dt_ms);      /* group + fallback spawn */
    rogue_enemy_ai_update(dt_ms);         /* movement / combat / death & loot */
    rogue_enemy_separation_pass();        /* separation & player collision */
}
