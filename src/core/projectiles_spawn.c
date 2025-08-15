#include "core/projectiles.h"
#include "core/projectiles_internal.h"
#include "core/app_state.h"
#include "util/log.h"
#include <math.h>

RogueImpactBurst g_impacts[ROGUE_MAX_IMPACT_BURSTS];
RogueShard g_shards[ROGUE_MAX_SHARDS];
RogueProjectile g_projectiles[ROGUE_MAX_PROJECTILES];
int g_last_projectile_damage = 0;

void rogue_projectiles_init(void){
    for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) g_projectiles[i].active=0;
    for(int i=0;i<ROGUE_MAX_IMPACT_BURSTS;i++) g_impacts[i].active=0;
    for(int i=0;i<ROGUE_MAX_SHARDS;i++) g_shards[i].active=0;
}

void rogue_projectiles_spawn(float x, float y, float dir_x, float dir_y, float speed, float life_ms, int damage){
    float len = sqrtf(dir_x*dir_x + dir_y*dir_y); if(len<=0.0001f) return; dir_x/=len; dir_y/=len;
    for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) if(!g_projectiles[i].active){
        RogueProjectile* p=&g_projectiles[i]; p->active=1; p->x=x; p->y=y; p->speed=speed; p->vx=dir_x*speed; p->vy=dir_y*speed; p->life_ms=0; p->max_life_ms=life_ms; p->damage=damage; g_last_projectile_damage = damage; 
        p->spawn_ms = (float)g_app.game_time_ms; p->anim_t = 0.0f; p->hcount=0; 
        ROGUE_LOG_INFO("Projectile spawned at (%.2f,%.2f) dir=(%.2f,%.2f) speed=%.2f life=%.0fms dmg=%d", x,y,dir_x,dir_y,speed,life_ms,damage);
        return; }
}

void rogue__spawn_impact(float x,float y){
    for(int i=0;i<ROGUE_MAX_IMPACT_BURSTS;i++) if(!g_impacts[i].active){ g_impacts[i].active=1; g_impacts[i].x=x; g_impacts[i].y=y; g_impacts[i].life_ms=260.0f; g_impacts[i].total_ms=260.0f; return; }
}
