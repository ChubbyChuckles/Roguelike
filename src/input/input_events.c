#include "input_events.h"
#include "../core/equipment/equipment.h"
#include "../core/hud/hud_overlays.h" /* alerts & metrics toggles */
#include "../core/inventory/inventory.h"
#include "../core/skills/skill_bar.h"
#include "../core/skills/skill_tree.h"
#include "../core/skills/skills.h"
#include "../core/vegetation/vegetation.h"
#include "../core/vendor/economy.h"
#include "../core/vendor/vendor.h"
#include "../entities/player.h"
#include "../game/damage_numbers.h"
#include "../game/game_loop.h"
#include "../game/hit_pixel_mask.h" // pixel hit detection toggle
#include "../game/hit_system.h"     /* debug toggle */
#include "../game/start_screen.h"
#include "../ui/core/ui_context.h" /* skill graph toggle */
#include "../world/tilemap.h"
#include "../world/world_gen.h"
#include "../world/world_gen_config.h"
#include <string.h> /* memset */
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* Simple ring buffer for deferred skill activations so that skills (e.g., projectile spawns) use
 * the post-movement player position. */
#define ROGUE_PENDING_SKILLS_MAX 32
typedef struct PendingSkillAct
{
    int skill_id;
    int bar_slot;
    double now_ms;
} PendingSkillAct;
static PendingSkillAct g_pending_skill_acts[ROGUE_PENDING_SKILLS_MAX];
static int g_pending_skill_head = 0; /* next write */
static int g_pending_skill_count = 0;

static void queue_skill_activation(int sid, int slot)
{
    if (sid < 0)
        return;
    if (g_pending_skill_count >= ROGUE_PENDING_SKILLS_MAX)
        return; /* drop oldest silently */
    int idx = (g_pending_skill_head + g_pending_skill_count) % ROGUE_PENDING_SKILLS_MAX;
    g_pending_skill_acts[idx].skill_id = sid;
    g_pending_skill_acts[idx].bar_slot = slot;
#ifdef ROGUE_HAVE_SDL
    g_pending_skill_acts[idx].now_ms = (double) SDL_GetTicks();
#else
    g_pending_skill_acts[idx].now_ms = 0.0;
#endif
    g_pending_skill_count++;
}

void rogue_process_events(void)
{
#ifdef ROGUE_HAVE_SDL
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT)
        {
            rogue_game_loop_request_exit();
        }
        rogue_input_process_sdl_event(&g_app.input, &ev);
        if (ev.type == SDL_KEYDOWN && !g_app.show_start_screen)
        {
            /* Allow vendor/equipment panel toggles even while skill tree UI is open. */
            if (rogue_skill_tree_is_open())
            {
                if (ev.key.keysym.sym != SDLK_v && ev.key.keysym.sym != SDLK_e)
                {
                    rogue_skill_tree_handle_key(ev.key.keysym.sym);
                    continue; /* consume all other keys while tree is open */
                }
            }
            if (ev.key.keysym.sym == SDLK_TAB)
            {
                g_app.show_stats_panel = !g_app.show_stats_panel;
            }
            if (ev.key.keysym.sym == SDLK_v)
            {
                g_app.show_vendor_panel = !g_app.show_vendor_panel;
                g_app.vendor_selection = 0;
            }
            if (ev.key.keysym.sym == SDLK_e)
            {
                g_app.show_equipment_panel = !g_app.show_equipment_panel;
            }
            if (ev.key.keysym.sym == SDLK_m)
            {
                g_app.show_minimap = !g_app.show_minimap;
            }
            if (ev.key.keysym.sym == SDLK_F1)
            {
                g_app.show_metrics_overlay = !g_app.show_metrics_overlay;
            }
            if (ev.key.keysym.sym == SDLK_F11)
            {
                g_app.show_skill_area_overlay = !g_app.show_skill_area_overlay;
            }
            if (g_app.show_equipment_panel && ev.key.keysym.sym == SDLK_r)
            { /* repair equipped weapon */
                rogue_equip_repair_slot(ROGUE_EQUIP_WEAPON);
            }
            if (g_app.show_vendor_panel)
            {
                if (ev.key.keysym.sym == SDLK_UP)
                {
                    g_app.vendor_selection--;
                    if (g_app.vendor_selection < 0)
                        g_app.vendor_selection =
                            rogue_vendor_item_count() > 0 ? rogue_vendor_item_count() - 1 : 0;
                }
                if (ev.key.keysym.sym == SDLK_DOWN)
                {
                    g_app.vendor_selection++;
                    if (g_app.vendor_selection >= rogue_vendor_item_count())
                        g_app.vendor_selection = 0;
                }
                if (ev.key.keysym.sym == SDLK_RETURN)
                {
                    const RogueVendorItem* vi = rogue_vendor_get(g_app.vendor_selection);
                    if (vi)
                    {
                        int price = rogue_econ_buy_price(vi);
                        if (!g_app.vendor_confirm_active)
                        {
                            /* open confirmation modal */
                            g_app.vendor_confirm_active = 1;
                            g_app.vendor_confirm_def_index = vi->def_index;
                            g_app.vendor_confirm_price = price;
                            g_app.vendor_insufficient_flash_ms = 0.0;
                        }
                        else
                        {
                            /* Accept inside modal */
                            if (rogue_econ_gold() >= g_app.vendor_confirm_price)
                            {
                                if (rogue_econ_try_buy(vi) == 0)
                                {
                                    rogue_inventory_add(vi->def_index, 1);
                                }
                                g_app.vendor_confirm_active = 0;
                            }
                            else
                            {
                                g_app.vendor_insufficient_flash_ms = 480.0; /* flash for ~0.5s */
                            }
                        }
                    }
                }
                if (ev.key.keysym.sym == SDLK_ESCAPE && g_app.vendor_confirm_active)
                {
                    g_app.vendor_confirm_active = 0;
                }
                if (ev.key.keysym.sym == SDLK_BACKSPACE)
                {
                    g_app.show_vendor_panel = 0;
                }
            }
            if (ev.key.keysym.sym == SDLK_k)
            {
                rogue_skill_tree_toggle();
            }
            /* Toggle experimental skill graph (new UI system) with G */
            if (ev.key.keysym.sym == SDLK_g)
            {
                g_app.show_skill_graph = !g_app.show_skill_graph;
            }
            if (g_app.show_stats_panel)
            {
                if (ev.key.keysym.sym == SDLK_LEFT)
                {
                    g_app.stats_panel_index = (g_app.stats_panel_index + 5) % 6;
                }
                if (ev.key.keysym.sym == SDLK_RIGHT)
                {
                    g_app.stats_panel_index = (g_app.stats_panel_index + 1) % 6;
                }
                if (ev.key.keysym.sym == SDLK_RETURN && g_app.unspent_stat_points > 0)
                {
                    if (g_app.stats_panel_index == 0)
                    {
                        g_app.player.strength++;
                    }
                    else if (g_app.stats_panel_index == 1)
                    {
                        g_app.player.dexterity++;
                    }
                    else if (g_app.stats_panel_index == 2)
                    {
                        g_app.player.vitality++;
                        rogue_player_recalc_derived(&g_app.player);
                    }
                    else if (g_app.stats_panel_index == 3)
                    {
                        g_app.player.intelligence++;
                    }
                    else if (g_app.stats_panel_index == 4)
                    {
                        g_app.player.crit_chance++;
                        if (g_app.player.crit_chance > 100)
                            g_app.player.crit_chance = 100;
                    }
                    else if (g_app.stats_panel_index == 5)
                    {
                        g_app.player.crit_damage += 5;
                        if (g_app.player.crit_damage > 400)
                            g_app.player.crit_damage = 400;
                    }
                    g_app.unspent_stat_points--;
                    g_app.stats_dirty = 1;
                }
                if (ev.key.keysym.sym == SDLK_BACKSPACE)
                {
                    g_app.show_stats_panel = 0;
                }
            }
            if (ev.key.keysym.sym == SDLK_r)
            {
                g_app.player_state = (g_app.player_state == 2) ? 1 : 2;
            }
            /* Synthetic alert test triggers (for headless / manual) */
            if (ev.key.keysym.sym == SDLK_F2)
            {
                rogue_alert_level_up();
            }
            if (ev.key.keysym.sym == SDLK_F3)
            {
                rogue_alert_low_health();
            }
            if (ev.key.keysym.sym == SDLK_F4)
            {
                rogue_alert_vendor_restock();
            }
            /* Skill activation keys 1-0 */
            int key = ev.key.keysym.sym;
            if (key >= SDLK_1 && key <= SDLK_9)
            {
                int slot = key - SDLK_1;
                int sid = g_app.skill_bar[slot];
                if (sid >= 0)
                {
                    queue_skill_activation(sid, slot);
                }
            }
            if (key == SDLK_0)
            {
                int sid = g_app.skill_bar[9];
                if (sid >= 0)
                {
                    queue_skill_activation(sid, 9);
                }
            }
            if (ev.key.keysym.sym == SDLK_F5)
            {
                g_app.gen_water_level -= 0.01;
                if (g_app.gen_water_level < 0.20)
                    g_app.gen_water_level = 0.20;
                g_app.gen_params_dirty = 1;
                ev.key.keysym.sym = SDLK_BACKQUOTE;
            }
            if (ev.key.keysym.sym == SDLK_F6)
            {
                g_app.gen_water_level += 0.01;
                if (g_app.gen_water_level > 0.55)
                    g_app.gen_water_level = 0.55;
                g_app.gen_params_dirty = 1;
                ev.key.keysym.sym = SDLK_BACKQUOTE;
            }
            if (ev.key.keysym.sym == SDLK_F7)
            {
                g_app.gen_noise_octaves++;
                if (g_app.gen_noise_octaves > 9)
                    g_app.gen_noise_octaves = 9;
                g_app.gen_params_dirty = 1;
                ev.key.keysym.sym = SDLK_BACKQUOTE;
            }
            if (ev.key.keysym.sym == SDLK_F8)
            {
                g_app.gen_noise_octaves--;
                if (g_app.gen_noise_octaves < 3)
                    g_app.gen_noise_octaves = 3;
                g_app.gen_params_dirty = 1;
                ev.key.keysym.sym = SDLK_BACKQUOTE;
            }
            if (ev.key.keysym.sym == SDLK_F9)
            {
                g_app.gen_river_sources += 2;
                if (g_app.gen_river_sources > 40)
                    g_app.gen_river_sources = 40;
                g_app.gen_params_dirty = 1;
                ev.key.keysym.sym = SDLK_BACKQUOTE;
            }
            if (ev.key.keysym.sym == SDLK_F10)
            {
                g_app.gen_river_sources -= 2;
                if (g_app.gen_river_sources < 2)
                    g_app.gen_river_sources = 2;
                g_app.gen_params_dirty = 1;
                ev.key.keysym.sym = SDLK_BACKQUOTE;
            }
            if (ev.key.keysym.sym == SDLK_f)
            {
                g_hit_debug_enabled = !g_hit_debug_enabled;
                g_app.show_hit_debug = g_hit_debug_enabled;
                ROGUE_LOG_INFO("hit_debug_toggle_f: enabled=%d", g_hit_debug_enabled);
            }
            const Uint8* ks_state2 = SDL_GetKeyboardState(NULL);
            /* SHIFT+M toggles pixel-mask hit detection (Slice B) */
            if ((ks_state2[SDL_SCANCODE_LSHIFT] || ks_state2[SDL_SCANCODE_RSHIFT]) &&
                ev.key.keysym.sym == SDLK_m)
            {
                g_hit_use_pixel_masks = !g_hit_use_pixel_masks;
                if (g_hit_use_pixel_masks)
                {
                    g_hit_debug_enabled = 1;
                    g_app.show_hit_debug = 1;
                }
                else
                { /* keep overlay state visible if previously on */
                    g_app.show_hit_debug = g_hit_debug_enabled;
                }
                ROGUE_LOG_INFO("hit_pixel_masks_toggle: %d (debug_overlay=%d show_hit_debug=%d)",
                               g_hit_use_pixel_masks, g_hit_debug_enabled, g_app.show_hit_debug);
            }
            /* Pixel mask per-facing adjustment: hold SHIFT and use arrow keys / PageUp/PageDown;
             * press SHIFT+0 to save */
            if ((ks_state2[SDL_SCANCODE_LSHIFT] || ks_state2[SDL_SCANCODE_RSHIFT]))
            {
                RogueHitboxTuning* tune = rogue_hitbox_tuning_get();
                int facing = g_app.player.facing;
                if (facing < 0 || facing > 3)
                    facing = 0;
                float step_px = 1.0f; /* pixel units */
                if (ev.key.keysym.sym == SDLK_UP)
                {
                    tune->mask_dy[facing] -= step_px;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_DOWN)
                {
                    tune->mask_dy[facing] += step_px;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_LEFT)
                {
                    tune->mask_dx[facing] -= step_px;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_RIGHT)
                {
                    tune->mask_dx[facing] += step_px;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_PAGEUP)
                {
                    tune->mask_scale_x[facing] *= 1.05f;
                    tune->mask_scale_y[facing] *= 1.05f;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_PAGEDOWN)
                {
                    tune->mask_scale_x[facing] *= 0.95f;
                    tune->mask_scale_y[facing] *= 0.95f;
                    if (tune->mask_scale_x[facing] < 0.05f)
                        tune->mask_scale_x[facing] = 0.05f;
                    if (tune->mask_scale_y[facing] < 0.05f)
                        tune->mask_scale_y[facing] = 0.05f;
                    return;
                }
                /* Numpad adjustments (independent of NumLock state handled by SDL keysyms) */
                if (ev.key.keysym.sym == SDLK_KP_8)
                {
                    tune->mask_dy[facing] -= step_px;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_KP_2)
                {
                    tune->mask_dy[facing] += step_px;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_KP_4)
                {
                    tune->mask_dx[facing] -= step_px;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_KP_6)
                {
                    tune->mask_dx[facing] += step_px;
                    return;
                }
                /* Numpad scaling: KP9 widen (x only), KP3 heighten (y only) */
                if (ev.key.keysym.sym == SDLK_KP_9)
                {
                    tune->mask_scale_x[facing] *= 1.05f;
                    if (tune->mask_scale_x[facing] < 0.05f)
                        tune->mask_scale_x[facing] = 0.05f;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_KP_3)
                {
                    tune->mask_scale_y[facing] *= 1.05f;
                    if (tune->mask_scale_y[facing] < 0.05f)
                        tune->mask_scale_y[facing] = 0.05f;
                    return;
                }
                /* Fine shrink with KP_MINUS (x) and KP_PLUS (y) */
                if (ev.key.keysym.sym == SDLK_KP_MINUS)
                {
                    tune->mask_scale_x[facing] *= 0.95f;
                    if (tune->mask_scale_x[facing] < 0.05f)
                        tune->mask_scale_x[facing] = 0.05f;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_KP_PLUS)
                {
                    tune->mask_scale_y[facing] *= 0.95f;
                    if (tune->mask_scale_y[facing] < 0.05f)
                        tune->mask_scale_y[facing] = 0.05f;
                    return;
                }
                if (ev.key.keysym.sym == SDLK_0)
                {
                    rogue_hitbox_tuning_save_resolved();
                    return;
                }
                if (ev.key.keysym.sym == SDLK_KP_0)
                {
                    rogue_hitbox_tuning_save_resolved();
                    return;
                }
            }
            /* Hitbox tuning hotkeys: hold CTRL to modify player capsule, ALT for enemy hit circles.
               Number keys 1-9 adjust magnitude; 0 saves to file. Without modifiers we cycle nothing
               (reserved). CTRL + (1/2) adjust player_offset_x +/-; (3/4) adjust player_offset_y;
               (5/6) adjust length; (7/8) adjust width. ALT + (1/2) adjust enemy_offset_x; (3/4)
               enemy_offset_y; (5/6) enemy_radius +/-.
            */
            const Uint8* ks_state = SDL_GetKeyboardState(NULL);
            int ctrl_down = ks_state[SDL_SCANCODE_LCTRL] || ks_state[SDL_SCANCODE_RCTRL];
            int alt_down2 = ks_state[SDL_SCANCODE_LALT] || ks_state[SDL_SCANCODE_RALT];
            int shift_down = ks_state[SDL_SCANCODE_LSHIFT] || ks_state[SDL_SCANCODE_RSHIFT];
            if (ctrl_down || alt_down2 || shift_down)
            {
                RogueHitboxTuning* tune = rogue_hitbox_tuning_get();
                float step = 0.02f; /* base granularity */
                int k = ev.key.keysym.sym;
                if (ctrl_down)
                {
                    if (k == SDLK_1)
                        tune->player_offset_x -= step;
                    else if (k == SDLK_2)
                        tune->player_offset_x += step;
                    else if (k == SDLK_3)
                        tune->player_offset_y -= step;
                    else if (k == SDLK_4)
                        tune->player_offset_y += step;
                    else if (k == SDLK_5)
                        tune->player_length -= step;
                    else if (k == SDLK_6)
                        tune->player_length += step;
                    else if (k == SDLK_7)
                        tune->player_width -= step;
                    else if (k == SDLK_8)
                        tune->player_width += step;
                    else if (k == SDLK_9)
                    {
                        tune->player_offset_x = 0;
                        tune->player_offset_y = 0;
                    }
                    else if (k == SDLK_0)
                    {
                        rogue_hitbox_tuning_save_resolved();
                    }
                }
                else if (alt_down2)
                {
                    if (k == SDLK_1)
                        tune->enemy_offset_x -= step;
                    else if (k == SDLK_2)
                        tune->enemy_offset_x += step;
                    else if (k == SDLK_3)
                        tune->enemy_offset_y -= step;
                    else if (k == SDLK_4)
                        tune->enemy_offset_y += step;
                    else if (k == SDLK_5)
                        tune->enemy_radius -= step;
                    else if (k == SDLK_6)
                        tune->enemy_radius += step;
                    else if (k == SDLK_9)
                    {
                        tune->enemy_offset_x = 0;
                        tune->enemy_offset_y = 0;
                    }
                    else if (k == SDLK_0)
                    {
                        rogue_hitbox_tuning_save_resolved();
                    }
                }
                else if (shift_down)
                {
                    if (k == SDLK_1)
                        tune->pursue_offset_x -= step;
                    else if (k == SDLK_2)
                        tune->pursue_offset_x += step;
                    else if (k == SDLK_3)
                        tune->pursue_offset_y -= step;
                    else if (k == SDLK_4)
                        tune->pursue_offset_y += step;
                    else if (k == SDLK_9)
                    {
                        tune->pursue_offset_x = 0;
                        tune->pursue_offset_y = 0;
                    }
                    else if (k == SDLK_0)
                    {
                        rogue_hitbox_tuning_save_resolved();
                    }
                }
            }
            if (ev.key.keysym.sym == SDLK_F11)
            {
                g_app.gen_noise_gain += 0.02;
                if (g_app.gen_noise_gain > 0.8)
                    g_app.gen_noise_gain = 0.8;
                g_app.gen_params_dirty = 1;
                ev.key.keysym.sym = SDLK_BACKQUOTE;
            }
            if (ev.key.keysym.sym == SDLK_F12)
            {
                g_app.gen_noise_gain -= 0.02;
                if (g_app.gen_noise_gain < 0.3)
                    g_app.gen_noise_gain = 0.3;
                g_app.gen_params_dirty = 1;
                ev.key.keysym.sym = SDLK_BACKQUOTE;
            }
            if (ev.key.keysym.sym == SDLK_BACKQUOTE)
            {
                g_app.pending_seed = (unsigned int) SDL_GetTicks();
                RogueWorldGenConfig wcfg = rogue_world_gen_config_build(g_app.pending_seed, 1, 1);
                rogue_tilemap_free(&g_app.world_map);
                if (!rogue_world_generate_full(&g_app.world_map, &wcfg))
                {
                    rogue_world_generate(&g_app.world_map, &wcfg);
                }
                int sx = 2, sy = 2;
                if (rogue_world_find_random_spawn(&g_app.world_map, wcfg.seed ^ 0xA5A5u, &sx, &sy))
                {
                    g_app.player.base.pos.x = (float) sx + 0.5f;
                    g_app.player.base.pos.y = (float) sy + 0.5f;
                }
                g_app.minimap_dirty = 1;
                /* Regenerate vegetation with same cover and new seed */
                rogue_vegetation_generate(rogue_vegetation_get_tree_cover(), g_app.pending_seed);
            }
            /* Vegetation density adjustments: Alt+[ decreases, Alt+] increases */
            const Uint8* ks = SDL_GetKeyboardState(NULL);
            int alt_down = ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_RALT];
            if (alt_down && ev.key.keysym.sym == SDLK_LEFTBRACKET)
            {
                float c = rogue_vegetation_get_tree_cover();
                c -= 0.02f;
                if (c < 0.0f)
                    c = 0.0f;
                rogue_vegetation_set_tree_cover(c);
            }
            if (alt_down && ev.key.keysym.sym == SDLK_RIGHTBRACKET)
            {
                float c = rogue_vegetation_get_tree_cover();
                c += 0.02f;
                if (c > 0.70f)
                    c = 0.70f;
                rogue_vegetation_set_tree_cover(c);
            }
        }
        if (ev.type == SDL_KEYDOWN && g_app.show_start_screen)
        {
            if (g_app.entering_seed)
            {
                if (ev.key.keysym.sym == SDLK_RETURN)
                {
                    rogue_tilemap_free(&g_app.world_map);
                    RogueWorldGenConfig wcfg =
                        rogue_world_gen_config_build(g_app.pending_seed, 0, 0);
                    if (!rogue_world_generate_full(&g_app.world_map, &wcfg))
                    {
                        rogue_world_generate(&g_app.world_map, &wcfg);
                    }
                    int sx = 2, sy = 2;
                    if (rogue_world_find_random_spawn(&g_app.world_map, wcfg.seed ^ 0x51C3u, &sx,
                                                      &sy))
                    {
                        g_app.player.base.pos.x = (float) sx + 0.5f;
                        g_app.player.base.pos.y = (float) sy + 0.5f;
                    }
                    g_app.chunks_x =
                        (g_app.world_map.width + g_app.chunk_size - 1) / g_app.chunk_size;
                    g_app.chunks_y =
                        (g_app.world_map.height + g_app.chunk_size - 1) / g_app.chunk_size;
                    size_t ctotal = (size_t) g_app.chunks_x * (size_t) g_app.chunks_y;
                    if (ctotal)
                    {
                        g_app.chunk_dirty = (unsigned char*) malloc(ctotal);
                        if (g_app.chunk_dirty)
                            memset(g_app.chunk_dirty, 0, ctotal);
                    }
                    g_app.entering_seed = 0;
                }
                else if (ev.key.keysym.sym == SDLK_ESCAPE)
                {
                    g_app.entering_seed = 0;
                }
            }
        }
    }
#endif
}

void rogue_process_pending_skill_activations(void)
{
    /* Consume queued activations in FIFO order */
    int processed = 0;
    while (g_pending_skill_count > 0)
    {
        int idx = g_pending_skill_head;
        PendingSkillAct* pa = &g_pending_skill_acts[idx];
        RogueSkillCtx ctx = {pa->now_ms, g_app.player.level, g_app.talent_points};
        if (rogue_skill_try_activate(pa->skill_id, &ctx))
        {
            if (pa->bar_slot >= 0 && pa->bar_slot < 10)
                rogue_skill_bar_flash(pa->bar_slot);
        }
        g_pending_skill_head = (g_pending_skill_head + 1) % ROGUE_PENDING_SKILLS_MAX;
        g_pending_skill_count--;
        processed++;
    }
    (void) processed; /* could log for debugging */
}
