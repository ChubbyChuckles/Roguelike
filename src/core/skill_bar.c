#include "core/skill_bar.h"
#include "core/app_state.h"
#include "core/skills.h"
#include "graphics/font.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <string.h>

void rogue_skill_bar_set_slot(int slot, int skill_id){ if(slot>=0 && slot<10) g_app.skill_bar[slot]=skill_id; }
int rogue_skill_bar_get_slot(int slot){ if(slot>=0 && slot<10) return g_app.skill_bar[slot]; return -1; }

void rogue_skill_bar_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return;
    int bar_w = 10*34 + 8; int bar_h = 46; int bar_x = 4; int bar_y = g_app.viewport_h - bar_h - 4;
    SDL_SetRenderDrawColor(g_app.renderer,20,20,32,210);
    SDL_Rect bg={bar_x,bar_y,bar_w,bar_h}; SDL_RenderFillRect(g_app.renderer,&bg);
    SDL_SetRenderDrawColor(g_app.renderer,80,80,120,255);
    SDL_Rect top={bar_x,bar_y,bar_w,2}; SDL_RenderFillRect(g_app.renderer,&top);
    for(int i=0;i<10;i++){
        int slot_x = bar_x + 6 + i*34;
        SDL_Rect cell={slot_x, bar_y+6, 32,32};
        int skill_id = g_app.skill_bar[i];
        const RogueSkillDef* def = rogue_skill_get_def(skill_id);
        SDL_SetRenderDrawColor(g_app.renderer, def? 60:30, def?60:30, def?80:30,255);
        SDL_RenderFillRect(g_app.renderer,&cell);
        if(def){
            char label[5];
            int rank = rogue_skill_get_state(skill_id)->rank;
            snprintf(label,sizeof label,"%d", rank);
            rogue_font_draw_text(cell.x+10, cell.y+10, label,1,(RogueColor){255,255,200,255});
        }
        char idx[3]; snprintf(idx,sizeof idx,"%d", (i+1)%10);
        rogue_font_draw_text(slot_x+10, bar_y+40, idx,1,(RogueColor){200,200,255,255});
    }
#endif
}
