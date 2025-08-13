#include "core/projectiles.h"
#include "core/app_state.h"
#include "entities/enemy.h"
#include <math.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#define ROGUE_MAX_PROJECTILES 128
static RogueProjectile g_projectiles[ROGUE_MAX_PROJECTILES];

void rogue_projectiles_init(void){ for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) g_projectiles[i].active=0; }

void rogue_projectiles_spawn(float x, float y, float dir_x, float dir_y, float speed, float life_ms, int damage){
    float len = sqrtf(dir_x*dir_x + dir_y*dir_y); if(len<=0.0001f) return; dir_x/=len; dir_y/=len;
    for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) if(!g_projectiles[i].active){
        RogueProjectile* p=&g_projectiles[i]; p->active=1; p->x=x; p->y=y; p->speed=speed; p->vx=dir_x*speed; p->vy=dir_y*speed; p->life_ms=0; p->max_life_ms=life_ms; p->damage=damage; return; }
}

static void projectile_hit_enemy(RogueProjectile* p, struct RogueEnemy* e){ (void)p; (void)e; /* integrate with combat later - just reduce health */ e->health -= p->damage; if(e->health<=0){ e->alive=0; g_app.enemy_count--; if(g_app.per_type_counts[e->type_index]>0) g_app.per_type_counts[e->type_index]--; }
}

void rogue_projectiles_update(float dt_ms){
    for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) if(g_projectiles[i].active){
        RogueProjectile* p=&g_projectiles[i];
        p->life_ms += dt_ms; if(p->life_ms >= p->max_life_ms){ p->active=0; continue; }
        p->x += p->vx * (dt_ms/1000.0f);
        p->y += p->vy * (dt_ms/1000.0f);
        if(p->x<0||p->y<0||p->x>=g_app.world_map.width||p->y>=g_app.world_map.height){ p->active=0; continue; }
        /* simple AABB vs enemies (point) */
        for(int ei=0; ei<ROGUE_MAX_ENEMIES; ++ei){ if(g_app.enemies[ei].alive){
            float dx = g_app.enemies[ei].base.pos.x - p->x; float dy = g_app.enemies[ei].base.pos.y - p->y; if(dx*dx + dy*dy < 0.5f*0.5f){ projectile_hit_enemy(p,&g_app.enemies[ei]); p->active=0; break; }
        }}
    }
}

void rogue_projectiles_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return; SDL_SetRenderDrawColor(g_app.renderer,255,140,40,255);
    for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) if(g_projectiles[i].active){
        SDL_Rect r={(int)(g_projectiles[i].x-2),(int)(g_projectiles[i].y-2),4,4}; SDL_RenderFillRect(g_app.renderer,&r);
    }
#endif
}
