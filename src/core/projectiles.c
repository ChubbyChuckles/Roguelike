#include "core/projectiles.h"
#include "core/app_state.h"
#include "entities/enemy.h"
#include "game/damage_numbers.h"
#include "util/log.h"
#include <math.h>
#include <stdlib.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#define ROGUE_MAX_PROJECTILES 128
/* Simple transient impact bursts for projectile hits */
typedef struct RogueImpactBurst { float x,y; float life_ms; float total_ms; int active; } RogueImpactBurst;
#define ROGUE_MAX_IMPACT_BURSTS 64
static RogueImpactBurst g_impacts[ROGUE_MAX_IMPACT_BURSTS];
/* Shard particles for flashy impact */
typedef struct RogueShard { float x,y; float vx,vy; float life_ms; float total_ms; int active; float size; } RogueShard;
#define ROGUE_MAX_SHARDS 256
static RogueShard g_shards[ROGUE_MAX_SHARDS];
static RogueProjectile g_projectiles[ROGUE_MAX_PROJECTILES];
static int g_last_projectile_damage = 0;

void rogue_projectiles_init(void){ for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) g_projectiles[i].active=0; for(int i=0;i<ROGUE_MAX_IMPACT_BURSTS;i++) g_impacts[i].active=0; for(int i=0;i<ROGUE_MAX_SHARDS;i++) g_shards[i].active=0; }

void rogue_projectiles_spawn(float x, float y, float dir_x, float dir_y, float speed, float life_ms, int damage){
    float len = sqrtf(dir_x*dir_x + dir_y*dir_y); if(len<=0.0001f) return; dir_x/=len; dir_y/=len;
    for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) if(!g_projectiles[i].active){
    RogueProjectile* p=&g_projectiles[i]; p->active=1; p->x=x; p->y=y; p->speed=speed; p->vx=dir_x*speed; p->vy=dir_y*speed; p->life_ms=0; p->max_life_ms=life_ms; p->damage=damage; g_last_projectile_damage = damage; 
    p->spawn_ms = (float)g_app.game_time_ms; p->anim_t = 0.0f; p->hcount=0; 
    ROGUE_LOG_INFO("Projectile spawned at (%.2f,%.2f) dir=(%.2f,%.2f) speed=%.2f life=%.0fms dmg=%d", x,y,dir_x,dir_y,speed,life_ms,damage);
    return; }
}

static void spawn_impact(float x,float y){
    for(int i=0;i<ROGUE_MAX_IMPACT_BURSTS;i++) if(!g_impacts[i].active){ g_impacts[i].active=1; g_impacts[i].x=x; g_impacts[i].y=y; g_impacts[i].life_ms=260.0f; g_impacts[i].total_ms=260.0f; return; }
}
static void spawn_shards(float x,float y,int count){
    for(int c=0;c<count;c++){
        for(int i=0;i<ROGUE_MAX_SHARDS;i++) if(!g_shards[i].active){
            float ang = (float)( (double)rand() / (double)RAND_MAX ) * 6.2831853f;
            float spd = 2.5f + ((float)rand()/(float)RAND_MAX)*3.5f; /* tiles/sec */
            g_shards[i].active=1; g_shards[i].x=x; g_shards[i].y=y; g_shards[i].vx=cosf(ang)*spd; g_shards[i].vy=sinf(ang)*spd;
            g_shards[i].life_ms = 340.0f + ((float)rand()/(float)RAND_MAX)*120.0f; g_shards[i].total_ms = g_shards[i].life_ms; g_shards[i].size = 4.0f + ((float)rand()/(float)RAND_MAX)*3.0f;
            break; }
    }
}
static void projectile_hit_enemy(RogueProjectile* p, struct RogueEnemy* e){
    e->health -= p->damage;
    rogue_add_damage_number(p->x, p->y - 0.3f, p->damage, 1);
    spawn_impact(p->x,p->y);
    spawn_shards(p->x,p->y, 10);
    if(e->health<=0){ e->alive=0; g_app.enemy_count--; if(g_app.per_type_counts[e->type_index]>0) g_app.per_type_counts[e->type_index]--; }
}

void rogue_projectiles_update(float dt_ms){
    for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) if(g_projectiles[i].active){
        RogueProjectile* p=&g_projectiles[i];
    p->life_ms += dt_ms; if(p->life_ms >= p->max_life_ms){ spawn_impact(p->x,p->y); spawn_shards(p->x,p->y, 6); p->active=0; continue; }
        p->anim_t += dt_ms;
        /* Store history (push previous position before moving) */
        if(p->hcount < ROGUE_PROJECTILE_HISTORY){
            for(int h=p->hcount; h>0; --h){ p->hx[h]=p->hx[h-1]; p->hy[h]=p->hy[h-1]; }
            p->hx[0]=p->x; p->hy[0]=p->y; p->hcount++;
        } else {
            for(int h=ROGUE_PROJECTILE_HISTORY-1; h>0; --h){ p->hx[h]=p->hx[h-1]; p->hy[h]=p->hy[h-1]; }
            p->hx[0]=p->x; p->hy[0]=p->y;
        }
        /* vx/vy are in tiles per second. Convert ms to seconds for integration */
        p->x += p->vx * (dt_ms * (1.0f/1000.0f));
        p->y += p->vy * (dt_ms * (1.0f/1000.0f));
        if(p->x<0||p->y<0||p->x>=g_app.world_map.width||p->y>=g_app.world_map.height){ p->active=0; continue; }
        /* simple AABB vs enemies (point) */
        for(int ei=0; ei<ROGUE_MAX_ENEMIES; ++ei){ if(g_app.enemies[ei].alive){
            float dx = g_app.enemies[ei].base.pos.x - p->x; float dy = g_app.enemies[ei].base.pos.y - p->y; if(dx*dx + dy*dy < 0.5f*0.5f){ projectile_hit_enemy(p,&g_app.enemies[ei]); p->active=0; break; }
        }}
    }
}

static void update_impacts(float dt_ms){
    for(int i=0;i<ROGUE_MAX_IMPACT_BURSTS;i++) if(g_impacts[i].active){
        g_impacts[i].life_ms -= dt_ms; if(g_impacts[i].life_ms<=0){ g_impacts[i].active=0; }
    }
}
static void update_shards(float dt_ms){
    float dt = dt_ms * (1.0f/1000.0f);
    for(int i=0;i<ROGUE_MAX_SHARDS;i++) if(g_shards[i].active){
        g_shards[i].life_ms -= dt_ms;
        g_shards[i].x += g_shards[i].vx * dt;
        g_shards[i].y += g_shards[i].vy * dt;
        g_shards[i].vy += 0.2f * dt; /* slight downward drift */
        if(g_shards[i].life_ms <= 0){ g_shards[i].active=0; }
    }
}

void rogue_projectiles_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return; 
    /* NOTE: Projectile positions are stored in world tile coordinates (like player/enemies). */
    const int tsz = g_app.tile_size;
    float dt_ms = (float)g_app.dt*1000.0f;
    update_impacts(dt_ms);
    update_shards(dt_ms);
    for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) if(g_projectiles[i].active){
        RogueProjectile* p=&g_projectiles[i];
        float life_ratio = p->life_ms / p->max_life_ms; if(life_ratio<0) life_ratio=0; if(life_ratio>1) life_ratio=1;
        /* Pulse alpha (sin) and size over time, fade out near end */
        float pulse = 0.5f + 0.5f * sinf((p->anim_t)*0.02f * 6.283185f); /* pulse every ~50ms */
        float fade = 1.0f - life_ratio;
        float size = 8.0f + 4.0f * pulse; /* 8..12 */
        Uint8 r = (Uint8)(200 + 55 * pulse);
        Uint8 g = (Uint8)(80 + 60 * (1.0f-pulse));
        Uint8 b = 40;
        Uint8 a = (Uint8)(180 + 75 * pulse); a = (Uint8)(a * fade);
        int px = (int)(p->x * tsz - g_app.cam_x);
        int py = (int)(p->y * tsz - g_app.cam_y);
        SDL_SetRenderDrawColor(g_app.renderer,r,g,b,a);
        SDL_Rect core={ (int)(px - size*0.5f), (int)(py - size*0.5f), (int)size, (int)size };
        SDL_RenderFillRect(g_app.renderer,&core);
        /* Add a bright inner core */
        SDL_SetRenderDrawColor(g_app.renderer,255,200,120,(Uint8)(220*fade));
        SDL_Rect inner={core.x+core.w/4, core.y+core.h/4, core.w/2, core.h/2};
        SDL_RenderFillRect(g_app.renderer,&inner);
        /* Simple additive-looking trail: render history with diminishing alpha */
        for(int h=0; h<p->hcount; ++h){
            float t = (float)(h+1)/(float)(ROGUE_PROJECTILE_HISTORY+1);
            int hx = (int)(p->hx[h]*tsz - g_app.cam_x);
            int hy = (int)(p->hy[h]*tsz - g_app.cam_y);
            float hs = size * (0.6f - 0.05f*h);
            if(hs < 2) hs = 2;
            Uint8 ha = (Uint8)(a * (0.4f * (1.0f - t)));
            SDL_SetRenderDrawColor(g_app.renderer,(Uint8)(r*0.8f),(Uint8)(g*0.6f),b,ha);
            SDL_Rect tr={ (int)(hx - hs*0.5f), (int)(hy - hs*0.5f), (int)hs, (int)hs };
            SDL_RenderFillRect(g_app.renderer,&tr);
        }
    }
    /* Render impact bursts (additive-like concentric rings) */
    for(int i=0;i<ROGUE_MAX_IMPACT_BURSTS;i++) if(g_impacts[i].active){
        float norm = 1.0f - (g_impacts[i].life_ms / g_impacts[i].total_ms);
        if(norm<0) norm=0; if(norm>1) norm=1;
        float base_size = 10.0f;
        float radius = base_size + norm * 28.0f;
        int px = (int)(g_impacts[i].x * tsz - g_app.cam_x);
        int py = (int)(g_impacts[i].y * tsz - g_app.cam_y);
        Uint8 alpha_outer = (Uint8)(180 * (1.0f - norm));
        Uint8 alpha_inner = (Uint8)(255 * (1.0f - norm*norm));
        SDL_SetRenderDrawColor(g_app.renderer,255,160,80,alpha_outer);
        SDL_Rect outer={ (int)(px - radius), (int)(py - radius), (int)(radius*2), (int)(radius*2)};
        SDL_RenderFillRect(g_app.renderer,&outer);
        SDL_SetRenderDrawColor(g_app.renderer,255,220,120,alpha_inner);
        float inner_r = radius*0.5f;
        SDL_Rect inner={ (int)(px - inner_r), (int)(py - inner_r), (int)(inner_r*2), (int)(inner_r*2)};
        SDL_RenderFillRect(g_app.renderer,&inner);
    }
    /* Render shards */
    for(int i=0;i<ROGUE_MAX_SHARDS;i++) if(g_shards[i].active){
        float norm = g_shards[i].life_ms / (g_shards[i].total_ms>0? g_shards[i].total_ms:1.0f); if(norm<0) norm=0; if(norm>1) norm=1;
        float fade = norm;
        int px = (int)(g_shards[i].x * tsz - g_app.cam_x);
        int py = (int)(g_shards[i].y * tsz - g_app.cam_y);
        float s = g_shards[i].size * (0.3f + 0.7f*norm);
        Uint8 a = (Uint8)(200 * fade);
        Uint8 r = (Uint8)(255);
        Uint8 gcol = (Uint8)(120 + 80*(1.0f-norm));
        Uint8 b = (Uint8)(50);
        SDL_SetRenderDrawColor(g_app.renderer,r,gcol,b,a);
        SDL_Rect shard={ (int)(px - s*0.5f), (int)(py - s*0.5f), (int)s, (int)s };
        SDL_RenderFillRect(g_app.renderer,&shard);
    }
#endif
}

int rogue_projectiles_active_count(void){
    int n=0; for(int i=0;i<ROGUE_MAX_PROJECTILES;i++) if(g_projectiles[i].active) n++; return n;
}
int rogue_projectiles_last_damage(void){ return g_last_projectile_damage; }
