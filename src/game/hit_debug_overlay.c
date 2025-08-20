#include "game/hit_system.h"
#include "game/hit_pixel_mask.h"
#include "util/log.h"
#include "core/app_state.h"
#include "graphics/sprite.h"
#include <stdio.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* Simple bitmap font debug text (reuse existing renderer primitives) */
void draw_text(int x,int y,const char* msg){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return; int ox=x; const unsigned char* p=(const unsigned char*)msg; while(*p){ unsigned char c=*p++; if(c=='\n'){ y+=12; x=ox; continue; } SDL_Rect r={x,y,6,10}; SDL_SetRenderDrawColor(g_app.renderer,255,255,255,255); SDL_RenderDrawRect(g_app.renderer,&r); x+=7; }
#else
    (void)x; (void)y; (void)msg;
#endif
}

void rogue_hit_debug_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return;
    const RogueHitDebugFrame* df = rogue_hit_debug_last(); if(!df) return;
    /* Minimal always-visible indicator so we know the function is executing */
    SDL_SetRenderDrawColor(g_app.renderer,255,255,0,180);
    SDL_Rect dbg_tag={4,4,6,6}; SDL_RenderFillRect(g_app.renderer,&dbg_tag);
    static int s_logged_frames=0; if(s_logged_frames<12){
        ROGUE_LOG_INFO("hit_debug_render_call: frame=%d show_hit_debug=%d pixel_toggle=%d pixel_valid=%d bits=%p", g_app.frame_count, g_app.show_hit_debug, g_hit_use_pixel_masks, df->pixel_mask_valid, (void*)df->mask_bits);
        s_logged_frames++;
    }
    /* Pixel mask visualization (slice C) */
    int tsz = g_app.tile_size ? g_app.tile_size : 32;
    if(df->pixel_mask_valid && df->mask_bits){
        float world_x = df->mask_player_x + df->mask_pose_dx - (float)df->mask_origin_x * df->mask_scale_x;
        float world_y = df->mask_player_y + df->mask_pose_dy - (float)df->mask_origin_y * df->mask_scale_y;
        int base_x = (int)(world_x * tsz - g_app.cam_x);
        int base_y = (int)(world_y * tsz - g_app.cam_y);
        SDL_Rect aabb = { base_x, base_y, (int)(df->mask_w * df->mask_scale_x * tsz), (int)(df->mask_h * df->mask_scale_y * tsz) };
        SDL_SetRenderDrawColor(g_app.renderer,40,40,40,160);
        SDL_RenderDrawRect(g_app.renderer,&aabb);
        int step = (df->mask_w * df->mask_h > 8000)?2:1; /* performance guard */
    for(int y=0;y<df->mask_h;y+=step){ const uint32_t* row = df->mask_bits + (size_t)y * df->mask_pitch_words; for(int x=0;x<df->mask_w;x+=step){ uint32_t w = row[x>>5]; if(w & (1u<<(x&31))){ int sx = base_x + (int)(x * df->mask_scale_x * tsz); int sy = base_y + (int)(y * df->mask_scale_y * tsz); SDL_SetRenderDrawColor(g_app.renderer,120,120,255,160); SDL_RenderDrawPoint(g_app.renderer,sx,sy); } } }
        int ox = (int)((df->mask_player_x + df->mask_pose_dx)*tsz - g_app.cam_x);
        int oy = (int)((df->mask_player_y + df->mask_pose_dy)*tsz - g_app.cam_y);
        SDL_SetRenderDrawColor(g_app.renderer,255,0,255,200);
        SDL_RenderDrawLine(g_app.renderer, ox-6, oy, ox+6, oy);
        SDL_RenderDrawLine(g_app.renderer, ox, oy-6, ox, oy+6);
    } else if(g_hit_use_pixel_masks){
        /* Diagnostics when pixel masks are enabled but we have not yet captured a frame */
        char msg[160];
        snprintf(msg,sizeof(msg),"PIXEL MASK: not captured yet (attack to populate) valid=%d bits=%p use=%d", df->pixel_mask_valid, (void*)df->mask_bits, g_hit_use_pixel_masks);
        draw_text(8,56,msg);
    }
    if(df->capsule_valid){
        float x0=df->last_capsule.x0, y0=df->last_capsule.y0, x1=df->last_capsule.x1, y1=df->last_capsule.y1;
        SDL_SetRenderDrawColor(g_app.renderer,0,200,255,255); /* cyan */
        SDL_RenderDrawLine(g_app.renderer,(int)(x0*tsz - g_app.cam_x),(int)(y0*tsz - g_app.cam_y),(int)(x1*tsz - g_app.cam_x),(int)(y1*tsz - g_app.cam_y));
        for(int i=-3;i<=3;i++) for(int j=-3;j<=3;j++){ float dx=(float)i,dy=(float)j; if(dx*dx+dy*dy<=9){ SDL_RenderDrawPoint(g_app.renderer,(int)(x0*tsz - g_app.cam_x + dx),(int)(y0*tsz - g_app.cam_y + dy)); SDL_RenderDrawPoint(g_app.renderer,(int)(x1*tsz - g_app.cam_x + dx),(int)(y1*tsz - g_app.cam_y + dy)); }}
    }
    /* Draw mismatches: pixel-only (yellow), capsule-only (red) */
    for(int i=0;i<df->pixel_hit_count;i++){ int ei=df->pixel_hits[i]; int shared=0; for(int j=0;j<df->capsule_hit_count;j++){ if(df->capsule_hits[j]==ei){ shared=1; break; } } float ex=g_app.enemies[ei].base.pos.x; float ey=g_app.enemies[ei].base.pos.y; if(shared){ SDL_SetRenderDrawColor(g_app.renderer,0,255,0,255); } else { SDL_SetRenderDrawColor(g_app.renderer,255,200,0,255); } SDL_RenderDrawPoint(g_app.renderer,(int)(ex*tsz - g_app.cam_x),(int)(ey*tsz - g_app.cam_y)); }
    for(int i=0;i<df->capsule_hit_count;i++){ int ei=df->capsule_hits[i]; int shared=0; for(int j=0;j<df->pixel_hit_count;j++){ if(df->pixel_hits[j]==ei){ shared=1; break; } } if(shared) continue; float ex=g_app.enemies[ei].base.pos.x; float ey=g_app.enemies[ei].base.pos.y; SDL_SetRenderDrawColor(g_app.renderer,255,0,0,255); SDL_RenderDrawPoint(g_app.renderer,(int)(ex*tsz - g_app.cam_x),(int)(ey*tsz - g_app.cam_y)); }
    /* Authoritative normals */
    for(int i=0;i<df->hit_count;i++){ int ei=df->last_hits[i]; if(ei>=0 && ei<g_app.enemy_count){ float ex=g_app.enemies[ei].base.pos.x; float ey=g_app.enemies[ei].base.pos.y; SDL_SetRenderDrawColor(g_app.renderer,255,255,255,255); SDL_RenderDrawLine(g_app.renderer,(int)(ex*tsz - g_app.cam_x),(int)(ey*tsz - g_app.cam_y),(int)(ex*tsz - g_app.cam_x + df->normals[i][0]*20),(int)(ey*tsz - g_app.cam_y + df->normals[i][1]*20)); }}
    float length_approx = 0.0f; if(df->capsule_valid){ length_approx = (float)hypot( df->last_capsule.x1-df->last_capsule.x0, df->last_capsule.y1-df->last_capsule.y0 ); }
    char buf[256]; snprintf(buf,sizeof(buf),"HITDBG frame=%d used=%c hits=%d cap=%d pix=%d misP=%d misC=%d len=%.2f\nPX=(%.2f,%.2f) mask[%d x %d] origin(%d,%d) pose(%.2f,%.2f) sx=%.2f sy=%.2f",\
        df->frame_id, df->pixel_used?'P':'C', df->hit_count, df->capsule_hit_count, df->pixel_hit_count, df->mismatch_pixel_only, df->mismatch_capsule_only, length_approx,\
        g_app.player.base.pos.x, g_app.player.base.pos.y, df->mask_w, df->mask_h, df->mask_origin_x, df->mask_origin_y, df->mask_pose_dx, df->mask_pose_dy, df->mask_scale_x, df->mask_scale_y);
    draw_text(8,8,buf);
#endif
}
