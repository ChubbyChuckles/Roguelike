/* Frame update & render loop extracted from app.c */
#include "../../game/damage_numbers.h"
#include "../../graphics/tile_sprites.h"
#include "../../input/input.h"
#include "../animation_system.h"
#include "../buffs.h"
#include "../dialogue.h"
#include "../enemy/enemy_render.h"
#include "../enemy/enemy_system.h"
#include "../equipment/equipment_stats.h"
#include "../game_loop.h"
#include "../hud/hud.h"
#include "../input_events.h" /* rogue_process_events */
#include "../loot/loot_instances.h"
#include "../loot/loot_pickup.h"
#include "../loot/loot_tables.h" /* rogue_loot_tables_count */
#include "../metrics.h"
#include "../minimap/minimap.h"
#include "../persistence/persistence_autosave.h"
#include "../player/player_assets.h"
#include "../player/player_assets.h" /* animation update */
#include "../player/player_controller.h"
#include "../player/player_progress.h"
#include "../player/player_render.h"
#include "../projectiles/projectiles.h"
#include "../scene_drawlist.h"
#include "../skills/skill_bar.h"
#include "../skills/skill_tree.h"
#include "../skills/skills.h"
#include "../start_screen.h"
#include "../stat_cache.h"
#include "../tile_sprite_cache.h"
#include "../vegetation/vegetation.h" /* rogue_vegetation_render */
#include "../vendor/vendor.h"
#include "../world_renderer.h"
#include "app.h"
#include "app_state.h"
#include <string.h> /* strlen used for fallback dialogue buffer registration */

/* UI panels (implemented in vendor_ui.c) */
#include "../../util/log.h"
#include "../equipment/equipment.h" /* if equipment panel declared elsewhere include appropriate header */
#include "../vendor/vendor_ui.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* Forward declare experimental skill graph runtime renderer */
void rogue_skillgraph_runtime_render(void);

int rogue_get_current_attack_frame(void)
{
    if (g_app.player_combat.phase == ROGUE_ATTACK_WINDUP ||
        g_app.player_combat.phase == ROGUE_ATTACK_STRIKE ||
        g_app.player_combat.phase == ROGUE_ATTACK_RECOVER)
    {
        return g_app.player.anim_frame;
    }
    return 0;
}

static void rogue_app_handle_vendor_restock(float dt_ms)
{
    g_app.vendor_time_accum_ms += dt_ms;
    if (g_app.vendor_time_accum_ms >= g_app.vendor_restock_interval_ms)
    {
        g_app.vendor_time_accum_ms -= g_app.vendor_restock_interval_ms;
        rogue_vendor_reset();
        if (rogue_loot_tables_count() > 0)
        {
            RogueGenerationContext vctx = {.enemy_level = g_app.player.level,
                                           .biome_id = 0,
                                           .enemy_archetype = 0,
                                           .player_luck = 0};
            unsigned int seed = g_app.vendor_seed;
            rogue_vendor_generate_inventory(0, 8, &vctx, &seed);
            g_app.vendor_seed = seed + 17u;
        }
    }
}

void rogue_app_add_hitstop(float ms)
{
    if (ms < 0)
        return;
    if (ms > 180.0f)
        ms = 180.0f;
    if (g_app.hitstop_timer_ms < ms)
        g_app.hitstop_timer_ms = ms;
}

void rogue_app_step(void)
{
    /* Diagnostics for test 10.4: always emit a probe line at function entry (even if not running)
     */
    {
        FILE* pf = NULL;
#if defined(_MSC_VER)
        if (fopen_s(&pf, "rm_probe.txt", "a") == 0 && pf)
#else
        pf = fopen("rm_probe.txt", "a");
        if (pf)
#endif
        {
            fprintf(pf, "entry running=%d show=%d rm=%d state=%d t=%.3f\n",
                    (int) g_game_loop.running, g_app.show_start_screen, g_app.reduced_motion,
                    g_app.start_state, (double) g_app.start_state_t);
            fclose(pf);
        }
    }
    /* Ensure reduced-motion fade skips are applied even if the game loop isn't running yet
        (unit tests invoke rogue_app_step() directly). This must not depend on show_start_screen
        because some test harnesses may not toggle it before the first step. */
    if (g_app.reduced_motion)
    {
        if (g_app.start_state == ROGUE_START_FADE_IN)
        {
            g_app.start_state = ROGUE_START_MENU;
            g_app.start_state_t = 1.0f;
        }
        else if (g_app.start_state == ROGUE_START_FADE_OUT)
        {
            g_app.start_state_t = 0.0f;
            g_app.show_start_screen = 0;
        }
    }
    if (!g_game_loop.running)
        return;
    rogue_process_events();
    double frame_start = rogue_metrics_frame_begin();
    /* Accessibility fast-path: if reduced motion is enabled, enforce fade skips
       immediately at frame start so tests and non-SDL code paths see the effect
        without relying on later UI updates. Do not gate on show_start_screen. */
    if (g_app.reduced_motion)
    {
        if (g_app.start_state == ROGUE_START_FADE_IN)
        {
            g_app.start_state = ROGUE_START_MENU;
            g_app.start_state_t = 1.0f;
        }
        else if (g_app.start_state == ROGUE_START_FADE_OUT)
        {
            g_app.start_state_t = 0.0f;
            g_app.show_start_screen = 0;
        }
        ROGUE_LOG_INFO("reduced_motion guard: state=%d t=%.3f show=%d", g_app.start_state,
                       (double) g_app.start_state_t, g_app.show_start_screen);
        {
            FILE* f = NULL;
#if defined(_MSC_VER)
            if (fopen_s(&f, "rm_guard.txt", "a") == 0 && f)
#else
            f = fopen("rm_guard.txt", "a");
            if (f)
#endif
            {
                fprintf(f, "state=%d t=%.3f show=%d\n", g_app.start_state,
                        (double) g_app.start_state_t, g_app.show_start_screen);
                fclose(f);
            }
        }
    }
#ifdef ROGUE_HAVE_SDL
    g_app.title_time += g_app.dt;
    SDL_SetRenderDrawColor(g_app.renderer, g_app.cfg.background_color.r,
                           g_app.cfg.background_color.g, g_app.cfg.background_color.b,
                           g_app.cfg.background_color.a);
    SDL_RenderClear(g_app.renderer);
    if (rogue_start_screen_active())
    {
        /* Ensure fade speed respects reduced-motion preference by shortening duration */
        if (g_app.start_state == ROGUE_START_FADE_IN && g_app.start_state_t == 0.0f)
        {
            /* If future global reduced motion flag exists, we could speed up here. */
            if (g_app.start_state_speed <= 0.0f)
                g_app.start_state_speed = 1.0f;
        }
        rogue_start_screen_update_and_render();
    }
    else
    {
        g_app.frame_draw_calls = 0;
        g_app.frame_tile_quads = 0;
        rogue_tile_sprite_cache_ensure();
        if (!g_app.player_loaded)
        {
            rogue_player_assets_ensure_loaded();
        }
        rogue_player_controller_update();
        extern void rogue_process_pending_skill_activations(void); /* declared in skills runtime */
        rogue_process_pending_skill_activations();
        int attack_pressed = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION);
        int dialogue_pressed = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DIALOGUE);
        float raw_dt_ms = (float) g_app.dt * 1000.0f;
        if (g_app.hitstop_timer_ms > 0)
        {
            g_app.hitstop_timer_ms -= raw_dt_ms;
            if (g_app.hitstop_timer_ms < 0)
                g_app.hitstop_timer_ms = 0;
        }
        float hitstop_scale = (g_app.hitstop_timer_ms > 0) ? 0.25f : 1.0f;
        float dt_ms = raw_dt_ms * hitstop_scale;
        rogue_player_assets_update_animation(raw_dt_ms, dt_ms, raw_dt_ms, attack_pressed);
        rogue_player_progress_update(g_app.dt);
        rogue_persistence_autosave_update(g_app.dt);
        if (g_app.player.base.pos.x < 0)
            g_app.player.base.pos.x = 0;
        if (g_app.player.base.pos.y < 0)
            g_app.player.base.pos.y = 0;
        if (g_app.player.base.pos.x > g_app.world_map.width - 1)
            g_app.player.base.pos.x = (float) (g_app.world_map.width - 1);
        if (g_app.player.base.pos.y > g_app.world_map.height - 1)
            g_app.player.base.pos.y = (float) (g_app.world_map.height - 1);
        rogue_enemy_system_update(dt_ms);
        rogue_items_update(dt_ms);
        rogue_loot_pickup_update(0.6f);
        rogue_app_handle_vendor_restock(dt_ms);
        if (g_app.vendor_insufficient_flash_ms > 0)
        {
            g_app.vendor_insufficient_flash_ms -= dt_ms;
            if (g_app.vendor_insufficient_flash_ms < 0)
                g_app.vendor_insufficient_flash_ms = 0;
        }
        rogue_animation_update((float) g_app.dt * 1000.0f);
        rogue_buffs_update(g_app.game_time_ms);
        rogue_projectiles_update(dt_ms);
        rogue_world_render_tiles();
        rogue_scene_drawlist_begin();
        rogue_vegetation_render();
        rogue_player_render();
        rogue_enemy_render();
        rogue_scene_drawlist_flush();
        rogue_world_render_items();
        rogue_projectiles_render();
        rogue_damage_numbers_render();
        if (g_app.show_minimap)
        {
            rogue_minimap_update_and_render(240);
            g_app.last_minimap_rendered = 1;
        }
        else
        {
            g_app.last_minimap_rendered = 0;
        }
        rogue_skill_bar_update(dt_ms);
        /* Dialogue runtime panel (direct SDL draw) & input binding */
        {
            if (dialogue_pressed)
            {
                const RogueDialoguePlayback* dp = rogue_dialogue_playback();
                if (dp)
                {
                    /* Advance current script; rogue_dialogue_advance returns 0 when finished */
                    rogue_dialogue_advance();
                }
                else
                {
                    /* Prefer JSON-loaded intro script (id=100) */
                    const RogueDialogueScript* json_sc = rogue_dialogue_get(100);
                    if (json_sc)
                    {
                        static int typewriter_init = 0;
                        if (!typewriter_init)
                        {
                            rogue_dialogue_typewriter_enable(1, 0.08f);
                            typewriter_init = 1;
                        }
                        rogue_dialogue_start(100);
                    }
                    else
                    {
                        /* Fallback legacy demo (only if JSON script missing) */
                        static int demo_loaded = 0;
                        if (!demo_loaded)
                        {
                            const char* buf =
                                "npc|Welcome to the realm, hero!\n"
                                "npc|This is a dialogue test line with tokens: ${player_name}.\n";
                            rogue_dialogue_register_from_buffer(1000, buf, (int) strlen(buf));
                            rogue_dialogue_typewriter_enable(1, 0.08f);
                            ROGUE_LOG_WARN(
                                "Dialogue script id=100 not found; using fallback demo (1000)");
                            demo_loaded = 1;
                        }
                        rogue_dialogue_start(1000);
                    }
                }
            }
            rogue_dialogue_update(dt_ms);
            rogue_dialogue_render_runtime();
        }
    }
    rogue_hud_render(); /* includes alerts + metrics overlay now */
    rogue_skill_bar_render();
    rogue_skill_tree_render();
    rogue_damage_numbers_update((float) g_app.dt);
    rogue_stats_panel_render();
    rogue_vendor_panel_render();
    rogue_equipment_panel_render();
    /* Dialogue runtime panel (direct SDL draw) & input binding */
    /* Experimental skill graph UI (toggle with 'G') */
    rogue_skillgraph_runtime_render();
    rogue_equipment_apply_stat_bonuses(&g_app.player);
    rogue_stat_cache_update(&g_app.player);
    if (!g_app.headless)
    {
        SDL_RenderPresent(g_app.renderer);
    }
    g_exposed_player_for_stats = g_app.player;
#endif
    rogue_game_loop_iterate();
    g_app.game_time_ms += g_app.dt * 1000.0;
    rogue_metrics_frame_end(frame_start);
    rogue_input_next_frame(&g_app.input);
}
