#include "game/hit_system.h"
#include "core/app_state.h"
#include "graphics/sprite.h"
#include <stdio.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* Simple bitmap font debug text (reuse existing renderer primitives) */
static void draw_text(int x,int y,const char* msg){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return; int ox=x; const unsigned char* p=(const unsigned char*)msg; while(*p){ unsigned char c=*p++; if(c=='\n'){ y+=12; x=ox; continue; } SDL_Rect r={x,y,6,10}; SDL_SetRenderDrawColor(g_app.renderer,255,255,255,255); SDL_RenderDrawRect(g_app.renderer,&r); x+=7; }
#endif
}

void rogue_hit_debug_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return;
    const RogueHitDebugFrame* df = rogue_hit_debug_last(); if(!df||!df->capsule_valid) return;
    /* Draw capsule as line + two end circles (approx with small rects) */
    float x0=df->last_capsule.x0, y0=df->last_capsule.y0, x1=df->last_capsule.x1, y1=df->last_capsule.y1;
    SDL_SetRenderDrawColor(g_app.renderer,0,200,255,255); /* cyan */
    SDL_RenderDrawLine(g_app.renderer,(int)(x0*32),(int)(y0*32),(int)(x1*32),(int)(y1*32));
    for(int i=-3;i<=3;i++) for(int j=-3;j<=3;j++){ float dx=(float)i,dy=(float)j; if(dx*dx+dy*dy<=9){ SDL_RenderDrawPoint(g_app.renderer,(int)(x0*32+dx),(int)(y0*32+dy)); SDL_RenderDrawPoint(g_app.renderer,(int)(x1*32+dx),(int)(y1*32+dy)); }}
    /* Hit points and normals */
    for(int i=0;i<df->hit_count;i++){
        int ei = df->last_hits[i]; if(ei>=0 && ei<g_app.enemy_count){ float ex=g_app.enemies[ei].base.pos.x; float ey=g_app.enemies[ei].base.pos.y; SDL_SetRenderDrawColor(g_app.renderer,255,255,0,255); SDL_RenderDrawPoint(g_app.renderer,(int)(ex*32),(int)(ey*32)); /* normal line */ SDL_RenderDrawLine(g_app.renderer,(int)(ex*32),(int)(ey*32),(int)(ex*32 + df->normals[i][0]*20),(int)(ey*32 + df->normals[i][1]*20)); }
    }
    RogueHitboxTuning* tune = rogue_hitbox_tuning_get();
    float length_approx = (float)hypot( df->last_capsule.x1-df->last_capsule.x0, df->last_capsule.y1-df->last_capsule.y0 );
    char buf[256]; snprintf(buf,sizeof(buf),"HITDBG frame=%d hits=%d len=%.2f width=%.2f\nPX=(%.2f,%.2f) tuneL=%.2f tuneW=%.2f ER=%.2f purs=(%.2f,%.2f)",
        df->frame_id, df->hit_count, length_approx, df->last_capsule.r*2.0f,
        g_app.player.base.pos.x, g_app.player.base.pos.y, tune->player_length, tune->player_width, tune->enemy_radius, tune->pursue_offset_x, tune->pursue_offset_y);
    draw_text(8,8,buf);
#endif
}
