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
#include "core/app.h"
#include "core/app_state.h"
#include "core/game_loop.h"
#include "util/log.h"
#include "graphics/font.h"
#include "world/world_gen.h"
#include "world/world_gen_config.h"
#include "input/input.h"
#include "entities/player.h"
#include "entities/enemy.h"
#include "game/combat.h"
#include "game/damage_numbers.h"
#include "core/enemy_system.h"
#include "core/vegetation.h"
#include "core/start_screen.h"
#include "core/player_assets.h"
#include "core/player_controller.h"
#include "core/minimap.h"
#include "graphics/sprite.h"
#include "graphics/tile_sprites.h"
#include "core/scene_drawlist.h"
#include "core/input_events.h"
#include "core/player_render.h"
#include "core/enemy_render.h"
#include "core/hud.h"
#include "core/player_progress.h"
#include "core/persistence_autosave.h"
#include "game/damage_numbers.h" /* will provide render/update */
#include "core/world_renderer.h"
#include "core/animation_system.h"
#include "core/persistence.h"
#include "core/asset_config.h"
#include "core/metrics.h"
#include "core/platform.h"
#include "core/tile_sprite_cache.h"
#include "core/skills.h"
#include "core/skill_tree.h"
#include "core/skill_bar.h"
#include "core/buffs.h"
#include "core/projectiles.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

/* timing now handled by metrics module */

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* state now in app_state.c */

/* window mode logic moved to platform.c */

int rogue_get_current_attack_frame(void){
    /* Return current player anim frame if attack state active, else 0 */
    if(g_app.player_combat.phase==ROGUE_ATTACK_WINDUP || g_app.player_combat.phase==ROGUE_ATTACK_STRIKE || g_app.player_combat.phase==ROGUE_ATTACK_RECOVER){
        return g_app.player.anim_frame;
    }
    return 0;
}


/* tile collision helper moved to player_controller */

int rogue_app_player_health(void){ return g_app.player.health; }

RogueEnemy* rogue_test_spawn_hostile_enemy(float x, float y){
    if(g_app.enemy_type_count<=0) return NULL;
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++) if(!g_app.enemies[i].alive){
        RogueEnemy* ne = &g_app.enemies[i];
        /* Spawn relative to player position so tests can provide small offsets independent of world gen spawn location */
        float px = g_app.player.base.pos.x + x;
        float py = g_app.player.base.pos.y + y;
        ne->base.pos.x = px; ne->base.pos.y = py; ne->anchor_x = px; ne->anchor_y = py;
    ne->patrol_target_x = px; ne->patrol_target_y = py; /* keep stationary for deterministic DPS timing */
        ne->max_health = 10; ne->health = 10; ne->alive=1; ne->hurt_timer=0; ne->anim_time=0; ne->anim_frame=0;
    ne->ai_state = ROGUE_ENEMY_AI_AGGRO; ne->facing=2; ne->type_index=0; ne->tint_r=255; ne->tint_g=255; ne->tint_b=255; ne->death_fade=1.0f; ne->tint_phase=0; ne->flash_timer=0; ne->attack_cooldown_ms=0; /* immediate first attack allowed */
    ne->crit_chance = 5; ne->crit_damage = 25;
        g_app.enemy_count++; g_app.per_type_counts[0]++;
        return ne;
    }
    return NULL;
}

/* damage number functions moved to damage_numbers.c */

/* Public helper to add hitstop (clamped) */
void rogue_app_add_hitstop(float ms){
    if(ms < 0) return;
    if(ms > 180.0f) ms = 180.0f;
    if(g_app.hitstop_timer_ms < ms) g_app.hitstop_timer_ms = ms; /* take longer value */
}

/* frame timing helpers moved to metrics.c */

bool rogue_app_init(const RogueAppConfig* cfg)
{
    g_app.cfg = *cfg;
    g_app.show_start_screen = 1;
    rogue_input_clear(&g_app.input);
    g_app.title_time = 0.0;
    g_app.menu_index = 0;
    g_app.entering_seed = 0;
    g_app.pending_seed = 1337u;
    /* Initialize player with defaults FIRST so persistence load overwrites instead of being clobbered. */
    rogue_player_init(&g_app.player);
    /* Default unspent stat points (will be overwritten if persistence file exists). */
    g_app.unspent_stat_points = 0;
    if(!rogue_platform_init(cfg)) return false;
    RogueGameLoopConfig loop_cfg = {.target_fps = cfg->target_fps};
    rogue_game_loop_init(&loop_cfg);
    /* Load generation params first (does not depend on skills). */
    rogue_persistence_load_generation_params();
    /* Initialize and register skills BEFORE loading player stats so rank data in the save file can map onto registered skills. */
    rogue_skills_init(); rogue_buffs_init(); rogue_projectiles_init();
    rogue_skill_tree_register_baseline();
    /* Now load player stats (level/xp + talent points + skill ranks). */
    rogue_persistence_load_player_stats();
    /* Now generate world using (possibly loaded) generation parameters. */
    RogueWorldGenConfig wcfg = rogue_world_gen_config_build(1337u, 1, 1);
    rogue_world_generate(&g_app.world_map, &wcfg);
    /* Vegetation definitions & initial generation */
    rogue_vegetation_init();
    rogue_vegetation_load_defs("assets/plants.cfg","assets/trees.cfg");
    rogue_vegetation_generate(0.12f, 1337u);
    /* Expose player snapshot for HUD/stats panel */
    g_exposed_player_for_stats = g_app.player;
    g_app.stats_dirty = 0;
    g_app.show_stats_panel = 0;
    g_app.stats_panel_index = 0;
    g_app.time_since_player_hit_ms = 0.0f;
    g_app.health_regen_accum_ms = 0.0f;
    g_app.mana_regen_accum_ms = 0.0f;
    g_app.levelup_aura_timer_ms = 0.0f;
    g_app.dmg_number_count = 0; g_app.spawn_accum_ms = 700.0; /* start beyond threshold so first spawn attempt happens next frame */
/* Audio (level-up SFX) */
#ifdef ROGUE_HAVE_SDL_MIXER
    g_app.sfx_levelup = NULL;
    if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512)!=0){
        ROGUE_LOG_WARN("Mix_OpenAudio failed: %s", Mix_GetError());
    } else { rogue_asset_load_sounds(); }
#endif
    g_app.tileset_loaded = 0; /* registry not finalized yet */
    g_app.tile_size = 16; /* terrain tiles */
    g_app.player_frame_size = 64;
    g_app.player_loaded = 0;
    g_app.player_state = 0;
    g_app.player_sheet_paths_loaded = 0;
    g_app.cam_x = 0.0f; g_app.cam_y = 0.0f;
    g_app.viewport_w = cfg->logical_width ? cfg->logical_width : cfg->window_width;
    g_app.viewport_h = cfg->logical_height ? cfg->logical_height : cfg->window_height;
    g_app.walk_speed = 45.0f;  /* increased from 3.0 */
    g_app.run_speed  = 85.0f;  /* increased from 6.0 */
    g_app.tile_sprite_lut = NULL;
    g_app.tile_sprite_lut_ready = 0;
    g_app.minimap_dirty = 1;
    g_app.minimap_w = g_app.minimap_h = 0; g_app.minimap_step = 1;
#ifdef ROGUE_HAVE_SDL
    g_app.minimap_tex = NULL;
#endif
    g_app.chunk_size = 32; g_app.chunks_x = g_app.chunks_y = 0; g_app.chunk_dirty = NULL;
    g_app.anim_dt_accum_ms = 0.0f;
    g_app.frame_draw_calls = 0;
    g_app.frame_tile_quads = 0;
    /* Values already loaded (or defaults applied) above */
    /* Init combat */
    rogue_combat_init(&g_app.player_combat);
    g_app.enemy_count = 0; g_app.total_kills = 0;
    g_app.enemy_type_count = ROGUE_MAX_ENEMY_TYPES; /* capacity passed to loader */
    if(!rogue_enemy_load_config("assets/enemies.cfg", g_app.enemy_types, &g_app.enemy_type_count) || g_app.enemy_type_count<=0){
    ROGUE_LOG_WARN("No enemy types loaded; injecting fallback dummy type.");
    g_app.enemy_type_count = 1;
    RogueEnemyTypeDef* t = &g_app.enemy_types[0]; memset(t,0,sizeof *t);
#if defined(_MSC_VER)
    strncpy_s(t->name,sizeof t->name,"dummy",_TRUNCATE);
#else
    strncpy(t->name,"dummy",sizeof t->name -1); t->name[sizeof t->name -1]='\0';
#endif
    t->group_min=1; t->group_max=2; t->patrol_radius=5; t->aggro_radius=6; t->speed=30.0f; t->pop_target=15; t->xp_reward=2; t->loot_chance=0.05f;
    } else {
    ROGUE_LOG_INFO("Loaded %d enemy types", g_app.enemy_type_count);
    }
    /* Ensure per-type population counters start at zero (uninitialized memory previously blocked spawns) */
    for(int i=0;i<ROGUE_MAX_ENEMY_TYPES;i++){ g_app.per_type_counts[i]=0; }
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++){ g_app.enemies[i].alive=0; }
    /* Log current working directory for asset path debugging */
    {
        char cwd_buf[512];
#ifdef _WIN32
        if(_getcwd(cwd_buf, sizeof cwd_buf)){
            ROGUE_LOG_INFO("CWD: %s", cwd_buf);
        } else { ROGUE_LOG_WARN("Could not determine CWD"); }
#else
        if(getcwd(cwd_buf, sizeof cwd_buf)){
            ROGUE_LOG_INFO("CWD: %s", cwd_buf);
        } else { ROGUE_LOG_WARN("Could not determine CWD"); }
#endif
    }
    return true;
}

/* input event loop moved to input_events.c */

/* minimap helpers moved to minimap.c */

/* Map tokens to indices */
static int state_name_to_index(const char* s){ if(strcmp(s,"idle")==0) return 0; if(strcmp(s,"walk")==0) return 1; if(strcmp(s,"run")==0) return 2; if(strcmp(s,"attack")==0) return 3; return -1; }
static int dir_name_to_index(const char* d){
    if(strcmp(d,"down")==0) return 0;
    if(strcmp(d,"left")==0) return 1; /* treat left/right separately if provided */
    if(strcmp(d,"right")==0) return 2;
    if(strcmp(d,"side")==0) return 1; /* side covers left+right */
    if(strcmp(d,"up")==0) return 3;
    return -1;
}

void rogue_app_step(void)
{
    if (!g_game_loop.running)
        return;
    rogue_process_events();
    double frame_start = rogue_metrics_frame_begin();
#ifdef ROGUE_HAVE_SDL
    g_app.title_time += g_app.dt;
    SDL_SetRenderDrawColor(g_app.renderer, g_app.cfg.background_color.r, g_app.cfg.background_color.g,
                           g_app.cfg.background_color.b, g_app.cfg.background_color.a);
    SDL_RenderClear(g_app.renderer);
    if (rogue_start_screen_active())
    {
        rogue_start_screen_update_and_render();
    }
    else
    {
    g_app.frame_draw_calls = 0; g_app.frame_tile_quads = 0; /* reset metrics each frame */
    /* TODO(modularization): Consider extracting tile sprite LUT build into a tile_sprite_cache module. */
    /* Lazy one-time load of assets (avoid repeated loads). Adjust paths to actual asset locations. */
    rogue_tile_sprite_cache_ensure();
    if(!g_app.player_loaded) { rogue_player_assets_ensure_loaded(); }

    /* Update player position / camera */
    rogue_player_controller_update();
    /* Attack input (SPACE/RETURN maps to ACTION) */
    int attack_pressed = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION);
    float raw_dt_ms = (float)g_app.dt * 1000.0f;
    if(g_app.hitstop_timer_ms > 0){
        g_app.hitstop_timer_ms -= raw_dt_ms;
        if(g_app.hitstop_timer_ms < 0) g_app.hitstop_timer_ms = 0;
    }
    float hitstop_scale = (g_app.hitstop_timer_ms > 0)? 0.25f : 1.0f; /* quarter speed during hitstop */
    float dt_ms = raw_dt_ms * hitstop_scale;
    rogue_player_assets_update_animation(raw_dt_ms, dt_ms, raw_dt_ms, attack_pressed);
    /* Progression (leveling, regen) */
    rogue_player_progress_update(g_app.dt);
    /* Decoupled persistence autosave tick */
    rogue_persistence_autosave_update(g_app.dt);
        /* Wrap inside map bounds */
        if(g_app.player.base.pos.x < 0) g_app.player.base.pos.x = 0;
        if(g_app.player.base.pos.y < 0) g_app.player.base.pos.y = 0;
        if(g_app.player.base.pos.x > g_app.world_map.width-1) g_app.player.base.pos.x = (float)(g_app.world_map.width-1);
        if(g_app.player.base.pos.y > g_app.world_map.height-1) g_app.player.base.pos.y = (float)(g_app.world_map.height-1);
    /* Enemy spawning & AI updates */
    rogue_enemy_system_update(dt_ms);

    /* Advance animations */
    rogue_animation_update((float)g_app.dt * 1000.0f);

    /* camera now handled in player_controller */

    /* Update timed buffs & projectiles */
    rogue_buffs_update(g_app.game_time_ms);
    rogue_projectiles_update(dt_ms);
    /* Render world tiles */
    rogue_world_render_tiles();
    /* Queue world-space actors with y-sorting */
    rogue_scene_drawlist_begin();
    rogue_vegetation_render(); /* queues vegetation */
    rogue_player_render();      /* queues player */
    rogue_enemy_render();       /* queues enemies */
    rogue_scene_drawlist_flush();
    /* Render projectiles (after world, before HUD) */
    rogue_projectiles_render();

    /* Floating damage numbers */
    rogue_damage_numbers_render();

    /* Mini-map in corner (scaled down, render-target cached) */
    /* Minimap */
    rogue_minimap_update_and_render(240);
    /* Skill bar flash/cooldown timers (needs dt_ms) */
    rogue_skill_bar_update(dt_ms);
    }
    /* HUD */
    rogue_hud_render();
    rogue_skill_bar_render();
    rogue_skill_tree_render();
    /* (skill bar update already handled inside gameplay branch) */
    /* Update floating damage numbers */
    rogue_damage_numbers_update((float)g_app.dt);
    /* (Overlay with debug metrics removed to show clean HUD; hotkeys still active) */
    rogue_stats_panel_render();
    if(!g_app.headless){ SDL_RenderPresent(g_app.renderer); }
    /* Refresh exported player after all stat changes this frame */
    g_exposed_player_for_stats = g_app.player;
#endif
    rogue_game_loop_iterate();
    /* Advance global game time for systems needing wall-clock ms */
    g_app.game_time_ms += g_app.dt * 1000.0;
    rogue_metrics_frame_end(frame_start);
    rogue_input_next_frame(&g_app.input);
}

void rogue_app_run(void)
{
    while (g_game_loop.running)
    {
        rogue_app_step();
    }
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
    /* Free tile sprite cache */
    rogue_tile_sprite_cache_free();
    /* Persist (gen params if dirty + player stats) */
    rogue_persistence_save_on_shutdown();
}

int rogue_app_frame_count(void) { return g_app.frame_count; }
int rogue_app_enemy_count(void) { return g_app.enemy_count; }
void rogue_app_skip_start_screen(void){ g_app.show_start_screen = 0; }

void rogue_app_toggle_fullscreen(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.window) return;
    if (g_app.cfg.window_mode == ROGUE_WINDOW_FULLSCREEN)
        g_app.cfg.window_mode = ROGUE_WINDOW_WINDOWED;
    else
        g_app.cfg.window_mode = ROGUE_WINDOW_FULLSCREEN;
    rogue_platform_apply_window_mode();
#endif
}

void rogue_app_set_vsync(int enabled)
{
#ifdef ROGUE_HAVE_SDL
     /* SDL2 does not provide dynamic vsync toggle pre-2.0.18 except by recreating the renderer.
         For simplicity we just store the flag here (no-op for current session). */
     (void) enabled;
     ROGUE_LOG_WARN("rogue_app_set_vsync: dynamic toggle not implemented (requires renderer recreation)");
#else
    (void) enabled;
#endif
}

void rogue_app_get_metrics(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms){
    rogue_metrics_get(out_fps, out_frame_ms, out_avg_frame_ms);
}
double rogue_app_delta_time(void){ return rogue_metrics_delta_time(); }
