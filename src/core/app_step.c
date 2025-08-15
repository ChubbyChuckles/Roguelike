/* Frame update & render loop extracted from app.c */
#include "core/app.h"
#include "core/app_state.h"
#include "core/game_loop.h"
#include "core/metrics.h"
#include "input/input.h"
#include "core/player_controller.h"
#include "core/player_assets.h"
#include "core/player_progress.h"
#include "core/persistence_autosave.h"
#include "core/enemy_system.h"
#include "core/skill_bar.h"
#include "core/skill_tree.h"
#include "core/skills.h"
#include "core/buffs.h"
#include "core/projectiles.h"
#include "core/loot_instances.h"
#include "core/loot_pickup.h"
#include "core/vendor.h"
#include "core/stat_cache.h"
#include "core/equipment_stats.h"
#include "core/hud.h"
#include "core/minimap.h"
#include "core/scene_drawlist.h"
#include "core/enemy_render.h"
#include "core/player_render.h"
#include "core/world_renderer.h"
#include "game/damage_numbers.h"
#include "graphics/tile_sprites.h"
#include "core/tile_sprite_cache.h"
#include "core/animation_system.h"
#include "core/start_screen.h"
#include "core/loot_tables.h" /* rogue_loot_tables_count */
#include "core/input_events.h" /* rogue_process_events */
#include "core/vegetation.h" /* rogue_vegetation_render */
#include "core/player_assets.h" /* animation update */
#include "core/player_controller.h"

/* UI panels (implemented in vendor_ui.c) */
void rogue_vendor_panel_render(void);
void rogue_equipment_panel_render(void);
#include "util/log.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

int rogue_get_current_attack_frame(void){
    if(g_app.player_combat.phase==ROGUE_ATTACK_WINDUP || g_app.player_combat.phase==ROGUE_ATTACK_STRIKE || g_app.player_combat.phase==ROGUE_ATTACK_RECOVER){
        return g_app.player.anim_frame;
    }
    return 0;
}

static void rogue_app_handle_vendor_restock(float dt_ms){
    g_app.vendor_time_accum_ms += dt_ms;
    if(g_app.vendor_time_accum_ms >= g_app.vendor_restock_interval_ms){
        g_app.vendor_time_accum_ms -= g_app.vendor_restock_interval_ms;
        rogue_vendor_reset();
        if(rogue_loot_tables_count()>0){
            RogueGenerationContext vctx = {.enemy_level=g_app.player.level,.biome_id=0,.enemy_archetype=0,.player_luck=0};
            unsigned int seed = g_app.vendor_seed; rogue_vendor_generate_inventory(0,8,&vctx,&seed); g_app.vendor_seed = seed + 17u;
        }
    }
}

void rogue_app_add_hitstop(float ms){ if(ms < 0) return; if(ms > 180.0f) ms = 180.0f; if(g_app.hitstop_timer_ms < ms) g_app.hitstop_timer_ms = ms; }

void rogue_app_step(void)
{
    if (!g_game_loop.running) return;
    rogue_process_events();
    double frame_start = rogue_metrics_frame_begin();
#ifdef ROGUE_HAVE_SDL
    g_app.title_time += g_app.dt;
    SDL_SetRenderDrawColor(g_app.renderer, g_app.cfg.background_color.r, g_app.cfg.background_color.g,
                           g_app.cfg.background_color.b, g_app.cfg.background_color.a);
    SDL_RenderClear(g_app.renderer);
    if (rogue_start_screen_active()){
        rogue_start_screen_update_and_render();
    } else {
        g_app.frame_draw_calls = 0; g_app.frame_tile_quads = 0;
        rogue_tile_sprite_cache_ensure();
        if(!g_app.player_loaded) { rogue_player_assets_ensure_loaded(); }
        rogue_player_controller_update();
    extern void rogue_process_pending_skill_activations(void); /* declared in skills runtime */
    rogue_process_pending_skill_activations();
        int attack_pressed = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION);
        float raw_dt_ms = (float)g_app.dt * 1000.0f;
        if(g_app.hitstop_timer_ms > 0){ g_app.hitstop_timer_ms -= raw_dt_ms; if(g_app.hitstop_timer_ms < 0) g_app.hitstop_timer_ms = 0; }
        float hitstop_scale = (g_app.hitstop_timer_ms > 0)? 0.25f : 1.0f;
        float dt_ms = raw_dt_ms * hitstop_scale;
        rogue_player_assets_update_animation(raw_dt_ms, dt_ms, raw_dt_ms, attack_pressed);
        rogue_player_progress_update(g_app.dt);
        rogue_persistence_autosave_update(g_app.dt);
        if(g_app.player.base.pos.x < 0) g_app.player.base.pos.x = 0;
        if(g_app.player.base.pos.y < 0) g_app.player.base.pos.y = 0;
        if(g_app.player.base.pos.x > g_app.world_map.width-1) g_app.player.base.pos.x = (float)(g_app.world_map.width-1);
        if(g_app.player.base.pos.y > g_app.world_map.height-1) g_app.player.base.pos.y = (float)(g_app.world_map.height-1);
        rogue_enemy_system_update(dt_ms);
        rogue_items_update(dt_ms);
        rogue_loot_pickup_update(0.6f);
        rogue_app_handle_vendor_restock(dt_ms);
        rogue_animation_update((float)g_app.dt * 1000.0f);
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
        rogue_minimap_update_and_render(240);
        rogue_skill_bar_update(dt_ms);
    }
    rogue_hud_render();
    rogue_skill_bar_render();
    rogue_skill_tree_render();
    rogue_damage_numbers_update((float)g_app.dt);
    rogue_stats_panel_render();
    rogue_vendor_panel_render();
    rogue_equipment_panel_render();
    rogue_equipment_apply_stat_bonuses(&g_app.player);
    rogue_stat_cache_update(&g_app.player);
    if(!g_app.headless){ SDL_RenderPresent(g_app.renderer); }
    g_exposed_player_for_stats = g_app.player;
#endif
    rogue_game_loop_iterate();
    g_app.game_time_ms += g_app.dt * 1000.0;
    rogue_metrics_frame_end(frame_start);
    rogue_input_next_frame(&g_app.input);
}
