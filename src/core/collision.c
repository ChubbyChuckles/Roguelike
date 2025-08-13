#include <math.h>
#include "core/collision.h"
#include "core/app_state.h"
#include "entities/enemy.h"
#include "entities/player.h"

/* Minimum separation radius between enemy center and player center (tiles). */
static const float ROGUE_ENEMY_PLAYER_MIN_DIST = 0.30f;

void rogue_collision_resolve_enemy_player(struct RogueEnemy* e){
    if(!e || !e->alive) return;
    float dx = e->base.pos.x - g_app.player.base.pos.x;
    float dy = e->base.pos.y - g_app.player.base.pos.y;
    float d2 = dx*dx + dy*dy;
    float min2 = ROGUE_ENEMY_PLAYER_MIN_DIST * ROGUE_ENEMY_PLAYER_MIN_DIST;
    if(d2 < min2){
        float d = (float)sqrt(d2);
        if(d < 1e-5f){ dx = 1.0f; dy = 0.0f; d = 1.0f; }
        float push = (ROGUE_ENEMY_PLAYER_MIN_DIST - d);
        dx /= d; dy /= d;
        e->base.pos.x += dx * push;
        e->base.pos.y += dy * push;
    }
}
