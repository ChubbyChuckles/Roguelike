/* hud_buff_belt.c - UI Phase 6.3 implementation */
#include "hud_buff_belt.h"
#include "../app_state.h"
#include "graphics/font.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_hud_buff_belt_refresh(RogueHUDBuffBeltState* st, double now_ms){
    if(!st) return; st->count=0; RogueBuff buffs[ROGUE_HUD_MAX_BUFF_ICONS];
    int n = rogue_buffs_snapshot(buffs, ROGUE_HUD_MAX_BUFF_ICONS, now_ms);
    for(int i=0;i<n;i++){
        st->icons[st->count].type = buffs[i].type;
        st->icons[st->count].magnitude = buffs[i].magnitude;
        float remaining = (float)(buffs[i].end_ms - now_ms); if(remaining<0) remaining=0; st->icons[st->count].remaining_ms = remaining; st->icons[st->count].total_ms = (float)(buffs[i].end_ms - (buffs[i].end_ms - remaining));
        st->count++;
    }
}

void rogue_hud_buff_belt_render(const RogueHUDBuffBeltState* st, int screen_w){
#ifndef ROGUE_HAVE_SDL
    (void)st; (void)screen_w; /* suppress unused warnings when SDL disabled */
#endif
#ifdef ROGUE_HAVE_SDL
    if(!st || !g_app.renderer) return; int icon_w=22, icon_h=22; int gap=4; int x = (screen_w - (st->count*icon_w + (st->count-1)*gap))/2; int y=4; if(x<4) x=4;
    for(int i=0;i<st->count;i++){
        const RogueHUDBuffIcon* ic = &st->icons[i]; float pct = ic->total_ms>0? (ic->remaining_ms/ic->total_ms):0.0f; if(pct<0)pct=0; if(pct>1)pct=1;
        SDL_SetRenderDrawColor(g_app.renderer,30,30,50,200); SDL_Rect bg={x,y,icon_w,icon_h}; SDL_RenderFillRect(g_app.renderer,&bg);
        SDL_SetRenderDrawColor(g_app.renderer,90,90,140,255); SDL_Rect br={x-1,y-1,icon_w+2,icon_h+2}; SDL_RenderDrawRect(g_app.renderer,&br);
        /* cooldown overlay (filled from bottom -> remaining) */
        int overlay_h = (int)(icon_h * (1.0f-pct)); if(overlay_h>0){ SDL_SetRenderDrawColor(g_app.renderer,0,0,0,140); SDL_Rect ov={x,y,icon_w,overlay_h}; SDL_RenderFillRect(g_app.renderer,&ov); }
        char txt[8]; snprintf(txt,sizeof txt,"%d", ic->magnitude);
        rogue_font_draw_text(x+4,y+4,txt,1,(RogueColor){220,220,255,255});
        x += icon_w + gap;
    }
#endif
}
