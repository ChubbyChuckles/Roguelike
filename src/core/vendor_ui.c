#include "core/app_state.h"
#include "core/vendor.h"
#include "core/economy.h"
#include "graphics/font.h"
#include "core/inventory.h"
#include "core/loot_item_defs.h"
#include "core/equipment.h"
#include "core/loot_instances.h"
#include <stdio.h>
#include "core/stat_cache.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_vendor_panel_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.show_vendor_panel) return; if(!g_app.renderer) return;
    SDL_Rect panel={g_app.viewport_w-320,60,300,260};
    SDL_SetRenderDrawColor(g_app.renderer,20,20,32,240); SDL_RenderFillRect(g_app.renderer,&panel);
    SDL_SetRenderDrawColor(g_app.renderer,90,90,120,255); SDL_Rect br={panel.x-2,panel.y-2,panel.w+4,panel.h+4}; SDL_RenderDrawRect(g_app.renderer,&br);
    rogue_font_draw_text(panel.x+6,panel.y+4,"VENDOR",1,(RogueColor){255,255,210,255});
    int y=panel.y+24; int count=rogue_vendor_item_count();
    for(int i=0;i<count;i++){
        const RogueVendorItem* vi = rogue_vendor_get(i); if(!vi) continue; const RogueItemDef* d = rogue_item_def_at(vi->def_index); if(!d) continue;
        int price = rogue_econ_buy_price(vi);
        char line[128]; snprintf(line,sizeof line,"%c %s (%d) %dG", (i==g_app.vendor_selection)?'>':' ', d->name, vi->rarity, price);
        rogue_font_draw_text(panel.x+10,y,line,1,(RogueColor){ (i==g_app.vendor_selection)?255:200, (i==g_app.vendor_selection)?255:200, (i==g_app.vendor_selection)?160:200,255});
        y+=18; if(y>panel.y+panel.h-40) break; /* clip */
    }
    char footer[96]; snprintf(footer,sizeof footer,"Gold:%d  REP:%d  ENTER=Buy  V=Close", rogue_econ_gold(), rogue_econ_get_reputation());
    rogue_font_draw_text(panel.x+6,panel.y+panel.h-18,footer,1,(RogueColor){180,220,255,255});
#endif
}

void rogue_equipment_panel_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.show_equipment_panel) return; if(!g_app.renderer) return;
    SDL_Rect panel={g_app.viewport_w-320,330,300,160};
    SDL_SetRenderDrawColor(g_app.renderer,28,18,18,235); SDL_RenderFillRect(g_app.renderer,&panel);
    SDL_SetRenderDrawColor(g_app.renderer,120,60,60,255); SDL_Rect br={panel.x-2,panel.y-2,panel.w+4,panel.h+4}; SDL_RenderDrawRect(g_app.renderer,&br);
    rogue_font_draw_text(panel.x+6,panel.y+4,"EQUIPMENT",1,(RogueColor){255,230,230,255});
    rogue_font_draw_text(panel.x+10,panel.y+26,"Weapon Slot: (W)",1,(RogueColor){220,200,200,255});
    rogue_font_draw_text(panel.x+10,panel.y+44,"Armor Slot : (A)",1,(RogueColor){200,220,200,255});
    char stats[96]; snprintf(stats,sizeof stats,"STR:%d DEX:%d VIT:%d INT:%d", g_app.player.strength,g_app.player.dexterity,g_app.player.vitality,g_app.player.intelligence);
    rogue_font_draw_text(panel.x+10,panel.y+panel.h-56,stats,1,(RogueColor){255,255,180,255});
    int w_inst = rogue_equip_get(ROGUE_EQUIP_WEAPON); int cur=0,max=0; if(w_inst>=0) rogue_item_instance_get_durability(w_inst,&cur,&max);
    if(max>0){ char dur[64]; snprintf(dur,sizeof dur,"WEAPON DUR:%d/%d (R=Repair)", cur,max); rogue_font_draw_text(panel.x+10,panel.y+panel.h-40,dur,1,(RogueColor){220,200,255,255}); }
    char derived[96]; snprintf(derived,sizeof derived,"DPS:%d EHP:%d Gold:%d", g_player_stat_cache.dps_estimate, g_player_stat_cache.ehp_estimate, rogue_econ_gold());
    rogue_font_draw_text(panel.x+10,panel.y+panel.h-22,derived,1,(RogueColor){200,240,200,255});
#endif
}
