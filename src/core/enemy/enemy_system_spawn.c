#include <math.h>
#include <stdlib.h>
#include "enemy_system_internal.h"
#include "world/tilemap.h"

static int enemy_tile_is_blocking(unsigned char t){
    switch(t){
        case ROGUE_TILE_WATER: case ROGUE_TILE_RIVER: case ROGUE_TILE_RIVER_WIDE: case ROGUE_TILE_RIVER_DELTA: case ROGUE_TILE_MOUNTAIN: case ROGUE_TILE_CAVE_WALL: return 1;
        default: return 0;
    }
}

void rogue_enemy_spawn_update(float dt_ms){
    g_app.spawn_accum_ms += dt_ms;
    const int global_cap = 120;
    if(g_app.spawn_accum_ms > 450.0f){
        g_app.spawn_accum_ms = 0.0f;
        if(g_app.enemy_type_count>0 && g_app.enemy_count < global_cap){
            for(int ti=0; ti<g_app.enemy_type_count; ++ti){
                RogueEnemyTypeDef* t=&g_app.enemy_types[ti];
                int cur = g_app.per_type_counts[ti]; int target = t->pop_target; if(target<=0) target=6; if(target>40) target=40;
                if(cur >= target) continue; int needed = target - cur; int groups = 1;
                for(int g=0; g<groups && needed>0; ++g){
                    int attempts=0; int gx=0,gy=0; unsigned char tile=0; const float min_player_dist=12.0f;
                    float pxp = g_app.player.base.pos.x; float pyp = g_app.player.base.pos.y;
                    while(attempts < 40){
                        gx = rand() % g_app.world_map.width; gy = rand() % g_app.world_map.height;
                        tile = g_app.world_map.tiles[gy*g_app.world_map.width + gx];
                        if(!(tile == ROGUE_TILE_GRASS || tile == ROGUE_TILE_FOREST)) { attempts++; continue; }
                        float dx = (float)gx - pxp; float dy = (float)gy - pyp; if(dx*dx + dy*dy < min_player_dist*min_player_dist){ attempts++; continue; }
                        break;
                    }
                    if(attempts>=40) continue;
                    int group_sz = t->group_min + (rand() % (t->group_max - t->group_min + 1)); if(group_sz>needed) group_sz=needed;
                    float angle_step = 6.2831853f / (float)group_sz; float base_angle = (float)(rand()%628) * 0.01f;
                    int spawned_in_group=0;
                    for(int m=0; m<group_sz && needed>0; ++m){
                        if(g_app.enemy_count >= global_cap) break;
                        float radius = (float)(2 + rand()% (t->patrol_radius>6?6:t->patrol_radius)); float ang = base_angle + angle_step * m;
                        float ex = gx + cosf(ang) * radius; float ey = gy + sinf(ang) * radius;
                        if(ex<1||ey<1||ex>g_app.world_map.width-2||ey>g_app.world_map.height-2) continue;
                        float pdx = ex - pxp; float pdy = ey - pyp; if(pdx*pdx + pdy*pdy < (min_player_dist-2.5f)*(min_player_dist-2.5f)) continue;
                        for(int slot=0; slot<ROGUE_MAX_ENEMIES; ++slot){ if(!g_app.enemies[slot].alive){
                            RogueEnemy* ne=&g_app.enemies[slot]; ne->team_id=1; ne->base.pos.x=ex; ne->base.pos.y=ey; ne->anchor_x=(float)gx; ne->anchor_y=(float)gy; ne->patrol_target_x=ex; ne->patrol_target_y=ey; ne->max_health=(int)(3 * g_app.difficulty_scalar); if(ne->max_health<1) ne->max_health=1; ne->health=ne->max_health; ne->alive=1; ne->hurt_timer=0; ne->anim_time=0; ne->anim_frame=0; ne->ai_state=ROGUE_ENEMY_AI_PATROL; ne->facing=2; ne->type_index=ti; ne->tint_r=255.0f; ne->tint_g=255.0f; ne->tint_b=255.0f; ne->death_fade=1.0f; ne->tint_phase=0.0f; ne->flash_timer=0.0f; ne->attack_cooldown_ms= (float)(400 + rand()%300); ne->crit_chance = 5; ne->crit_damage = 25; ne->armor=0; ne->resist_physical=0; ne->resist_fire=0; ne->resist_frost=0; ne->resist_arcane=0; ne->resist_bleed=0; ne->resist_poison=0; ne->guard_meter_max=60.0f; ne->guard_meter=ne->guard_meter_max; ne->poise_max=40.0f; ne->poise=ne->poise_max; ne->staggered=0; ne->stagger_timer_ms=0.0f; ne->ai_intensity=1; ne->ai_intensity_score=1.0f; ne->ai_intensity_cooldown_ms=0.0f; g_app.enemy_count++; g_app.per_type_counts[ti]++; needed--; spawned_in_group++; break; }
                        }
                    }
                    if(spawned_in_group>0){ /* debug hook */ }
                }
            }
        }
    }
    static float s_no_enemy_timer_ms = 0.0f;
    if(g_app.enemy_count==0){
        s_no_enemy_timer_ms += dt_ms;
        if(s_no_enemy_timer_ms > 150.0f && g_app.enemy_type_count>0){
            for(int slot=0; slot<ROGUE_MAX_ENEMIES; ++slot){ if(!g_app.enemies[slot].alive){
                RogueEnemy* ne=&g_app.enemies[slot]; int ti = 0; float spawn_x = g_app.player.base.pos.x + 0.5f; if(spawn_x > g_app.world_map.width-2) spawn_x = g_app.player.base.pos.x - 0.5f; float spawn_y = g_app.player.base.pos.y;
                ne->team_id=1; ne->base.pos.x=spawn_x; ne->base.pos.y=spawn_y; ne->anchor_x=spawn_x; ne->anchor_y=spawn_y; ne->patrol_target_x=spawn_x; ne->patrol_target_y=spawn_y;
                ne->max_health=(int)(3 * g_app.difficulty_scalar); if(ne->max_health<1) ne->max_health=1; ne->health=ne->max_health; ne->alive=1; ne->hurt_timer=0; ne->anim_time=0; ne->anim_frame=0; ne->ai_state=ROGUE_ENEMY_AI_AGGRO; ne->facing=2; ne->type_index=ti; ne->tint_r=255.0f; ne->tint_g=255.0f; ne->tint_b=255.0f; ne->death_fade=1.0f; ne->tint_phase=0.0f; ne->flash_timer=0.0f; ne->attack_cooldown_ms=0.0f; ne->crit_chance = 5; ne->crit_damage = 25; ne->armor=0; ne->resist_physical=0; ne->resist_fire=0; ne->resist_frost=0; ne->resist_arcane=0; ne->resist_bleed=0; ne->resist_poison=0; ne->guard_meter_max=60.0f; ne->guard_meter=ne->guard_meter_max; ne->poise_max=40.0f; ne->poise=ne->poise_max; ne->staggered=0; ne->stagger_timer_ms=0.0f; ne->ai_intensity=2; ne->ai_intensity_score=2.0f; ne->ai_intensity_cooldown_ms=0.0f; g_app.enemy_count++; g_app.per_type_counts[ti]++; s_no_enemy_timer_ms = 0.0f; break; }}
        }
    } else { s_no_enemy_timer_ms = 0.0f; }
    (void)enemy_tile_is_blocking;
}
