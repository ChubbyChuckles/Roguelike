#include "core/hud.h"
#include "graphics/font.h"
#include "core/hud_layout.h" /* Phase 6.1 data-driven HUD layout */
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_hud_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return;

    // Load layout on first use (lazy) - test harness can call loader explicitly.
    const RogueHUDLayout* lay = rogue_hud_layout();
    int hp_w=lay->health.w,hp_h=lay->health.h,hp_x=lay->health.x,hp_y=lay->health.y;
    float hp_ratio=(g_app.player.max_health>0)? (float)g_app.player.health/(float)g_app.player.max_health:0.0f;
    if(hp_ratio<0) hp_ratio=0; if(hp_ratio>1) hp_ratio=1;
    SDL_SetRenderDrawColor(g_app.renderer,40,12,12,255);
    SDL_Rect hbgb={hp_x-2,hp_y-2,hp_w+4,hp_h+4}; SDL_RenderFillRect(g_app.renderer,&hbgb);
    SDL_SetRenderDrawColor(g_app.renderer,95,0,0,255);
    SDL_Rect hbf1={hp_x,hp_y,(int)(hp_w*hp_ratio),hp_h}; SDL_RenderFillRect(g_app.renderer,&hbf1);
    SDL_SetRenderDrawColor(g_app.renderer,170,20,20,255);
    SDL_Rect hbf2={hp_x,hp_y,(int)(hp_w*hp_ratio*0.55f),hp_h}; SDL_RenderFillRect(g_app.renderer,&hbf2);

    // Mana bar
    int mp_w=lay->mana.w,mp_h=lay->mana.h,mp_x=lay->mana.x,mp_y=lay->mana.y;
    float mp_ratio=(g_app.player.max_mana>0)? (float)g_app.player.mana/(float)g_app.player.max_mana:0.0f;
    if(mp_ratio<0) mp_ratio=0; if(mp_ratio>1) mp_ratio=1;
    SDL_SetRenderDrawColor(g_app.renderer,10,18,40,255);
    SDL_Rect mpbgb={mp_x-2,mp_y-2,mp_w+4,mp_h+4}; SDL_RenderFillRect(g_app.renderer,&mpbgb);
    SDL_SetRenderDrawColor(g_app.renderer,15,50,140,255);
    SDL_Rect mpbf1={mp_x,mp_y,(int)(mp_w*mp_ratio),mp_h}; SDL_RenderFillRect(g_app.renderer,&mpbf1);
    SDL_SetRenderDrawColor(g_app.renderer,40,90,210,255);
    SDL_Rect mpbf2={mp_x,mp_y,(int)(mp_w*mp_ratio*0.55f),mp_h}; SDL_RenderFillRect(g_app.renderer,&mpbf2);

    // XP bar
    int xp_w=lay->xp.w,xp_h=lay->xp.h,xp_x=lay->xp.x,xp_y=lay->xp.y;
    float xp_ratio=(g_app.player.xp_to_next>0)? (float)g_app.player.xp/(float)g_app.player.xp_to_next:0.0f;
    if(xp_ratio<0) xp_ratio=0; if(xp_ratio>1) xp_ratio=1;
    SDL_SetRenderDrawColor(g_app.renderer,25,25,25,255);
    SDL_Rect xpbgb={xp_x-2,xp_y-2,xp_w+4,xp_h+4}; SDL_RenderFillRect(g_app.renderer,&xpbgb);
    SDL_SetRenderDrawColor(g_app.renderer,90,60,10,255);
    SDL_Rect xpbf1={xp_x,xp_y,(int)(xp_w*xp_ratio),xp_h}; SDL_RenderFillRect(g_app.renderer,&xpbf1);
    SDL_SetRenderDrawColor(g_app.renderer,200,140,30,255);
    SDL_Rect xpbf2={xp_x,xp_y,(int)(xp_w*xp_ratio*0.55f),xp_h}; SDL_RenderFillRect(g_app.renderer,&xpbf2);

    // Level text
    char lvlbuf[32];
    snprintf(lvlbuf,sizeof lvlbuf,"Lv %d", g_app.player.level);
    rogue_font_draw_text(lay->level_text_x, lay->level_text_y, lvlbuf,1,(RogueColor){255,255,180,255});
#endif
}

void rogue_stats_panel_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.show_stats_panel) return;
    if(!g_app.renderer || g_app.headless) return;

    SDL_Rect panel={160,70,200,180};
    SDL_SetRenderDrawColor(g_app.renderer,12,12,28,235);
    SDL_RenderFillRect(g_app.renderer,&panel);

    // Border
    SDL_SetRenderDrawColor(g_app.renderer,90,90,140,255);
    SDL_Rect btop={panel.x-2,panel.y-2,panel.w+4,2}; SDL_RenderFillRect(g_app.renderer,&btop);
    SDL_Rect bbot={panel.x-2,panel.y+panel.h,panel.w+4,2}; SDL_RenderFillRect(g_app.renderer,&bbot);
    SDL_Rect bl={panel.x-2,panel.y,2,panel.h}; SDL_RenderFillRect(g_app.renderer,&bl);
    SDL_Rect br={panel.x+panel.w,panel.y,2,panel.h}; SDL_RenderFillRect(g_app.renderer,&br);

    // Header gradient
    SDL_SetRenderDrawColor(g_app.renderer,130,50,170,255);
    SDL_Rect hdr={panel.x,panel.y,panel.w,16}; SDL_RenderFillRect(g_app.renderer,&hdr);
    SDL_SetRenderDrawColor(g_app.renderer,180,80,220,255);
    SDL_Rect hdr2={panel.x,panel.y,panel.w/2,16}; SDL_RenderFillRect(g_app.renderer,&hdr2);
    rogue_font_draw_text(panel.x+6, panel.y+4, "STATS",1,(RogueColor){255,255,255,255});

    const char* labels[6]={"STR","DEX","VIT","INT","CRIT%","CRITDMG"};
    int values[6]={g_app.player.strength,g_app.player.dexterity,g_app.player.vitality,g_app.player.intelligence,g_app.player.crit_chance,g_app.player.crit_damage};
    for(int i=0;i<6;i++){
        int highlight=(i==g_app.stats_panel_index);
        char line[64];
        snprintf(line,sizeof line,"%s %3d%s", labels[i], values[i], highlight?" *":"");
        rogue_font_draw_text(panel.x+10, panel.y+22 + i*18, line,1,(RogueColor){ highlight?255:200, highlight?255:255, highlight?160:255,255});
        int barw = values[i]; if(i==5) barw=values[i]/4; if(barw>70) barw=70; if(barw<0) barw=0;
        SDL_SetRenderDrawColor(g_app.renderer,50,60,90,255);
        SDL_Rect bg={panel.x+10, panel.y+22 + i*18 + 10,72,4}; SDL_RenderFillRect(g_app.renderer,&bg);
        SDL_SetRenderDrawColor(g_app.renderer, highlight?255:140, highlight?200:140, highlight?90:160,255);
        SDL_Rect fg={panel.x+10, panel.y+22 + i*18 + 10, barw,4}; SDL_RenderFillRect(g_app.renderer,&fg);
    }

    char footer[96];
    snprintf(footer,sizeof footer,"PTS:%d  ENTER=+  TAB=Close", g_app.unspent_stat_points);
    rogue_font_draw_text(panel.x+6, panel.y+panel.h-14, footer,1,(RogueColor){180,220,255,255});
#endif
}
