/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
/* Initialization & shutdown logic extracted from former app.c */
#include "core/app.h"
#include "core/app_state.h"
#include "core/game_loop.h"
#include "core/platform.h"
#include "core/metrics.h"
#include "core/animation_system.h"
#include "core/persistence.h"
#include "core/persistence_autosave.h"
#include "core/asset_config.h"
#include "core/skills.h"
#include "core/skill_tree.h"
#include "core/skill_bar.h"
#include "core/buffs.h"
#include "core/projectiles.h"
#include "core/projectiles_config.h"
#include "core/loot_item_defs.h"
#include "core/loot_tables.h"
#include "core/loot_instances.h"
#include "core/inventory.h"
#include "core/loot_logging.h"
#include "core/equipment_stats.h"
#include "core/stat_cache.h"
#include "core/vendor.h"
#include "core/economy.h"
#include "core/tile_sprite_cache.h"
#include "core/vegetation.h"
#include "world/world_gen.h"
#include "world/world_gen_config.h"
#include "entities/player.h"
#include "entities/enemy.h"
#include "game/combat.h"
#include "util/log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#ifdef ROGUE_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

bool rogue_app_init(const RogueAppConfig* cfg)
{
    g_app.cfg = *cfg;
    g_app.show_start_screen = 1;
    rogue_input_clear(&g_app.input);
    g_app.title_time = 0.0;
    g_app.menu_index = 0;
    g_app.entering_seed = 0;
    g_app.pending_seed = 1337u;
    rogue_player_init(&g_app.player);
    g_app.unspent_stat_points = 0;
    if(!rogue_platform_init(cfg)) return false;
    RogueGameLoopConfig loop_cfg = {.target_fps = cfg->target_fps};
    rogue_game_loop_init(&loop_cfg);
    rogue_persistence_load_generation_params();
    rogue_skills_init(); rogue_buffs_init(); rogue_projectiles_init();
    rogue_projectiles_config_load_and_watch("assets/projectiles.cfg");
    rogue_skill_tree_register_baseline();
    rogue_item_defs_reset();
    rogue_loot_tables_reset();
    rogue_items_init_runtime();
    rogue_inventory_init();
    rogue_loot_logging_init_from_env();
     rogue_persistence_load_player_stats();
     /* Cheat override: always start session with 100 talent points (do not reset while spending).
         Applied AFTER loading persistence so prior saves don't reduce this. */
     g_app.talent_points = 100;
     g_app.stats_dirty = 1; /* ensure any dependent stat recalculations happen */
    RogueWorldGenConfig wcfg = rogue_world_gen_config_build(1337u, 1, 1);
    rogue_world_generate(&g_app.world_map, &wcfg);
    rogue_vegetation_init();
    rogue_vegetation_load_defs("assets/plants.cfg","assets/trees.cfg");
    rogue_vegetation_generate(0.12f, 1337u);
    rogue_vegetation_set_trunk_collision_enabled(1);
    rogue_vegetation_set_canopy_tile_blocking_enabled(0);
    g_exposed_player_for_stats = g_app.player;
    g_app.stats_dirty = 0; g_app.show_stats_panel = 0; g_app.stats_panel_index = 0;
    g_app.time_since_player_hit_ms = 0.0f; g_app.health_regen_accum_ms = 0.0f; g_app.mana_regen_accum_ms = 0.0f; g_app.ap_regen_accum_ms = 0.0f; g_app.levelup_aura_timer_ms = 0.0f;
    g_app.dmg_number_count = 0; g_app.spawn_accum_ms = 700.0;
    g_app.show_vendor_panel = 0; g_app.vendor_selection = 0; g_app.vendor_seed = 424242u; g_app.vendor_time_accum_ms = 0.0; g_app.vendor_restock_interval_ms = 30000.0; g_app.vendor_x = 4.5f; g_app.vendor_y = 4.5f; g_app.show_equipment_panel = 0;
    int items_loaded = rogue_item_defs_load_directory("assets/items");
    if(items_loaded <= 0){ items_loaded = rogue_item_defs_load_from_cfg("assets/test_items.cfg"); }
    int tables_loaded = rogue_loot_tables_load_from_cfg("assets/test_loot_tables.cfg");
    if(tables_loaded > 0){
        RogueGenerationContext vctx = { .enemy_level = g_app.player.level, .biome_id = 0, .enemy_archetype = 0, .player_luck = 0 };
        unsigned int seed = g_app.vendor_seed; rogue_vendor_reset(); rogue_vendor_generate_inventory(0,8,&vctx,&seed); g_app.vendor_seed = seed + 17u;
    }
    rogue_econ_add_gold(250);
#ifdef ROGUE_HAVE_SDL_MIXER
    g_app.sfx_levelup = NULL;
    if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512)!=0){
        ROGUE_LOG_WARN("Mix_OpenAudio failed: %s", Mix_GetError());
    } else { rogue_asset_load_sounds(); }
#endif
    g_app.tileset_loaded = 0; g_app.tile_size = 16; g_app.player_frame_size = 64; g_app.player_loaded = 0; g_app.player_state = 0; g_app.player_sheet_paths_loaded = 0;
    g_app.cam_x = 0.0f; g_app.cam_y = 0.0f;
    g_app.viewport_w = cfg->logical_width ? cfg->logical_width : cfg->window_width;
    g_app.viewport_h = cfg->logical_height ? cfg->logical_height : cfg->window_height;
    g_app.walk_speed = 45.0f; g_app.run_speed  = 85.0f;
    g_app.tile_sprite_lut = NULL; g_app.tile_sprite_lut_ready = 0; g_app.minimap_dirty = 1; g_app.minimap_w = g_app.minimap_h = 0; g_app.minimap_step = 1;
    g_app.chunk_size = 32; g_app.chunks_x = g_app.chunks_y = 0; g_app.chunk_dirty = NULL;
    g_app.anim_dt_accum_ms = 0.0f; g_app.frame_draw_calls = 0; g_app.frame_tile_quads = 0;
    rogue_combat_init(&g_app.player_combat);
    g_app.enemy_count = 0; g_app.total_kills = 0;
    g_app.enemy_type_count = ROGUE_MAX_ENEMY_TYPES;
    if(!rogue_enemy_load_config("assets/enemies.cfg", g_app.enemy_types, &g_app.enemy_type_count) || g_app.enemy_type_count<=0){
        ROGUE_LOG_WARN("No enemy types loaded; injecting fallback dummy type.");
        g_app.enemy_type_count = 1; RogueEnemyTypeDef* t = &g_app.enemy_types[0]; memset(t,0,sizeof *t);
#if defined(_MSC_VER)
        strncpy_s(t->name,sizeof t->name,"dummy",_TRUNCATE);
#else
        strncpy(t->name,"dummy",sizeof t->name -1); t->name[sizeof t->name -1]='\0';
#endif
        t->group_min=1; t->group_max=2; t->patrol_radius=5; t->aggro_radius=6; t->speed=30.0f; t->pop_target=15; t->xp_reward=2; t->loot_chance=0.05f;
    } else { ROGUE_LOG_INFO("Loaded %d enemy types", g_app.enemy_type_count); }
    for(int i=0;i<ROGUE_MAX_ENEMY_TYPES;i++){ g_app.per_type_counts[i]=0; }
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++){ g_app.enemies[i].alive=0; }
    {
        char cwd_buf[512];
#ifdef _WIN32
        if(_getcwd(cwd_buf, sizeof cwd_buf)){ ROGUE_LOG_INFO("CWD: %s", cwd_buf); }
        else { ROGUE_LOG_WARN("Could not determine CWD"); }
#else
        if(getcwd(cwd_buf, sizeof cwd_buf)){ ROGUE_LOG_INFO("CWD: %s", cwd_buf); }
        else { ROGUE_LOG_WARN("Could not determine CWD"); }
#endif
    }
    return true;
}

void rogue_app_shutdown(void)
{
#ifdef ROGUE_HAVE_SDL_MIXER
    if(g_app.sfx_levelup){ Mix_FreeChunk(g_app.sfx_levelup); g_app.sfx_levelup=NULL; }
    Mix_CloseAudio();
#endif
    rogue_platform_shutdown();
    rogue_skills_shutdown();
    if(g_app.chunk_dirty){ free(g_app.chunk_dirty); g_app.chunk_dirty=NULL; }
    rogue_tile_sprite_cache_free();
    rogue_persistence_save_on_shutdown();
}
