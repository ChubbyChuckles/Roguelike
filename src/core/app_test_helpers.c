/* Test helper implementations extracted from former app.c monolith. */
#include "core/app_state.h"
#include "core/app.h"
#include "entities/enemy.h"

RogueEnemy* rogue_test_spawn_hostile_enemy(float x, float y){
    if(g_app.enemy_type_count <= 0) return NULL;
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++) if(!g_app.enemies[i].alive){
        RogueEnemy* ne = &g_app.enemies[i];
        float px = g_app.player.base.pos.x + x;
        float py = g_app.player.base.pos.y + y;
        ne->base.pos.x = px; ne->base.pos.y = py; ne->anchor_x = px; ne->anchor_y = py;
        ne->patrol_target_x = px; ne->patrol_target_y = py; /* stationary for deterministic timing */
        ne->max_health = 10; ne->health = 10; ne->alive = 1; ne->hurt_timer = 0; ne->anim_time = 0; ne->anim_frame = 0;
        ne->ai_state = ROGUE_ENEMY_AI_AGGRO; ne->facing = 2; ne->type_index = 0; ne->tint_r = 255; ne->tint_g = 255; ne->tint_b = 255; ne->death_fade = 1.0f; ne->tint_phase = 0; ne->flash_timer = 0; ne->attack_cooldown_ms = 0; /* immediate first attack */
        ne->crit_chance = 5; ne->crit_damage = 25;
        g_app.enemy_count++; g_app.per_type_counts[0]++;
        return ne;
    }
    return NULL;
}
