#include "core/input_events.h"
#include "core/game_loop.h"
#include "world/world_gen.h"
#include "world/tilemap.h"
#include "entities/player.h"
#include "core/start_screen.h"
#include "game/damage_numbers.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_process_events(void){
#ifdef ROGUE_HAVE_SDL
    SDL_Event ev; while(SDL_PollEvent(&ev)){
        if(ev.type==SDL_QUIT){ rogue_game_loop_request_exit(); }
        rogue_input_process_sdl_event(&g_app.input,&ev);
        if(ev.type==SDL_KEYDOWN && !g_app.show_start_screen){
            if(ev.key.keysym.sym==SDLK_TAB){ g_app.show_stats_panel=!g_app.show_stats_panel; }
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
                RogueWorldGenConfig wcfg={ .seed=g_app.pending_seed,.width=80,.height=60,.biome_regions=10,.cave_iterations=3,.cave_fill_chance=0.45,.river_attempts=2,.small_island_max_size=3,.small_island_passes=2,.shore_fill_passes=1,.advanced_terrain=1,.water_level=g_app.gen_water_level,.noise_octaves=g_app.gen_noise_octaves,.noise_gain=g_app.gen_noise_gain,.noise_lacunarity=g_app.gen_noise_lacunarity,.river_sources=g_app.gen_river_sources,.river_max_length=g_app.gen_river_max_length,.cave_mountain_elev_thresh=g_app.gen_cave_thresh};
                wcfg.width*=10; wcfg.height*=10; wcfg.biome_regions=1000;
                rogue_tilemap_free(&g_app.world_map); rogue_world_generate(&g_app.world_map,&wcfg); g_app.minimap_dirty=1;
            }
        }
        if(ev.type==SDL_KEYDOWN && g_app.show_start_screen){
            if(g_app.entering_seed){
                if(ev.key.keysym.sym==SDLK_RETURN){
                    rogue_tilemap_free(&g_app.world_map);
                    RogueWorldGenConfig wcfg={ .seed=g_app.pending_seed,.width=80,.height=60,.biome_regions=10,.cave_iterations=3,.cave_fill_chance=0.45,.river_attempts=2,.small_island_max_size=3,.small_island_passes=2,.shore_fill_passes=1,.advanced_terrain=1,.water_level=0.34,.noise_octaves=6,.noise_gain=0.48,.noise_lacunarity=2.05,.river_sources=10,.river_max_length=1200,.cave_mountain_elev_thresh=0.60};
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
