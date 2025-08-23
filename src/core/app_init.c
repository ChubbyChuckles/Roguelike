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
#include "core/animation_system.h"
#include "core/app.h"
#include "core/app_state.h"
#include "core/asset_config.h"
#include "core/buffs.h"
#include "core/dialogue.h" /* dialogue system */
#include "core/equipment/equipment_stats.h"
#include "core/game_loop.h"
#include "core/localization.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_logging.h"
#include "core/loot/loot_tables.h"
#include "core/metrics.h"
#include "core/persistence.h"
#include "core/platform.h"
#include "core/projectiles.h"
#include "core/projectiles_config.h"
#include "core/save_manager.h" /* Phase1 scaffold present but not auto-initialized yet */
#include "core/skill_bar.h"
#include "core/skill_tree.h"
#include "core/skills.h"
#include "core/start_screen.h"
#include "core/stat_cache.h"
#include "core/tile_sprite_cache.h"
#include "core/vegetation/vegetation.h"
#include "core/vendor/economy.h"
#include "core/vendor/vendor.h"
#include "core\inventory\inventory.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "game/combat.h"
#include "game/hit_system.h" /* weapon geometry JSON */
#include "persistence/persistence_autosave.h"
#include "util/log.h"
#include "world/world_gen.h"
#include "world/world_gen_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#if defined(_MSC_VER)
    rogue_app_state_maybe_init();
#endif
    g_app.cfg = *cfg;
    g_app.show_start_screen = 1;
    rogue_input_clear(&g_app.input);
    g_app.title_time = 0.0;
    g_app.menu_index = 0;
    g_app.entering_seed = 0;
    g_app.pending_seed = 1337u;
    g_app.start_state = ROGUE_START_FADE_IN;
    g_app.start_state_t = 0.0f;
    g_app.start_state_speed = 1.0f; /* adjusted later if reduced motion desired */
#ifdef ROGUE_HAVE_SDL
    g_app.start_bg_tex = NULL;
#endif
    g_app.start_bg_loaded = 0;
    g_app.start_bg_scale = ROGUE_BG_COVER;
    g_app.start_bg_tint = 0xFFFFFFFFu;
    g_app.reduced_motion = 0;
    /* Start screen nav repeat config (Phase 3.2) */
    g_app.start_nav_accum_ms = 0.0;
    g_app.start_nav_dir_v = 0;
    g_app.start_nav_repeating = 0;
    g_app.start_nav_initial_ms = 400.0; /* desktop-ish default */
    g_app.start_nav_interval_ms = 65.0; /* ~15Hz */
    rogue_player_init(&g_app.player);
    g_app.unspent_stat_points = 0;
    if (!rogue_platform_init(cfg))
        return false;
    RogueGameLoopConfig loop_cfg = {.target_fps = cfg->target_fps};
    rogue_game_loop_init(&loop_cfg);
    rogue_persistence_load_generation_params();
    /* Phase1 SaveManager scaffold available (manual tests only) */
    rogue_skills_init();
    rogue_buffs_init();
    rogue_projectiles_init();
    rogue_projectiles_config_load_and_watch("assets/projectiles.cfg");
    rogue_skill_tree_register_baseline();
    rogue_item_defs_reset();
    rogue_loot_tables_reset();
    rogue_items_init_runtime();
    rogue_inventory_init();
    g_app.inventory_sort_mode = 0; /* default sort none */
    rogue_loot_logging_init_from_env();
    /* Load hitbox tuning if present */
    RogueHitboxTuning* tune = rogue_hitbox_tuning_get();
    tune->enemy_radius = 0.40f; /* default */
    if (rogue_hitbox_tuning_resolve_path())
    {
        const char* tpath = rogue_hitbox_tuning_path();
        if (tpath && rogue_hitbox_tuning_load(tpath, tune) == 0)
        {
            ROGUE_LOG_INFO("hitbox_tuning_loaded: %s", tpath);
        }
    }
    rogue_persistence_load_player_stats();
    /* Reset localization to defaults at startup. */
    rogue_locale_reset();
    /* Accessibility: allow reduced motion via env ROGUE_REDUCED_MOTION=1 */
#if defined(_MSC_VER)
    {
        char* rm = NULL;
        size_t rml = 0;
        if (_dupenv_s(&rm, &rml, "ROGUE_REDUCED_MOTION") == 0 && rm)
        {
            if (rm[0] == '1')
                g_app.reduced_motion = 1;
            free(rm);
        }
    }
#else
    {
        const char* rm = getenv("ROGUE_REDUCED_MOTION");
        if (rm && rm[0] == '1')
            g_app.reduced_motion = 1;
    }
#endif
    /* Cheat override (dev convenience): set talent points to 100 only if env var
       ROGUE_TALENT_CHEAT=1. Keeps unit tests deterministic (they expect default progression) while
       allowing manual play sessions to use the shortcut without editing code. */
    const char* cheat_env = NULL;
#if defined(_MSC_VER)
    char* cheat_tmp = NULL;
    size_t cheat_len = 0;
    if (_dupenv_s(&cheat_tmp, &cheat_len, "ROGUE_TALENT_CHEAT") == 0 && cheat_tmp)
    {
        cheat_env = cheat_tmp;
    }
#else
    cheat_env = getenv("ROGUE_TALENT_CHEAT");
#endif
    if (cheat_env && cheat_env[0] == '1')
    {
        g_app.talent_points = 100;
        g_app.stats_dirty = 1; /* ensure any dependent stat recalculations happen */
    }
#if defined(_MSC_VER)
    if (cheat_tmp)
        free(cheat_tmp);
#endif
    RogueWorldGenConfig wcfg = rogue_world_gen_config_build(1337u, 1, 1);
    if (!rogue_world_generate_full(&g_app.world_map, &wcfg))
    {
        rogue_world_generate(&g_app.world_map, &wcfg); /* fallback */
    }
    /* Random player spawn on walkable tile (seed-derived) */
    int spawn_x = 2, spawn_y = 2;
    if (rogue_world_find_random_spawn(&g_app.world_map, wcfg.seed * 1664525u + 1013904223u,
                                      &spawn_x, &spawn_y))
    {
        g_app.player.base.pos.x = (float) spawn_x + 0.5f;
        g_app.player.base.pos.y = (float) spawn_y + 0.5f;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("assets/plants.cfg", "assets/trees.cfg");
    rogue_vegetation_generate(0.12f, 1337u);
    rogue_vegetation_set_trunk_collision_enabled(1);
    rogue_vegetation_set_canopy_tile_blocking_enabled(0);
    g_exposed_player_for_stats = g_app.player;
    g_app.stats_dirty = 0;
    g_app.show_stats_panel = 0;
    g_app.stats_panel_index = 0;
    g_app.time_since_player_hit_ms = 0.0f;
    g_app.health_regen_accum_ms = 0.0f;
    g_app.mana_regen_accum_ms = 0.0f;
    g_app.ap_regen_accum_ms = 0.0f;
    g_app.levelup_aura_timer_ms = 0.0f;
    g_app.dmg_number_count = 0;
    g_app.spawn_accum_ms = 700.0;
    g_app.show_vendor_panel = 0;
    g_app.vendor_selection = 0;
    g_app.vendor_seed = 424242u;
    g_app.vendor_time_accum_ms = 0.0;
    g_app.vendor_restock_interval_ms = 30000.0;
    g_app.vendor_x = 4.5f;
    g_app.vendor_y = 4.5f;
    g_app.show_equipment_panel = 0;
    int items_loaded = rogue_item_defs_load_directory("assets/items");
    if (items_loaded <= 0)
    {
        items_loaded = rogue_item_defs_load_from_cfg("assets/test_items.cfg");
    }
    int tables_loaded = rogue_loot_tables_load_from_cfg("assets/test_loot_tables.cfg");
    if (tables_loaded > 0)
    {
        RogueGenerationContext vctx = {.enemy_level = g_app.player.level,
                                       .biome_id = 0,
                                       .enemy_archetype = 0,
                                       .player_luck = 0};
        unsigned int seed = g_app.vendor_seed;
        rogue_vendor_reset();
        rogue_vendor_generate_inventory(0, 8, &vctx, &seed);
        g_app.vendor_seed = seed + 17u;
    }
    rogue_econ_add_gold(250);
#ifdef ROGUE_HAVE_SDL_MIXER
    g_app.sfx_levelup = NULL;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512) != 0)
    {
        ROGUE_LOG_WARN("Mix_OpenAudio failed: %s", Mix_GetError());
    }
    else
    {
        rogue_asset_load_sounds();
    }
#endif
    g_app.tileset_loaded = 0;
    g_app.tile_size = 16;
    g_app.player_frame_size = 64;
    g_app.player_loaded = 0;
    g_app.player_state = 0;
    g_app.player_sheet_paths_loaded = 0;
    g_app.cam_x = 0.0f;
    g_app.cam_y = 0.0f;
    g_app.viewport_w = cfg->logical_width ? cfg->logical_width : cfg->window_width;
    g_app.viewport_h = cfg->logical_height ? cfg->logical_height : cfg->window_height;
    g_app.walk_speed = 45.0f;
    g_app.run_speed = 85.0f;
    g_app.tile_sprite_lut = NULL;
    g_app.tile_sprite_lut_ready = 0;
    g_app.minimap_dirty = 1;
    g_app.minimap_w = g_app.minimap_h = 0;
    g_app.minimap_step = 1;
    g_app.chunk_size = 32;
    g_app.chunks_x = g_app.chunks_y = 0;
    g_app.chunk_dirty = NULL;
    g_app.anim_dt_accum_ms = 0.0f;
    g_app.frame_draw_calls = 0;
    g_app.frame_tile_quads = 0;
    rogue_combat_init(&g_app.player_combat);
    g_app.enemy_count = 0;
    g_app.total_kills = 0;
    g_app.enemy_type_count = ROGUE_MAX_ENEMY_TYPES;
    int loaded_json_count = 0;
    int json_ok = rogue_enemy_types_load_directory_json("../assets/enemies", g_app.enemy_types,
                                                        ROGUE_MAX_ENEMY_TYPES, &loaded_json_count);
    if (json_ok && loaded_json_count > 0)
    {
        g_app.enemy_type_count = loaded_json_count;
        ROGUE_LOG_INFO("Loaded %d enemy JSON type(s)", loaded_json_count);
    }
    else
    {
        ROGUE_LOG_WARN("No enemy JSON types loaded; injecting fallback dummy type.");
        g_app.enemy_type_count = 1;
        RogueEnemyTypeDef* t = &g_app.enemy_types[0];
        memset(t, 0, sizeof *t);
#if defined(_MSC_VER)
        strncpy_s(t->name, sizeof t->name, "dummy", _TRUNCATE);
#else
        strncpy(t->name, "dummy", sizeof t->name - 1);
        t->name[sizeof t->name - 1] = '\0';
#endif
        t->group_min = 1;
        t->group_max = 2;
        t->patrol_radius = 5;
        t->aggro_radius = 6;
        t->speed = 30.0f;
        t->pop_target = 15;
        t->xp_reward = 2;
        t->loot_chance = 0.05f;
    }
    /* Weapon hit geometry load (Phase 1). Try multiple candidate roots for dev/build execution
     * contexts. */
    const char* geo_candidates[] = {"assets/weapon_hit_geo.json", "../assets/weapon_hit_geo.json",
                                    "../../assets/weapon_hit_geo.json"};
    int geo_loaded = 0;
    for (size_t gi = 0; gi < sizeof(geo_candidates) / sizeof(geo_candidates[0]); ++gi)
    {
        int r = rogue_weapon_hit_geo_load_json(geo_candidates[gi]);
        if (r > 0)
        {
            ROGUE_LOG_INFO("Weapon hit geometry loaded: %d entries (%s)", r, geo_candidates[gi]);
            geo_loaded = 1;
            break;
        }
        else
        {
            ROGUE_LOG_DEBUG("Weapon hit geo load attempt failed (%s)", geo_candidates[gi]);
        }
    }
    if (!geo_loaded)
    {
        rogue_weapon_hit_geo_ensure_default();
        ROGUE_LOG_WARN("No weapon_hit_geo.json loaded; using default geometry");
    }
    for (int i = 0; i < ROGUE_MAX_ENEMY_TYPES; i++)
    {
        g_app.per_type_counts[i] = 0;
    }
    for (int i = 0; i < ROGUE_MAX_ENEMIES; i++)
    {
        g_app.enemies[i].alive = 0;
    }
    {
        char cwd_buf[512];
#ifdef _WIN32
        if (_getcwd(cwd_buf, sizeof cwd_buf))
        {
            ROGUE_LOG_INFO("CWD: %s", cwd_buf);
        }
        else
        {
            ROGUE_LOG_WARN("Could not determine CWD");
        }
#else
        if (getcwd(cwd_buf, sizeof cwd_buf))
        {
            ROGUE_LOG_INFO("CWD: %s", cwd_buf);
        }
        else
        {
            ROGUE_LOG_WARN("Could not determine CWD");
        }
#endif
    }
    /* Dialogue initialization (style + scripts). This block previously lived in old app.c which is
       no longer compiled. We attempt a few relative roots so running from either repo root or build
       dir works. Non-fatal on failure. */
    {
        const char* style_candidates[] = {"assets/dialogue/style_default.json",
                                          "../assets/dialogue/style_default.json",
                                          "../../assets/dialogue/style_default.json"};
        int style_loaded = 0;
        for (size_t i = 0; i < sizeof(style_candidates) / sizeof(style_candidates[0]); ++i)
        {
            int r = rogue_dialogue_style_load_from_json(style_candidates[i]);
            if (r == 0)
            {
                ROGUE_LOG_INFO("Dialogue style loaded: %s", style_candidates[i]);
                style_loaded = 1;
                break;
            }
            else
            {
                ROGUE_LOG_WARN("Dialogue style load failed(code=%d): %s", r, style_candidates[i]);
            }
        }
        if (!style_loaded)
            ROGUE_LOG_WARN("No dialogue style loaded (all candidates failed)");

        const char* script_candidates[] = {"assets/dialogue/dialogues.json",
                                           "../assets/dialogue/dialogues.json",
                                           "../../assets/dialogue/dialogues.json"};
        int scripts_loaded = 0;
        const char* used_path = NULL;
        for (size_t i = 0; i < sizeof(script_candidates) / sizeof(script_candidates[0]); ++i)
        {
            int r = rogue_dialogue_load_script_from_json_file(script_candidates[i]);
            if (r == 0)
            {
                scripts_loaded = 1;
                used_path = script_candidates[i];
                break;
            }
            else
            {
                ROGUE_LOG_WARN("Dialogue script load failed(code=%d): %s", r, script_candidates[i]);
            }
        }
        if (scripts_loaded)
        {
            ROGUE_LOG_INFO("Dialogue scripts loaded from: %s", used_path);
        }
        else
        {
            ROGUE_LOG_WARN("No dialogue scripts loaded (all candidates failed)");
        }

        /* Diagnostics: dump registry */
        extern int rogue_dialogue_script_count(void);
        extern const RogueDialogueScript* rogue_dialogue_get(int id);
        int sc_total = rogue_dialogue_script_count();
        ROGUE_LOG_INFO("Dialogue registry count=%d", sc_total);
        for (int probe = 50; probe <= 1100; ++probe)
        {
            const RogueDialogueScript* sc = rogue_dialogue_get(probe);
            if (sc)
            {
                ROGUE_LOG_INFO("Dialogue present id=%d lines=%d", sc->id, sc->line_count);
            }
        }
    }
    g_app.analytics_damage_dealt_total = 0ULL;
    g_app.analytics_gold_earned_total = 0ULL;
    g_app.permadeath_mode = 0;
    /* Reduced-motion test 10.4 diagnostics: when invoked by that test, emit a one-line init dump
       so we can verify initial flags before the first rogue_app_step(). */
    if (cfg && cfg->window_title && strcmp(cfg->window_title, "StartScreenReducedMotion") == 0)
    {
        FILE* f = NULL;
#if defined(_MSC_VER)
        if (fopen_s(&f, "rm_init.txt", "w") == 0 && f)
#else
        f = fopen("rm_init.txt", "w");
        if (f)
#endif
        {
            fprintf(f, "init running=%d show=%d rm=%d state=%d t=%.3f\n", (int) g_game_loop.running,
                    g_app.show_start_screen, g_app.reduced_motion, g_app.start_state,
                    (double) g_app.start_state_t);
            fclose(f);
        }
    }
    return true;
}

void rogue_app_shutdown(void)
{
#ifdef ROGUE_HAVE_SDL_MIXER
    if (g_app.sfx_levelup)
    {
        Mix_FreeChunk(g_app.sfx_levelup);
        g_app.sfx_levelup = NULL;
    }
    Mix_CloseAudio();
#endif
    rogue_platform_shutdown();
    rogue_skills_shutdown();
    if (g_app.chunk_dirty)
    {
        free(g_app.chunk_dirty);
        g_app.chunk_dirty = NULL;
    }
    rogue_tile_sprite_cache_free();
    rogue_persistence_save_on_shutdown();
}
