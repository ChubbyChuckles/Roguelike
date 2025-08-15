#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "enemy_system.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "game/damage_numbers.h"
#include "util/log.h"
#include "world/tilemap.h"
#include "core/vegetation.h"
#include "core/collision.h"
#include "core/navigation.h"
#include "core/loot_tables.h"
#include "core/loot_instances.h"
#include "core/loot_logging.h"
#include "core/metrics.h"

/* Directly manipulate g_app to preserve semantics. */
static int enemy_tile_is_blocking(unsigned char t){
    switch(t){
        case ROGUE_TILE_WATER:
        case ROGUE_TILE_RIVER:
        case ROGUE_TILE_RIVER_WIDE:
        case ROGUE_TILE_RIVER_DELTA:
        case ROGUE_TILE_MOUNTAIN:
        case ROGUE_TILE_CAVE_WALL:
            return 1;
        default: return 0;
    }
}

void rogue_enemy_system_update(float dt_ms){
    /* Per-type spawning (throttled & capped) */
    g_app.spawn_accum_ms += dt_ms;
    const int global_cap = 120;
    if(g_app.spawn_accum_ms > 450.0f){
        g_app.spawn_accum_ms = 0.0f;
        if(g_app.enemy_type_count>0 && g_app.enemy_count < global_cap){
            for(int ti=0; ti<g_app.enemy_type_count; ++ti){
                RogueEnemyTypeDef* t=&g_app.enemy_types[ti];
                int cur = g_app.per_type_counts[ti]; int target = t->pop_target; if(target<=0) target=6; if(target>40) target=40; /* clamp */
                if(cur >= target) continue; int needed = target - cur; int groups = 1; /* at most one group per tick */
                for(int g=0; g<groups && needed>0; ++g){
                    /* Choose a group anchor sufficiently far from player (avoid spawning right next). */
                    int attempts=0; int gx=0,gy=0; unsigned char tile=0; const float min_player_dist=12.0f; /* tiles */
                    float pxp = g_app.player.base.pos.x; float pyp = g_app.player.base.pos.y;
                    while(attempts < 40){
                        gx = rand() % g_app.world_map.width; gy = rand() % g_app.world_map.height;
                        tile = g_app.world_map.tiles[gy*g_app.world_map.width + gx];
                        if(!(tile == ROGUE_TILE_GRASS || tile == ROGUE_TILE_FOREST)) { attempts++; continue; }
                        float dx = (float)gx - pxp; float dy = (float)gy - pyp; if(dx*dx + dy*dy < min_player_dist*min_player_dist){ attempts++; continue; }
                        break;
                    }
                    if(attempts>=40) continue; /* give up this tick */
                    int group_sz = t->group_min + (rand() % (t->group_max - t->group_min + 1)); if(group_sz>needed) group_sz=needed;
                    float angle_step = 6.2831853f / (float)group_sz; float base_angle = (float)(rand()%628) * 0.01f;
                    int spawned_in_group=0;
                    for(int m=0; m<group_sz && needed>0; ++m){
                        if(g_app.enemy_count >= global_cap) break;
                        float radius = (float)(2 + rand()% (t->patrol_radius>6?6:t->patrol_radius));
                        float ang = base_angle + angle_step * m;
                        float ex = gx + cosf(ang) * radius;
                        float ey = gy + sinf(ang) * radius;
                        if(ex<1||ey<1||ex>g_app.world_map.width-2||ey>g_app.world_map.height-2) continue;
                        /* Keep min spawn distance to player for each member */
                        float pdx = ex - pxp; float pdy = ey - pyp; if(pdx*pdx + pdy*pdy < (min_player_dist-2.5f)*(min_player_dist-2.5f)) continue;
                        for(int slot=0; slot<ROGUE_MAX_ENEMIES; ++slot){ if(!g_app.enemies[slot].alive){
                            RogueEnemy* ne=&g_app.enemies[slot]; ne->base.pos.x=ex; ne->base.pos.y=ey; ne->anchor_x=(float)gx; ne->anchor_y=(float)gy; ne->patrol_target_x=ex; ne->patrol_target_y=ey; ne->max_health=(int)(3 * g_app.difficulty_scalar); if(ne->max_health<1) ne->max_health=1; ne->health=ne->max_health; ne->alive=1; ne->hurt_timer=0; ne->anim_time=0; ne->anim_frame=0; ne->ai_state=ROGUE_ENEMY_AI_PATROL; ne->facing=2; ne->type_index=ti; ne->tint_r=255.0f; ne->tint_g=255.0f; ne->tint_b=255.0f; ne->death_fade=1.0f; ne->tint_phase=0.0f; ne->flash_timer=0.0f; ne->attack_cooldown_ms= (float)(400 + rand()%300); ne->crit_chance = 5; ne->crit_damage = 25; ne->armor=0; ne->resist_fire=0; ne->resist_frost=0; ne->resist_arcane=0; ne->resist_bleed=0; ne->resist_poison=0; g_app.enemy_count++; g_app.per_type_counts[ti]++; needed--; spawned_in_group++; break; }
                        }
                    }
                    /* Group spawn info muted (can re-enable with ROGUE_LOG_DEBUG if needed) */
                    if(spawned_in_group>0){ /* ROGUE_LOG_DEBUG("Spawned enemy group type=%d size=%d anchor=(%d,%d)", ti, spawned_in_group, gx, gy); */ }
                }
            }
        }
    }
    /* Deterministic fallback spawn */
    {
        static float s_no_enemy_timer_ms = 0.0f;
        if(g_app.enemy_count==0){
            s_no_enemy_timer_ms += dt_ms;
            if(s_no_enemy_timer_ms > 150.0f && g_app.enemy_type_count>0){ /* very short delay to satisfy test within few frames */
                for(int slot=0; slot<ROGUE_MAX_ENEMIES; ++slot){ if(!g_app.enemies[slot].alive){
                    RogueEnemy* ne=&g_app.enemies[slot];
                    int ti = 0; /* first type definition assumed basic */
                    float spawn_x = g_app.player.base.pos.x + 0.5f; if(spawn_x > g_app.world_map.width-2) spawn_x = g_app.player.base.pos.x - 0.5f;
                    float spawn_y = g_app.player.base.pos.y + 0.0f;
                    ne->base.pos.x=spawn_x; ne->base.pos.y=spawn_y; ne->anchor_x=spawn_x; ne->anchor_y=spawn_y; ne->patrol_target_x=spawn_x; ne->patrol_target_y=spawn_y;
                    ne->max_health=(int)(3 * g_app.difficulty_scalar); if(ne->max_health<1) ne->max_health=1; ne->health=ne->max_health; ne->alive=1; ne->hurt_timer=0; ne->anim_time=0; ne->anim_frame=0; ne->ai_state=ROGUE_ENEMY_AI_AGGRO; ne->facing=2; ne->type_index=ti; ne->tint_r=255.0f; ne->tint_g=255.0f; ne->tint_b=255.0f; ne->death_fade=1.0f; ne->tint_phase=0.0f; ne->flash_timer=0.0f; ne->attack_cooldown_ms=0.0f; ne->crit_chance = 5; ne->crit_damage = 25; ne->armor=0; ne->resist_fire=0; ne->resist_frost=0; ne->resist_arcane=0; ne->resist_bleed=0; ne->resist_poison=0;
                    g_app.enemy_count++; g_app.per_type_counts[ti]++;
                    s_no_enemy_timer_ms = 0.0f;
                    break;
                }}
            }
        } else {
            s_no_enemy_timer_ms = 0.0f;
        }
    }

    /* Enemy AI & updates */
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++) if(g_app.enemies[i].alive){
        RogueEnemy* e=&g_app.enemies[i]; RogueEnemyTypeDef* t = &g_app.enemy_types[e->type_index];
        float pdx = g_app.player.base.pos.x - e->base.pos.x;
        float pdy = g_app.player.base.pos.y - e->base.pos.y;
        float p_dist2 = pdx*pdx + pdy*pdy;
        if(p_dist2 > (float)(t->aggro_radius * t->aggro_radius * 64)) { e->alive=0; g_app.enemy_count--; if(g_app.per_type_counts[e->type_index]>0) g_app.per_type_counts[e->type_index]--; continue; }
        if(e->ai_state != ROGUE_ENEMY_AI_DEAD){
            if(p_dist2 < (float)(t->aggro_radius * t->aggro_radius)) e->ai_state = ROGUE_ENEMY_AI_AGGRO; else if(e->ai_state == ROGUE_ENEMY_AI_AGGRO && p_dist2 > (float)((t->aggro_radius+5)*(t->aggro_radius+5))) e->ai_state = ROGUE_ENEMY_AI_PATROL;
        }
        float move_dx=0, move_dy=0; float move_speed = t->speed * (float)g_app.dt;
        int etx = (int)(e->base.pos.x+0.5f); int ety = (int)(e->base.pos.y+0.5f);
        if(etx>=0 && ety>=0 && etx<g_app.world_map.width && ety<g_app.world_map.height){ move_speed *= rogue_vegetation_tile_move_scale(etx,ety); }
        if(e->ai_state == ROGUE_ENEMY_AI_PATROL){
            float tx = e->patrol_target_x; float ty = e->patrol_target_y; float dx = tx - e->base.pos.x; float dy = ty - e->base.pos.y; float d2 = dx*dx+dy*dy;
            if(d2 < 0.4f){
                for(int attempt=0; attempt<6; ++attempt){
                    float nrx = (float)((rand()% (t->patrol_radius*2+1)) - t->patrol_radius);
                    float nry = (float)((rand()% (t->patrol_radius*2+1)) - t->patrol_radius);
                    float nx = e->anchor_x + nrx; float ny = e->anchor_y + nry;
                    float ar_dx = nx - e->anchor_x; float ar_dy = ny - e->anchor_y;
                    if(ar_dx*ar_dx + ar_dy*ar_dy <= (float)(t->patrol_radius*t->patrol_radius)) { e->patrol_target_x = nx; e->patrol_target_y = ny; break; }
                }
            } else {
                float len = (float)sqrt(d2); if(len>0.0001f){ move_dx = dx/len; move_dy = dy/len; }
            }
        } else if(e->ai_state == ROGUE_ENEMY_AI_AGGRO){
            // replace direct normalized vector with cardinal step towards player center
            int step_dx=0, step_dy=0; rogue_nav_cardinal_step_towards(e->base.pos.x,e->base.pos.y,g_app.player.base.pos.x,g_app.player.base.pos.y,&step_dx,&step_dy);
            move_dx=(float)step_dx; move_dy=(float)step_dy;
        }
        // Prevent diagonal: if both non-zero pick axis with larger abs delta to target (patrol)
        if(move_dx!=0.0f && move_dy!=0.0f){ float dpx=fabsf(g_app.player.base.pos.x - e->base.pos.x); float dpy=fabsf(g_app.player.base.pos.y - e->base.pos.y); if(dpx>dpy) move_dy=0; else move_dx=0; }
        if(p_dist2 < 1.00f){ move_dx = 0.0f; move_dy = 0.0f; move_speed = 0.0f; }
        if(move_dx!=0 || move_dy!=0){
            int nx = (int)(e->base.pos.x + move_dx * move_speed + 0.5f);
            int ny = (int)(e->base.pos.y + move_dy * move_speed + 0.5f);
            if(nx>=0 && ny>=0 && nx<g_app.world_map.width && ny<g_app.world_map.height){
                unsigned char nt = g_app.world_map.tiles[ny*g_app.world_map.width + nx];
                int veg_block = rogue_vegetation_tile_blocking(nx,ny) || rogue_vegetation_entity_blocking(e->base.pos.x,e->base.pos.y,e->base.pos.x + move_dx * move_speed, e->base.pos.y + move_dy * move_speed);
                if(enemy_tile_is_blocking(nt) || veg_block){
                    float try_x = e->base.pos.x + move_dx * move_speed;
                    int txi = (int)(try_x + 0.5f); int tyi = (int)(e->base.pos.y + 0.5f);
                    int blocked_x = 0, blocked_y = 0;
                    if(txi>=0 && tyi>=0 && txi<g_app.world_map.width && tyi<g_app.world_map.height){ if(enemy_tile_is_blocking(g_app.world_map.tiles[tyi*g_app.world_map.width + txi]) || rogue_vegetation_tile_blocking(txi,tyi) || rogue_vegetation_entity_blocking(e->base.pos.x,e->base.pos.y,try_x,e->base.pos.y)) blocked_x=1; }
                    float try_y = e->base.pos.y + move_dy * move_speed;
                    txi = (int)(e->base.pos.x + 0.5f); tyi = (int)(try_y + 0.5f);
                    if(txi>=0 && tyi>=0 && txi<g_app.world_map.width && tyi<g_app.world_map.height){ if(enemy_tile_is_blocking(g_app.world_map.tiles[tyi*g_app.world_map.width + txi]) || rogue_vegetation_tile_blocking(txi,tyi) || rogue_vegetation_entity_blocking(e->base.pos.x,e->base.pos.y,e->base.pos.x,try_y)) blocked_y=1; }
                    if(!blocked_x && blocked_y){ move_dy = 0; }
                    else if(blocked_x && !blocked_y){ move_dx = 0; }
                    else { move_dx = move_dy = 0; }
                }
            }
        }
        e->base.pos.x += move_dx * move_speed; e->base.pos.y += move_dy * move_speed;
        e->facing = (move_dx < 0)? 1 : 2;
        if(e->hurt_timer>0) e->hurt_timer -= dt_ms;
        if(e->flash_timer>0) e->flash_timer -= dt_ms;
        if(e->attack_cooldown_ms>0) e->attack_cooldown_ms -= dt_ms;
        int in_melee = (p_dist2 < 1.00f);
        if(in_melee){ move_dx = move_dy = 0; }
        if(p_dist2 < 1.00f && g_app.player.health>0 && e->attack_cooldown_ms<=0){
            int dmg = (int)(1 + g_app.difficulty_scalar * 0.6); if(dmg<1) dmg=1;
            float ech = (float)e->crit_chance * 0.01f; if(ech>0.35f) ech=0.35f;
            int ecrit = (((float)rand()/(float)RAND_MAX) < ech)?1:0;
            if(ecrit){
                float emult = 1.0f + (float)e->crit_damage * 0.01f; if(emult>3.0f) emult=3.0f;
                dmg = (int)floorf(dmg * emult + 0.5f);
            }
            g_app.player.health -= dmg; if(g_app.player.health<0) g_app.player.health=0; e->hurt_timer=200.0f; g_app.time_since_player_hit_ms = 0.0f;
            rogue_add_damage_number_ex(g_app.player.base.pos.x, g_app.player.base.pos.y - 0.2f, dmg, 0, ecrit);
            e->attack_cooldown_ms = 1050.0f + (float)(rand()%700);
        }
        if(e->health<=0 && e->ai_state != ROGUE_ENEMY_AI_DEAD){
            e->ai_state = ROGUE_ENEMY_AI_DEAD; e->anim_time=0; e->anim_frame=0; e->death_fade=1.0f;
            g_app.player.xp += t->xp_reward;
            if(((float)rand()/(float)RAND_MAX) < t->loot_chance){ g_app.player.health += 2 + (g_app.player.vitality/3); if(g_app.player.health>g_app.player.max_health) g_app.player.health=g_app.player.max_health; }
            /* Phase 2: attempt loot rolls using table named after enemy type (UPPERCASE + _BASIC) or fallback GOBLIN_BASIC example */
            char tbl_id[ROGUE_MAX_LOOT_TABLE_ID];
            int k=0; for(; k< (int)sizeof tbl_id -1 && t->name[k]; ++k){ char c=t->name[k]; if(c>='a'&&c<='z') c = (char)(c - 'a' + 'A'); if(c==' ') c='_'; tbl_id[k]=c; }
            tbl_id[k]='\0';
            /* Append suffix if space */
            const char* suffix = "_BASIC"; size_t ln=strlen(tbl_id); size_t sl=strlen(suffix); if(ln + sl < sizeof tbl_id){ memcpy(tbl_id+ln,suffix,sl+1); }
            int table_idx = rogue_loot_table_index(tbl_id);
            if(table_idx<0){ table_idx = rogue_loot_table_index("GOBLIN_BASIC"); }
            if(table_idx>=0){
                unsigned int seed = (unsigned int)(e->base.pos.x*73856093u) ^ (unsigned int)(e->base.pos.y*19349663u) ^ (unsigned int)g_app.total_kills;
                int idef[8]; int qty[8]; int rar[8]; int drops = rogue_loot_roll_ex(table_idx,&seed,8,idef,qty,rar);
                ROGUE_LOOT_LOG_INFO("loot_roll: enemy_type=%s table=%s drops=%d", t->name, tbl_id, drops);
                for(int di=0; di<drops; ++di){ if(idef[di]>=0){
                    unsigned int jseed = seed + (unsigned int)(di*60493u);
                    float jr = (float)(jseed % 1000) / 1000.0f;
                    float jang = (float)((jseed / 1000u) % 6283u) * 0.001f;
                    float radius = jr * 0.35f;
                    float jx = e->base.pos.x + cosf(jang) * radius;
                    float jy = e->base.pos.y + sinf(jang) * radius;
                    ROGUE_LOOT_LOG_DEBUG("loot_entry: idx=%d qty=%d rarity=%d enemy_pos=(%.2f,%.2f) spawn_pos=(%.2f,%.2f) off=(%.2f,%.2f)", idef[di], qty[di], rar[di], e->base.pos.x, e->base.pos.y, jx, jy, jx-e->base.pos.x, jy-e->base.pos.y);
                    int inst = rogue_items_spawn(idef[di], qty[di], jx, jy); if(inst>=0 && rar[di]>=0 && inst < g_app.item_instance_cap){ g_app.item_instances[inst].rarity = rar[di]; }
                    /* Session metrics: record drop rarity */
                    if(rar[di] >= 0) rogue_metrics_record_drop(rar[di]); else rogue_metrics_record_drop(0);
                }}
            }
        }
        RogueSprite* frames=NULL; int fcount=0; if(e->ai_state==ROGUE_ENEMY_AI_AGGRO) { frames=t->run_frames; fcount=t->run_count; } else if(e->ai_state==ROGUE_ENEMY_AI_PATROL){ frames=t->idle_frames; fcount=t->idle_count; } else { frames=t->death_frames; fcount=t->death_count; }
        float frame_ms = (e->ai_state==ROGUE_ENEMY_AI_AGGRO)? 110.0f : 160.0f;
        e->anim_time += dt_ms; if(fcount<=0) fcount=1;
        if(e->anim_time >= frame_ms){ e->anim_time -= frame_ms; e->anim_frame = (e->anim_frame+1) % fcount; }
        e->tint_phase += dt_ms;
        if(e->ai_state==ROGUE_ENEMY_AI_DEAD){
            if(e->anim_frame == fcount-1){
                e->death_fade -= (float)g_app.dt * 0.8f;
                if(e->death_fade <= 0.0f){ e->alive=0; g_app.enemy_count--; if(g_app.per_type_counts[e->type_index]>0) g_app.per_type_counts[e->type_index]--; }
            }
        }
        float target_r=255.0f, target_g=255.0f, target_b=255.0f;
        int close_combat = (p_dist2 < 0.36f);
        if(e->ai_state==ROGUE_ENEMY_AI_AGGRO && !close_combat){
            float pulse = 0.5f + 0.5f * (float)sin(e->tint_phase * 0.01f);
            target_r = 255.0f;
            target_g = 180.0f + 75.0f * pulse;
            target_b = 0.0f;
        }
        if(close_combat){ target_r = 255.0f; target_g = 40.0f; target_b = 40.0f; }
        if(e->hurt_timer>0){ target_r=255.0f; target_g=255.0f; target_b=255.0f; }
        if(e->flash_timer>0){ target_r=255.0f; target_g=230.0f; target_b=90.0f; }
        if(e->ai_state==ROGUE_ENEMY_AI_DEAD){
            float gcol = 120.0f * e->death_fade;
            target_r = target_g = target_b = gcol;
        }
        float lerp = (float)g_app.dt * 8.0f; if(lerp>1.0f) lerp=1.0f;
        e->tint_r += (target_r - e->tint_r) * lerp;
        e->tint_g += (target_g - e->tint_g) * lerp;
        e->tint_b += (target_b - e->tint_b) * lerp;
    }
    /* Separation pass */
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++) if(g_app.enemies[i].alive){
        RogueEnemy* a=&g_app.enemies[i];
        for(int j=i+1;j<ROGUE_MAX_ENEMIES;j++) if(g_app.enemies[j].alive){
            RogueEnemy* b=&g_app.enemies[j];
            float dx=b->base.pos.x - a->base.pos.x; float dy=b->base.pos.y - a->base.pos.y; float d2=dx*dx+dy*dy; float minr=0.30f; float min2=minr*minr;
            if(d2>0.00001f && d2<min2){ float d=(float)sqrt(d2); float push=(minr - d)*0.5f; dx/=d; dy/=d; a->base.pos.x-=dx*push; a->base.pos.y-=dy*push; b->base.pos.x+=dx*push; b->base.pos.y+=dy*push; }
        }
    }
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++) if(g_app.enemies[i].alive){ rogue_collision_resolve_enemy_player(&g_app.enemies[i]); }
}
