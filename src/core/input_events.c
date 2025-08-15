#include "core/input_events.h"
#include "core/game_loop.h"
#include "world/world_gen.h"
#include "world/world_gen_config.h"
#include "world/tilemap.h"
#include "entities/player.h"
#include "core/start_screen.h"
#include "game/damage_numbers.h"
#include "core/skill_tree.h"
#include "core/skill_bar.h"
#include "core/skills.h"
#include "core/vegetation.h"
#include "core/vendor.h"
#include "core/economy.h"
#include "core/inventory.h"
#include "core/equipment.h"
#include <string.h> /* memset */
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* Simple ring buffer for deferred skill activations so that skills (e.g., projectile spawns) use the post-movement player position. */
#define ROGUE_PENDING_SKILLS_MAX 32
typedef struct PendingSkillAct { int skill_id; int bar_slot; double now_ms; } PendingSkillAct;
static PendingSkillAct g_pending_skill_acts[ROGUE_PENDING_SKILLS_MAX];
static int g_pending_skill_head = 0; /* next write */
static int g_pending_skill_count = 0;

static void queue_skill_activation(int sid, int slot){
    if(sid<0) return;
    if(g_pending_skill_count >= ROGUE_PENDING_SKILLS_MAX) return; /* drop oldest silently */
    int idx = (g_pending_skill_head + g_pending_skill_count) % ROGUE_PENDING_SKILLS_MAX;
    g_pending_skill_acts[idx].skill_id = sid;
    g_pending_skill_acts[idx].bar_slot = slot;
#ifdef ROGUE_HAVE_SDL
    g_pending_skill_acts[idx].now_ms = (double)SDL_GetTicks();
#else
    g_pending_skill_acts[idx].now_ms = 0.0;
#endif
    g_pending_skill_count++;
}

void rogue_process_events(void){
#ifdef ROGUE_HAVE_SDL
    SDL_Event ev; while(SDL_PollEvent(&ev)){
        if(ev.type==SDL_QUIT){ rogue_game_loop_request_exit(); }
        rogue_input_process_sdl_event(&g_app.input,&ev);
    if(ev.type==SDL_KEYDOWN && !g_app.show_start_screen){
            if(rogue_skill_tree_is_open()){ rogue_skill_tree_handle_key(ev.key.keysym.sym); continue; }
            if(ev.key.keysym.sym==SDLK_TAB){ g_app.show_stats_panel=!g_app.show_stats_panel; }
            if(ev.key.keysym.sym==SDLK_v){ g_app.show_vendor_panel = !g_app.show_vendor_panel; g_app.vendor_selection = 0; }
            if(ev.key.keysym.sym==SDLK_e){ g_app.show_equipment_panel = !g_app.show_equipment_panel; }
            if(g_app.show_equipment_panel && ev.key.keysym.sym==SDLK_r){ /* repair equipped weapon */
                rogue_equip_repair_slot(ROGUE_EQUIP_WEAPON);
            }
            if(g_app.show_vendor_panel){
                if(ev.key.keysym.sym==SDLK_UP){ g_app.vendor_selection--; if(g_app.vendor_selection<0) g_app.vendor_selection=rogue_vendor_item_count()>0?rogue_vendor_item_count()-1:0; }
                if(ev.key.keysym.sym==SDLK_DOWN){ g_app.vendor_selection++; if(g_app.vendor_selection>=rogue_vendor_item_count()) g_app.vendor_selection=0; }
                if(ev.key.keysym.sym==SDLK_RETURN){
                    const RogueVendorItem* vi = rogue_vendor_get(g_app.vendor_selection);
                    if(vi){
                        int price = rogue_econ_buy_price(vi);
                        if(!g_app.vendor_confirm_active){
                            /* open confirmation modal */
                            g_app.vendor_confirm_active=1; g_app.vendor_confirm_def_index=vi->def_index; g_app.vendor_confirm_price=price; g_app.vendor_insufficient_flash_ms=0.0;
                        } else {
                            /* Accept inside modal */
                            if(rogue_econ_gold() >= g_app.vendor_confirm_price){
                                if(rogue_econ_try_buy(vi)==0){ rogue_inventory_add(vi->def_index,1); }
                                g_app.vendor_confirm_active=0;
                            } else {
                                g_app.vendor_insufficient_flash_ms = 480.0; /* flash for ~0.5s */
                            }
                        }
                    }
                }
                if(ev.key.keysym.sym==SDLK_ESCAPE && g_app.vendor_confirm_active){ g_app.vendor_confirm_active=0; }
                if(ev.key.keysym.sym==SDLK_BACKSPACE){ g_app.show_vendor_panel=0; }
            }
            if(ev.key.keysym.sym==SDLK_k){ rogue_skill_tree_toggle(); }
            if(g_app.show_stats_panel){
                if(ev.key.keysym.sym==SDLK_LEFT){ g_app.stats_panel_index=(g_app.stats_panel_index+5)%6; }
                if(ev.key.keysym.sym==SDLK_RIGHT){ g_app.stats_panel_index=(g_app.stats_panel_index+1)%6; }
                if(ev.key.keysym.sym==SDLK_RETURN && g_app.unspent_stat_points>0){
                    if(g_app.stats_panel_index==0){ g_app.player.strength++; }
                    else if(g_app.stats_panel_index==1){ g_app.player.dexterity++; }
                    else if(g_app.stats_panel_index==2){ g_app.player.vitality++; rogue_player_recalc_derived(&g_app.player); }
                    else if(g_app.stats_panel_index==3){ g_app.player.intelligence++; }
                    else if(g_app.stats_panel_index==4){ g_app.player.crit_chance++; if(g_app.player.crit_chance>100) g_app.player.crit_chance=100; }
                    else if(g_app.stats_panel_index==5){ g_app.player.crit_damage+=5; if(g_app.player.crit_damage>400) g_app.player.crit_damage=400; }
                    g_app.unspent_stat_points--; g_app.stats_dirty=1; }
                if(ev.key.keysym.sym==SDLK_BACKSPACE){ g_app.show_stats_panel=0; }
            }
            if(ev.key.keysym.sym==SDLK_r){ g_app.player_state=(g_app.player_state==2)?1:2; }
            /* Skill activation keys 1-0 */
            int key = ev.key.keysym.sym;
            if(key>=SDLK_1 && key<=SDLK_9){ int slot = key - SDLK_1; int sid = g_app.skill_bar[slot]; if(sid>=0){ queue_skill_activation(sid, slot); } }
            if(key==SDLK_0){ int sid = g_app.skill_bar[9]; if(sid>=0){ queue_skill_activation(sid, 9); } }
            if(ev.key.keysym.sym==SDLK_F5){ g_app.gen_water_level -= 0.01; if(g_app.gen_water_level<0.20) g_app.gen_water_level=0.20; g_app.gen_params_dirty=1; ev.key.keysym.sym=SDLK_BACKQUOTE; }
            if(ev.key.keysym.sym==SDLK_F6){ g_app.gen_water_level += 0.01; if(g_app.gen_water_level>0.55) g_app.gen_water_level=0.55; g_app.gen_params_dirty=1; ev.key.keysym.sym=SDLK_BACKQUOTE; }
            if(ev.key.keysym.sym==SDLK_F7){ g_app.gen_noise_octaves++; if(g_app.gen_noise_octaves>9) g_app.gen_noise_octaves=9; g_app.gen_params_dirty=1; ev.key.keysym.sym=SDLK_BACKQUOTE; }
            if(ev.key.keysym.sym==SDLK_F8){ g_app.gen_noise_octaves--; if(g_app.gen_noise_octaves<3) g_app.gen_noise_octaves=3; g_app.gen_params_dirty=1; ev.key.keysym.sym=SDLK_BACKQUOTE; }
            if(ev.key.keysym.sym==SDLK_F9){ g_app.gen_river_sources+=2; if(g_app.gen_river_sources>40) g_app.gen_river_sources=40; g_app.gen_params_dirty=1; ev.key.keysym.sym=SDLK_BACKQUOTE; }
            if(ev.key.keysym.sym==SDLK_F10){ g_app.gen_river_sources-=2; if(g_app.gen_river_sources<2) g_app.gen_river_sources=2; g_app.gen_params_dirty=1; ev.key.keysym.sym=SDLK_BACKQUOTE; }
            if(ev.key.keysym.sym==SDLK_F11){ g_app.gen_noise_gain += 0.02; if(g_app.gen_noise_gain>0.8) g_app.gen_noise_gain=0.8; g_app.gen_params_dirty=1; ev.key.keysym.sym=SDLK_BACKQUOTE; }
            if(ev.key.keysym.sym==SDLK_F12){ g_app.gen_noise_gain -= 0.02; if(g_app.gen_noise_gain<0.3) g_app.gen_noise_gain=0.3; g_app.gen_params_dirty=1; ev.key.keysym.sym=SDLK_BACKQUOTE; }
            if(ev.key.keysym.sym==SDLK_BACKQUOTE){
                g_app.pending_seed=(unsigned int)SDL_GetTicks();
                RogueWorldGenConfig wcfg = rogue_world_gen_config_build(g_app.pending_seed, 1, 1);
                rogue_tilemap_free(&g_app.world_map); rogue_world_generate(&g_app.world_map,&wcfg); g_app.minimap_dirty=1;
                /* Regenerate vegetation with same cover and new seed */
                rogue_vegetation_generate(rogue_vegetation_get_tree_cover(), g_app.pending_seed);
            }
            /* Vegetation density adjustments: Alt+[ decreases, Alt+] increases */
            const Uint8* ks = SDL_GetKeyboardState(NULL);
            int alt_down = ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_RALT];
            if(alt_down && ev.key.keysym.sym==SDLK_LEFTBRACKET){ float c=rogue_vegetation_get_tree_cover(); c-=0.02f; if(c<0.0f) c=0.0f; rogue_vegetation_set_tree_cover(c); }
            if(alt_down && ev.key.keysym.sym==SDLK_RIGHTBRACKET){ float c=rogue_vegetation_get_tree_cover(); c+=0.02f; if(c>0.70f) c=0.70f; rogue_vegetation_set_tree_cover(c); }
        }
        if(ev.type==SDL_KEYDOWN && g_app.show_start_screen){
            if(g_app.entering_seed){
                if(ev.key.keysym.sym==SDLK_RETURN){
                    rogue_tilemap_free(&g_app.world_map);
                    RogueWorldGenConfig wcfg = rogue_world_gen_config_build(g_app.pending_seed, 0, 0);
                    rogue_world_generate(&g_app.world_map,&wcfg);
                    g_app.chunks_x=(g_app.world_map.width + g_app.chunk_size - 1)/g_app.chunk_size;
                    g_app.chunks_y=(g_app.world_map.height + g_app.chunk_size - 1)/g_app.chunk_size;
                    size_t ctotal=(size_t)g_app.chunks_x * (size_t)g_app.chunks_y; if(ctotal){ g_app.chunk_dirty=(unsigned char*)malloc(ctotal); if(g_app.chunk_dirty) memset(g_app.chunk_dirty,0,ctotal);} g_app.entering_seed=0;
                } else if(ev.key.keysym.sym==SDLK_ESCAPE){ g_app.entering_seed=0; }
            }
        }
    }
#endif
}

void rogue_process_pending_skill_activations(void){
    /* Consume queued activations in FIFO order */
    int processed = 0;
    while(g_pending_skill_count>0){
        int idx = g_pending_skill_head;
        PendingSkillAct *pa = &g_pending_skill_acts[idx];
        RogueSkillCtx ctx = { pa->now_ms, g_app.player.level, g_app.talent_points };
        if(rogue_skill_try_activate(pa->skill_id, &ctx)){
            if(pa->bar_slot>=0 && pa->bar_slot<10) rogue_skill_bar_flash(pa->bar_slot);
        }
        g_pending_skill_head = (g_pending_skill_head + 1) % ROGUE_PENDING_SKILLS_MAX;
        g_pending_skill_count--; processed++;
    }
    (void)processed; /* could log for debugging */
}
