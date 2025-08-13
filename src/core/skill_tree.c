#include "core/skill_tree.h"
#include "core/app_state.h"
#include "core/skills.h"
#include "graphics/font.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <stdio.h>

static int g_skill_tree_open = 0;
static int g_tree_index = 0; /* selection */

/* Example effect: increases player strength per activation (placeholder) */
static int effect_power_strike(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx){ (void)def;(void)st;(void)ctx; /* integrate with combat later */ return 1; }
static int effect_dash(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx){ (void)def;(void)st;(void)ctx; return 1; }
static int effect_fireball(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx){ (void)def;(void)st;(void)ctx; return 1; }

void rogue_skill_tree_register_baseline(void){
    RogueSkillDef defs[] = {
        {-1,"PowerStrike","icon_power",5, 2500,150,effect_power_strike},
        {-1,"Dash","icon_dash",3,  5000,500,effect_dash},
        {-1,"Fireball","icon_fire",5, 6000,400,effect_fireball},
        {-1,"IceNova","icon_ice",4, 7500,500,NULL},
        {-1,"Berserk","icon_berserk",3, 12000,1000,NULL},
    };
    for(unsigned i=0;i<sizeof(defs)/sizeof(defs[0]);++i){ rogue_skill_register(&defs[i]); }
    /* default bar assignment first 3 skills */
    for(int i=0;i<10;i++) g_app.skill_bar[i]=-1;
    if(g_app.skill_count>0) g_app.skill_bar[0]=0;
    if(g_app.skill_count>1) g_app.skill_bar[1]=1;
    if(g_app.skill_count>2) g_app.skill_bar[2]=2;
}

void rogue_skill_tree_toggle(void){ g_skill_tree_open = !g_skill_tree_open; }
int rogue_skill_tree_is_open(void){ return g_skill_tree_open; }

void rogue_skill_tree_handle_key(int sym){
#ifdef ROGUE_HAVE_SDL
    if(!g_skill_tree_open) return;
    if(sym==SDLK_LEFT){ g_tree_index = (g_tree_index + g_app.skill_count -1)%g_app.skill_count; }
    else if(sym==SDLK_RIGHT){ g_tree_index = (g_tree_index +1)%g_app.skill_count; }
    else if(sym==SDLK_UP || sym==SDLK_DOWN){ /* swap slot with next empty? placeholder */ }
    else if(sym==SDLK_RETURN){ rogue_skill_rank_up(g_tree_index); }
    else if(sym==SDLK_ESCAPE || sym==SDLK_TAB){ g_skill_tree_open=0; }
#endif
}

void rogue_skill_tree_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_skill_tree_open) return;
    if(!g_app.renderer) return;
    int panel_w = 340; int panel_h = 140; int px = (g_app.viewport_w - panel_w)/2; int py = g_app.viewport_h - panel_h - 90;
    SDL_SetRenderDrawColor(g_app.renderer,18,12,32,230);
    SDL_Rect bg={px,py,panel_w,panel_h}; SDL_RenderFillRect(g_app.renderer,&bg);
    SDL_SetRenderDrawColor(g_app.renderer,90,60,140,255);
    SDL_Rect top={px,py,panel_w,18}; SDL_RenderFillRect(g_app.renderer,&top);
    rogue_font_draw_text(px+6, py+4, "SKILL TREE",1,(RogueColor){255,255,255,255});
    int icon_size=40; int spacing=12; int start_x = px+14; int y=py+36;
    for(int i=0;i<g_app.skill_count;i++){
        int sel = (i==g_tree_index);
        int ix = start_x + i*(icon_size+spacing);
        SDL_Rect cell={ix,y,icon_size,icon_size};
        SDL_SetRenderDrawColor(g_app.renderer, sel?140:60, sel?90:60, sel?40:80,255);
        SDL_RenderFillRect(g_app.renderer,&cell);
        const RogueSkillDef* def = rogue_skill_get_def(i);
        const RogueSkillState* st = rogue_skill_get_state(i);
        if(def){
            rogue_font_draw_text(ix+4, y+4, def->name,0,(RogueColor){255,255,220,255});
            char rb[16]; snprintf(rb,sizeof rb,"%d/%d", st->rank, def->max_rank);
            rogue_font_draw_text(ix+10, y+20, rb,1,(RogueColor){220,255,255,255});
        }
    }
    char pts[64]; snprintf(pts,sizeof pts,"Talent Points: %d (ENTER to rank up)", g_app.talent_points);
    rogue_font_draw_text(px+10, py+panel_h-20, pts,1,(RogueColor){255,255,160,255});
#endif
}
