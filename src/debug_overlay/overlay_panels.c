#include "../core/app/app_state.h"
#include "../core/audio_vfx/audiovfx_debug.h"
#include "../core/entities/entity_debug.h"
#include "../core/player/player_debug.h"
#include "../core/skills/skill_debug.h"
#include "../core/skills/skills_coeffs.h"
#include "../core/world/map_debug.h"
#include "overlay_core.h"
#include "overlay_input.h"
#include "overlay_widgets.h"
/* Needed for RogueVfxFrameStats definition used in the Audio/VFX panel */
#include "../audio_vfx/effects.h"
#include "../core/integration/state_validation_manager.h"
#include "../core/loot/item_debug.h"
#include "../util/asset_dep.h"

/* Standard headers for string/IO utilities used in this file */
#include <stdio.h>
#include <string.h>

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_system(void* user)
{
    (void) user;
    if (!overlay_begin_panel("System", 10, 10, 320))
        return;
    char buf[128];
    snprintf(buf, sizeof(buf), "FPS: %.1f  (%.3f ms)", g_app.fps, g_app.frame_ms);
    overlay_label(buf);
    snprintf(buf, sizeof(buf), "Draw calls: %d", g_app.frame_draw_calls);
    overlay_label(buf);
    snprintf(buf, sizeof(buf), "Tile quads: %d", g_app.frame_tile_quads);
    overlay_label(buf);
    int flags = g_app.show_metrics_overlay ? 1 : 0;
    if (overlay_checkbox("Show metrics overlay (F1)", &flags))
    {
        g_app.show_metrics_overlay = flags;
        overlay_set_enabled(flags);
    }
    overlay_end_panel();
}

static void panel_player(void* user)
{
    (void) user;
    if (!overlay_begin_panel("Player", 10, 220, 360))
        return;
    /* Health/Mana/AP */
    int hp = rogue_player_debug_get_health();
    int hp_max = rogue_player_debug_get_max_health();
    if (overlay_slider_int("Health", &hp, 0, hp_max))
        rogue_player_debug_set_health(hp);
    int mp = rogue_player_debug_get_mana();
    int mp_max = rogue_player_debug_get_max_mana();
    if (overlay_slider_int("Mana", &mp, 0, mp_max))
        rogue_player_debug_set_mana(mp);
    int ap = rogue_player_debug_get_ap();
    int ap_max = rogue_player_debug_get_max_ap();
    if (overlay_slider_int("Action Points", &ap, 0, ap_max))
        rogue_player_debug_set_ap(ap);

    /* Core stats */
    if (overlay_columns_begin(2, NULL))
    {
        int v;
        v = rogue_player_debug_get_stat(ROGUE_STAT_STRENGTH);
        if (overlay_slider_int("STR", &v, 1, 200))
            rogue_player_debug_set_stat(ROGUE_STAT_STRENGTH, v);
        overlay_next_column();
        v = rogue_player_debug_get_stat(ROGUE_STAT_DEXTERITY);
        if (overlay_slider_int("DEX", &v, 1, 200))
            rogue_player_debug_set_stat(ROGUE_STAT_DEXTERITY, v);
        overlay_next_column();
        v = rogue_player_debug_get_stat(ROGUE_STAT_VITALITY);
        if (overlay_slider_int("VIT", &v, 1, 300))
            rogue_player_debug_set_stat(ROGUE_STAT_VITALITY, v);
        overlay_next_column();
        v = rogue_player_debug_get_stat(ROGUE_STAT_INTELLIGENCE);
        if (overlay_slider_int("INT", &v, 1, 200))
            rogue_player_debug_set_stat(ROGUE_STAT_INTELLIGENCE, v);
        overlay_columns_end();
    }

    /* Toggles */
    int god = rogue_player_debug_get_god_mode();
    if (overlay_checkbox("God Mode", &god))
        rogue_player_debug_set_god_mode(god);
    int noclip = rogue_player_debug_get_noclip();
    if (overlay_checkbox("No-clip", &noclip))
        rogue_player_debug_set_noclip(noclip);

    /* Teleport controls: simple snap buttons */
    if (overlay_button("Teleport to Spawn"))
    {
        float sx = 2.5f, sy = 2.5f;
        rogue_player_debug_teleport(sx, sy);
    }
    if (overlay_button("Teleport to Center"))
    {
        float cx = 0.5f * (float) g_app.world_map.width;
        float cy = 0.5f * (float) g_app.world_map.height;
        rogue_player_debug_teleport(cx, cy);
    }

    overlay_end_panel();
}

static void panel_skills(void* user)
{
    (void) user;
    if (!overlay_begin_panel("Skills", 380, 10, 420))
        return;
    /* Simulation profile controls (persist across frames) */
    static float sim_duration_ms = 2000.0f;
    static float sim_tick_ms = 16.0f;
    static float sim_ap_regen_per_sec = 0.0f;
    static char prio_buf[128] = ""; /* comma-separated ids, empty = selected only */
    static char sim_result[256] = "";
    const char* overrides_path = "build/skills_overrides.json"; /* default within repo/build */
    const char* base_skills_path = "assets/skills_uhf87f.json";
    static int auto_reload = 1;
    static int auto_reload_base = 0;
    int count = rogue_skill_debug_count();
    static int sel = 0;
    if (sel < 0)
        sel = 0;
    if (sel >= count)
        sel = count - 1;
    /* Selection slider */
    if (count <= 0)
    {
        overlay_label("No skills registered");
        overlay_end_panel();
        return;
    }
    if (overlay_slider_int("Skill Index", &sel, 0, count - 1))
    {
        /* keep within bounds */
        if (sel < 0)
            sel = 0;
        if (sel >= count)
            sel = count - 1;
    }
    /* Name */
    const char* name = rogue_skill_debug_name(sel);
    char buf[256];
    snprintf(buf, sizeof buf, "[%d] %s", sel, name ? name : "<noname>");
    overlay_label(buf);

    /* Timing fields */
    float base_cd = 0.f, cd_red = 0.f, cast_ms = 0.f;
    if (rogue_skill_debug_get_timing(sel, &base_cd, &cd_red, &cast_ms) == 0)
    {
        if (overlay_slider_float("Base Cooldown (ms)", &base_cd, 0.f, 60000.f) ||
            overlay_slider_float("CD Reduction/rank (ms)", &cd_red, -1000.f, 1000.f) ||
            overlay_slider_float("Cast Time (ms)", &cast_ms, 0.f, 5000.f))
        {
            rogue_skill_debug_set_timing(sel, base_cd, cd_red, cast_ms);
            /* auto-save */
            (void) rogue_skill_debug_save_overrides(overrides_path);
        }
    }

    /* Coeff params */
    RogueSkillCoeffParams cp;
    if (rogue_skill_debug_get_coeff(sel, &cp) == 0)
    {
        int changed = 0;
        changed |= overlay_slider_float("Coeff Base", &cp.base_scalar, 0.0f, 10.0f);
        changed |= overlay_slider_float("Coeff per Rank", &cp.per_rank_scalar, -1.0f, 5.0f);
        changed |= overlay_slider_float("STR %/10", &cp.str_pct_per10, -50.0f, 200.0f);
        changed |= overlay_slider_float("INT %/10", &cp.int_pct_per10, -50.0f, 200.0f);
        changed |= overlay_slider_float("DEX %/10", &cp.dex_pct_per10, -50.0f, 200.0f);
        changed |= overlay_slider_float("Stat Cap %", &cp.stat_cap_pct, 0.0f, 200.0f);
        changed |= overlay_slider_float("Stat Softness", &cp.stat_softness, 0.1f, 10.0f);
        if (changed)
        {
            rogue_skill_debug_set_coeff(sel, &cp);
            (void) rogue_skill_debug_save_overrides(overrides_path);
        }
    }

    /* Simulation profile UI */
    overlay_label("Simulation Profile");
    (void) overlay_slider_float("Duration (ms)", &sim_duration_ms, 50.0f, 60000.0f);
    (void) overlay_slider_float("Tick (ms)", &sim_tick_ms, 1.0f, 100.0f);
    (void) overlay_slider_float("AP regen (/sec)", &sim_ap_regen_per_sec, 0.0f, 200.0f);
    (void) overlay_input_text("Priority IDs (comma)", prio_buf, sizeof prio_buf);

    /* Simulate with current profile */
    if (overlay_button("Simulate"))
    {
        char profile[256];
        /* Build priority array */
        char prio_json[128] = {0};
        int pj = 0;
        prio_json[pj++] = '[';
        if (prio_buf[0] == '\0')
        {
            pj += snprintf(prio_json + pj, (int) sizeof prio_json - pj, "%d", sel);
        }
        else
        {
            /* Copy digits and commas only to be tolerant */
            for (const char* p = prio_buf; *p && pj + 1 < (int) sizeof prio_json; ++p)
            {
                char c = *p;
                if ((c >= '0' && c <= '9') || c == ',' || c == '-')
                {
                    prio_json[pj++] = c;
                }
            }
        }
        if (pj + 2 < (int) sizeof prio_json)
        {
            prio_json[pj++] = ']';
            prio_json[pj] = '\0';
        }
        else
        {
            prio_json[0] = '[';
            prio_json[1] = ']';
            prio_json[2] = '\0';
        }
        snprintf(profile, sizeof profile,
                 "{\"duration_ms\":%d,\"tick_ms\":%.1f,\"ap_regen_per_sec\":%.1f,\"priority\":%s}",
                 (int) sim_duration_ms, sim_tick_ms, sim_ap_regen_per_sec, prio_json);
        if (rogue_skill_debug_simulate(profile, sim_result, (int) sizeof sim_result) != 0)
        {
            snprintf(sim_result, sizeof sim_result, "Simulation failed");
        }
    }
    if (sim_result[0])
    {
        overlay_label(sim_result);
    }

    /* Manual Save/Load buttons */
    if (overlay_button("Save Overrides JSON"))
    {
        int rc = rogue_skill_debug_save_overrides(overrides_path);
        char msg[128];
        snprintf(msg, sizeof msg, "Save: %s (%d)", (rc == 0 ? "OK" : "ERR"), rc);
        overlay_label(msg);
    }
    if (overlay_button("Load Overrides JSON"))
    {
        int applied = rogue_skill_debug_load_overrides_file(overrides_path);
        char msg[128];
        snprintf(msg, sizeof msg, "Load: %s (%d)", (applied >= 0 ? "OK" : "ERR"), applied);
        overlay_label(msg);
    }

    /* Auto-Reload toggles and ticks */
    if (overlay_checkbox("Auto-Reload Overrides", &auto_reload))
    {
        /* no-op: state persisted in static */
    }
    if (auto_reload)
    {
        int applied = rogue_skill_debug_autoreload_tick(overrides_path);
        if (applied > 0)
        {
            char msg[96];
            snprintf(msg, sizeof msg, "Auto-Reload applied: %d entries", applied);
            overlay_label(msg);
        }
    }
    if (overlay_checkbox("Auto-Reload Base Skills JSON", &auto_reload_base))
    {
        /* no-op */
    }
    if (auto_reload_base)
    {
        int loaded = rogue_skills_base_autoreload_tick(base_skills_path);
        if (loaded > 0)
        {
            char msg[96];
            snprintf(msg, sizeof msg, "Base reload: %d skills loaded", loaded);
            overlay_label(msg);
        }
    }

    overlay_end_panel();
}

static void panel_entities(void* user)
{
    (void) user;
    if (!overlay_begin_panel("Entities", 820, 10, 360))
        return;
    static int selected_slot = -1;
    int total = rogue_entity_debug_count();
    char hdr[64];
    snprintf(hdr, sizeof hdr, "Alive: %d", total);
    overlay_label(hdr);
    /* Simple selection via index slider using compact list of alive indices */
    int idxs[64];
    int n = rogue_entity_debug_list(idxs, (int) (sizeof idxs / sizeof idxs[0]));
    if (n <= 0)
    {
        overlay_label("No enemies alive");
        if (overlay_button("Spawn @ Player+2,0"))
        {
            int si = rogue_entity_debug_spawn_at_player(2.0f, 0.0f);
            if (si >= 0)
                selected_slot = si;
        }
        overlay_end_panel();
        return;
    }
    /* Represent selection by index into the compact list for stable navigation */
    static int sel_i = 0;
    if (sel_i < 0)
        sel_i = 0;
    if (sel_i >= n)
        sel_i = n - 1;
    if (overlay_slider_int("Select", &sel_i, 0, n - 1))
    {
        selected_slot = idxs[sel_i];
    }
    if (selected_slot < 0 || selected_slot >= ROGUE_MAX_ENEMIES)
        selected_slot = idxs[sel_i];
    RogueEntityDebugInfo info;
    if (rogue_entity_debug_get_info(selected_slot, &info) == 0 && info.alive)
    {
        char line[128];
        snprintf(line, sizeof line, "Slot %d  Type %d  HP %d/%d", info.slot_index, info.type_index,
                 info.health, info.max_health);
        overlay_label(line);
        snprintf(line, sizeof line, "Pos: %.2f, %.2f", info.x, info.y);
        overlay_label(line);

        if (overlay_columns_begin(2, NULL))
        {
            if (overlay_button("Kill"))
            {
                (void) rogue_entity_debug_kill(info.slot_index);
            }
            overlay_next_column();
            if (overlay_button("Teleport -> Player"))
            {
                (void) rogue_entity_debug_teleport(info.slot_index, g_app.player.base.pos.x,
                                                   g_app.player.base.pos.y);
            }
            overlay_columns_end();
        }
    }
    else
    {
        overlay_label("Selection not alive");
    }

    if (overlay_button("Spawn @ Player+2,0"))
    {
        int si = rogue_entity_debug_spawn_at_player(2.0f, 0.0f);
        if (si >= 0)
        {
            selected_slot = si;
            sel_i = 0; /* will be corrected next frame */
        }
    }

    overlay_end_panel();
}

static void panel_map_editor(void* user)
{
    (void) user;
    if (!overlay_begin_panel("Map Editor", 1190, 10, 360))
        return;
    /* Tileset picker + brush controls */
    static int brush_radius = 1;
    static int brush_mode = 0;                     /* 0 = square, 1 = rect */
    static int erase_mode = 0;                     /* when set, paint EMPTY */
    static int tile_val = 1;                       /* default GRASS */
    static int rx0 = 0, ry0 = 0, rx1 = 7, ry1 = 7; /* rect inputs */
    static char path_buf[128] = "build/map.json";

    if (brush_radius < 0)
        brush_radius = 0;

    /* Tile name helper for readability */
    const char* tile_names[ROGUE_TILE_MAX] = {
        "EMPTY",       "WATER",        "GRASS",      "FOREST",   "MOUNTAIN",
        "CAVE_WALL",   "CAVE_FLOOR",   "RIVER",      "SWAMP",    "SNOW",
        "RIVER_DELTA", "RIVER_WIDE",   "LAVA",       "ORE_VEIN", "BRIDGE_HINT",
        "STRUCT_WALL", "STRUCT_FLOOR", "DNG_ENTR",   "DNG_WALL", "DNG_FLOOR",
        "DNG_DOOR",    "DNG_LOCKED",   "DNG_SECRET", "DNG_TRAP", "DNG_KEY"};

    int max_tile = ROGUE_TILE_MAX - 1;
    if (tile_val < 0)
        tile_val = 0;
    if (tile_val > max_tile)
        tile_val = max_tile;

    /* Tile selection */
    overlay_slider_int("Tile ID", &tile_val, 0, max_tile);
    {
        char lbl[96];
        const char* nm = (tile_val >= 0 && tile_val < ROGUE_TILE_MAX) ? tile_names[tile_val] : "?";
        snprintf(lbl, sizeof lbl, "Selected: [%d] %s", tile_val, nm);
        overlay_label(lbl);
    }

    /* Erase toggle (paints EMPTY regardless of tile selection) */
    overlay_checkbox("Erase (paint EMPTY)", &erase_mode);

    /* Brush mode via combo */
    {
        const char* modes[] = {"Square", "Rect"};
        (void) overlay_combo("Brush Mode", &brush_mode, modes, 2);
    }
    if (brush_mode == 0)
    {
        overlay_slider_int("Square Radius", &brush_radius, 0, 32);
        if (overlay_columns_begin(2, NULL))
        {
            if (overlay_button("Paint at Center"))
            {
                int cx = g_app.world_map.width / 2;
                int cy = g_app.world_map.height / 2;
                unsigned char v = (unsigned char) (erase_mode ? 0 : tile_val);
                (void) rogue_map_debug_brush_square(cx, cy, brush_radius, v);
            }
            overlay_next_column();
            if (overlay_button("Paint at Player"))
            {
                int px = (int) g_app.player.base.pos.x;
                int py = (int) g_app.player.base.pos.y;
                if (px < 0)
                    px = 0;
                if (py < 0)
                    py = 0;
                if (px >= g_app.world_map.width)
                    px = g_app.world_map.width - 1;
                if (py >= g_app.world_map.height)
                    py = g_app.world_map.height - 1;
                unsigned char v = (unsigned char) (erase_mode ? 0 : tile_val);
                (void) rogue_map_debug_brush_square(px, py, brush_radius, v);
            }
            overlay_columns_end();
        }
    }
    else
    {
        /* Rect inputs */
        overlay_slider_int("x0", &rx0, 0,
                           (g_app.world_map.width > 0) ? g_app.world_map.width - 1 : 0);
        overlay_slider_int("y0", &ry0, 0,
                           (g_app.world_map.height > 0) ? g_app.world_map.height - 1 : 0);
        overlay_slider_int("x1", &rx1, 0,
                           (g_app.world_map.width > 0) ? g_app.world_map.width - 1 : 0);
        overlay_slider_int("y1", &ry1, 0,
                           (g_app.world_map.height > 0) ? g_app.world_map.height - 1 : 0);
        if (overlay_button("Paint Rect"))
        {
            unsigned char v = (unsigned char) (erase_mode ? 0 : tile_val);
            (void) rogue_map_debug_brush_rect(rx0, ry0, rx1, ry1, v);
        }
    }

    /* Utilities */
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_button("Pick Under Player"))
        {
            int px = (int) g_app.player.base.pos.x;
            int py = (int) g_app.player.base.pos.y;
            if (px >= 0 && py >= 0 && px < g_app.world_map.width && py < g_app.world_map.height)
            {
                tile_val = (int) g_app.world_map.tiles[py * g_app.world_map.width + px];
            }
        }
        overlay_next_column();
        if (overlay_button("Fill Entire Map"))
        {
            unsigned char v = (unsigned char) (erase_mode ? 0 : tile_val);
            (void) rogue_map_debug_brush_rect(0, 0, g_app.world_map.width - 1,
                                              g_app.world_map.height - 1, v);
        }
        overlay_columns_end();
    }

    /* Advanced block in a collapsible tree */
    {
        static int adv_open = 0;
        if (overlay_tree_node("Advanced", &adv_open))
        {
            if (overlay_button("Clear (EMPTY)"))
            {
                (void) rogue_map_debug_brush_rect(0, 0, g_app.world_map.width - 1,
                                                  g_app.world_map.height - 1, 0);
            }
            overlay_tree_pop();
        }
    }

    /* Save/Load path controls */
    overlay_input_text("Map JSON Path", path_buf, sizeof path_buf);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_button("Save JSON"))
        {
            int rc = rogue_map_debug_save_json(path_buf);
            char msg[64];
            snprintf(msg, sizeof msg, "save rc=%d", rc);
            overlay_label(msg);
        }
        overlay_next_column();
        if (overlay_button("Load JSON"))
        {
            int rc = rogue_map_debug_load_json(path_buf);
            char msg[64];
            snprintf(msg, sizeof msg, "load rc=%d", rc);
            overlay_label(msg);
        }
        overlay_columns_end();
    }

    overlay_end_panel();
}

static void panel_audiovfx(void* user)
{
    (void) user;
    if (!overlay_begin_panel("Audio / VFX", 10, 590, 380))
        return;
    /* Simple inputs: audio id, vfx id, and spawn at cursor */
    static char audio_id[32] = "click";
    static char vfx_id[32] = "SPARKLE";
    overlay_input_text("Audio ID", audio_id, sizeof audio_id);
    overlay_input_text("VFX ID", vfx_id, sizeof vfx_id);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_button("Play Sound"))
            (void) rogue_audiovfx_debug_play(audio_id);
        overlay_next_column();
        if (overlay_button("Spawn VFX @ Cursor"))
        {
            const OverlayInputState* in = overlay_input_get();
            (void) rogue_audiovfx_debug_spawn_at_cursor(vfx_id, in->mouse_x, in->mouse_y);
        }
        overlay_columns_end();
    }

    /* Mixer controls */
    static float master = 1.0f;
    static float cat_sfx = 1.0f;
    static float cat_ui = 1.0f;
    static int mute = 0;
    if (overlay_slider_float("Master", &master, 0.0f, 1.0f))
        rogue_audiovfx_debug_set_master(master);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_slider_float("SFX", &cat_sfx, 0.0f, 1.0f))
            rogue_audiovfx_debug_set_category(0, cat_sfx);
        overlay_next_column();
        if (overlay_slider_float("UI", &cat_ui, 0.0f, 1.0f))
            rogue_audiovfx_debug_set_category(1, cat_ui);
        overlay_columns_end();
    }
    if (overlay_checkbox("Mute", &mute))
        rogue_audiovfx_debug_set_mute(mute);

    /* VFX perf controls */
    static float perf = 1.0f;
    static int soft_cap = 0;
    static int hard_cap = 0;
    if (overlay_slider_float("VFX Perf Scale", &perf, 0.1f, 1.0f))
        rogue_audiovfx_debug_set_perf(perf);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_slider_int("Soft Budget", &soft_cap, 0, 2000))
            rogue_audiovfx_debug_set_budgets(soft_cap, hard_cap);
        overlay_next_column();
        if (overlay_slider_int("Hard Budget", &hard_cap, 0, 4000))
            rogue_audiovfx_debug_set_budgets(soft_cap, hard_cap);
        overlay_columns_end();
    }

    /* Stats readout */
    struct RogueVfxFrameStats st = {0};
    rogue_audiovfx_debug_get_last_stats(&st);
    char buf[160];
    snprintf(buf, sizeof buf,
             "parts: %d  inst: %d  spawned(core:%d trail:%d) culled(s:%d h:%d p:%d)",
             st.active_particles, st.active_instances, st.spawned_core, st.spawned_trail,
             st.culled_soft, st.culled_hard, st.culled_pacing);
    overlay_label(buf);

    overlay_end_panel();
}

static void panel_items(void* user)
{
    (void) user;
#if ROGUE_ENABLE_DEBUG_OVERLAY
    if (!overlay_begin_panel("Items", 1550, 10, 360))
        return;

    /* Table of items */
    static int sel = 0;
    static int sort_col = 0, sort_dir = 1;
    const char* headers[] = {"ID", "Name", "Cat", "Lvl"};
    int count = rogue_item_debug_count();
    if (count < 0)
        count = 0;
    if (sel < 0)
        sel = 0;
    if (sel >= count)
        sel = count - 1;
    if (overlay_table_begin("items", headers, 4, &sort_col, &sort_dir, NULL))
    {
        /* No sorting implemented yet; render in registry order */
        for (int i = 0; i < count; ++i)
        {
            const RogueItemDef* d = rogue_item_debug_get(i);
            if (!d)
                continue;
            char id[64];
            char nm[80];
            char cat[16];
            char lvl[16];
            snprintf(id, sizeof id, "%s", d->id);
            snprintf(nm, sizeof nm, "%s", d->name);
            snprintf(cat, sizeof cat, "%d", (int) d->category);
            snprintf(lvl, sizeof lvl, "%d", d->level_req);
            const char* cells[] = {id, nm, cat, lvl};
            (void) overlay_table_row(cells, 4, i, &sel);
        }
        overlay_table_end();
    }

    /* Selected item editor */
    if (sel >= 0 && sel < count)
    {
        const RogueItemDef* d = rogue_item_debug_get(sel);
        if (d)
        {
            static int last_sel = -1;
            static char name_buf[ROGUE_MAX_ITEM_NAME_LEN];
            if (last_sel != sel)
            {
                /* refresh buffer on selection change */
#if defined(_MSC_VER)
                strncpy_s(name_buf, sizeof name_buf, d->name, _TRUNCATE);
#else
                strncpy(name_buf, d->name, sizeof name_buf - 1);
                name_buf[sizeof name_buf - 1] = '\0';
#endif
                last_sel = sel;
            }
            overlay_label("Edit Selected:");
            if (overlay_input_text("Name", name_buf, (int) sizeof name_buf))
            {
                (void) rogue_item_debug_set_name(sel, name_buf);
            }
            int v;
            v = d->level_req;
            if (overlay_slider_int("Level Req", &v, 1, 100))
                (void) rogue_item_debug_set_int(sel, "level_req", v);
            v = d->stack_max;
            if (overlay_slider_int("Stack Max", &v, 1, 999))
                (void) rogue_item_debug_set_int(sel, "stack_max", v);
            if (overlay_columns_begin(2, NULL))
            {
                v = d->base_damage_min;
                if (overlay_slider_int("Dmg Min", &v, 0, 999))
                    (void) rogue_item_debug_set_int(sel, "base_damage_min", v);
                overlay_next_column();
                v = d->base_damage_max;
                if (overlay_slider_int("Dmg Max", &v, 0, 999))
                    (void) rogue_item_debug_set_int(sel, "base_damage_max", v);
                overlay_columns_end();
            }
            v = d->base_armor;
            if (overlay_slider_int("Armor", &v, 0, 999))
                (void) rogue_item_debug_set_int(sel, "base_armor", v);
            v = d->rarity;
            if (overlay_slider_int("Rarity", &v, 0, 5))
                (void) rogue_item_debug_set_int(sel, "rarity", v);
            if (overlay_columns_begin(2, NULL))
            {
                v = d->socket_min;
                if (overlay_slider_int("Sock Min", &v, 0, 6))
                    (void) rogue_item_debug_set_int(sel, "socket_min", v);
                overlay_next_column();
                v = d->socket_max;
                if (overlay_slider_int("Sock Max", &v, 0, 6))
                    (void) rogue_item_debug_set_int(sel, "socket_max", v);
                overlay_columns_end();
            }
        }
    }

    /* Save/Load overrides JSON */
    static char path_buf[128] = "build/items_overrides.json";
    overlay_input_text("Items JSON Path", path_buf, (int) sizeof path_buf);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_button("Save JSON"))
        {
            int rc = rogue_item_debug_save_json(path_buf);
            char msg[64];
            snprintf(msg, sizeof msg, "save rc=%d", rc);
            overlay_label(msg);
        }
        overlay_next_column();
        if (overlay_button("Load JSON"))
        {
            int added = rogue_item_debug_load_json(path_buf);
            char msg[64];
            snprintf(msg, sizeof msg, "load added=%d", added);
            overlay_label(msg);
        }
        overlay_columns_end();
    }

    /* Item Creation Wizard (Phase 10.4) */
    static int wizard_open = 1;
    if (overlay_tree_node("Create New Item", &wizard_open))
    {
        static char new_id[ROGUE_MAX_ITEM_ID_LEN] = "";
        static char new_name[ROGUE_MAX_ITEM_NAME_LEN] = "";
        static int cat_idx = (int) ROGUE_ITEM_MISC;
        static int lvl = 1, stack_max = 1, value = 1;
        static int dmg_min = 0, dmg_max = 0, armor = 0, rarity = 0;
        static int sock_min = 0, sock_max = 0;

        overlay_input_text("ID", new_id, (int) sizeof new_id);
        overlay_input_text("Name", new_name, (int) sizeof new_name);
        const char* cat_items[] = {"MISC", "CONSUMABLE", "WEAPON", "ARMOR", "GEM", "MATERIAL"};
        overlay_combo("Category", &cat_idx, cat_items, 6);
        overlay_slider_int("Level Req", &lvl, 1, 100);
        overlay_slider_int("Stack Max", &stack_max, 1, 999);
        overlay_slider_int("Base Value", &value, 0, 100000);
        if (overlay_columns_begin(2, NULL))
        {
            overlay_slider_int("Dmg Min", &dmg_min, 0, 9999);
            overlay_next_column();
            overlay_slider_int("Dmg Max", &dmg_max, 0, 9999);
            overlay_columns_end();
        }
        overlay_slider_int("Base Armor", &armor, 0, 9999);
        overlay_slider_int("Rarity", &rarity, 0, 5);
        if (overlay_columns_begin(2, NULL))
        {
            overlay_slider_int("Sock Min", &sock_min, 0, 6);
            overlay_next_column();
            overlay_slider_int("Sock Max", &sock_max, 0, 6);
            overlay_columns_end();
        }
        if (overlay_button("Create"))
        {
            if (new_id[0] && new_name[0])
            {
                int idx = rogue_item_debug_create(new_id, new_name, (RogueItemCategory) cat_idx,
                                                  lvl, stack_max, value, dmg_min, dmg_max, armor,
                                                  rarity, sock_min, sock_max);
                char msg[96];
                snprintf(msg, sizeof msg, "create idx=%d", idx);
                overlay_label(msg);
                if (idx >= 0)
                {
                    /* select the new item and clear form */
                    sel = idx;
                    new_id[0] = '\0';
                    new_name[0] = '\0';
                    dmg_min = dmg_max = armor = rarity = sock_min = sock_max = 0;
                    stack_max = 1;
                    value = 1;
                    lvl = 1;
                    cat_idx = (int) ROGUE_ITEM_MISC;
                }
            }
            else
            {
                overlay_label("Error: ID and Name are required.");
            }
        }
        overlay_tree_pop();
    }

    overlay_end_panel();
#endif
}

static void panel_validation(void* user)
{
    (void) user;
    if (!overlay_begin_panel("Validation", 1550, 390, 360))
        return;
    // Controls
    static int force_all = 0;
    overlay_checkbox("Force All (ignore snapshot skip)", &force_all);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_button("Run Now"))
        {
            (void) rogue_validation_run_now(force_all ? 1 : 0);
        }
        overlay_next_column();
        static int interval = 120;
        if (overlay_slider_int("Interval (ticks)", &interval, 0, 600))
        {
            rogue_validation_set_interval((uint32_t) interval);
        }
        overlay_columns_end();
    }

    // Stats
    RogueValidationStats st = {0};
    rogue_validation_get_stats(&st);
    char line[160];
    snprintf(line, sizeof line,
             "runs: %llu done: %llu sys: %llu skipped: %llu cross: %llu warn: %llu corrupt: %llu",
             (unsigned long long) st.runs_initiated, (unsigned long long) st.runs_completed,
             (unsigned long long) st.system_validations_run,
             (unsigned long long) st.system_validations_skipped_unchanged,
             (unsigned long long) st.cross_rule_runs, (unsigned long long) st.warnings,
             (unsigned long long) st.corruptions_detected);
    overlay_label(line);

    // Events
    const RogueValidationEvent* evs = NULL;
    size_t count = 0;
    if (rogue_validation_events_get(&evs, &count) == 0 && count > 0 && evs)
    {
        size_t max_show = count < 16 ? count : 16; // show up to last 16
        for (size_t i = 0; i < max_show; ++i)
        {
            const RogueValidationEvent* e = &evs[i];
            char msg[192];
            const char* sev = (e->severity == ROGUE_VALID_OK)     ? "OK"
                              : (e->severity == ROGUE_VALID_WARN) ? "WARN"
                                                                  : "CORRUPT";
            snprintf(msg, sizeof msg, "#%llu t=%llu sys=%d %s code=%u %s%s msg=%s",
                     (unsigned long long) e->seq, (unsigned long long) e->tick, e->system_id, sev,
                     e->code, e->repair_attempted ? "repaired:" : "",
                     (e->repair_attempted && e->repair_success)
                         ? "ok"
                         : (e->repair_attempted ? "fail" : ""),
                     e->message);
            overlay_label(msg);
        }
    }
    else
    {
        overlay_label("No events yet.");
    }

    overlay_end_panel();
}

/* Helper: does node `src_id` list `target_id` as a direct dependency? */
static int content_graph_has_dep_on(const char* src_id, const char* target_id)
{
    const char* deps[16];
    int dc =
        rogue_asset_dep_get_deps(src_id ? src_id : "", deps, (int) (sizeof deps / sizeof deps[0]));
    for (int i = 0; i < dc; ++i)
        if (deps[i] && target_id && strcmp(deps[i], target_id) == 0)
            return 1;
    return 0;
}

/* Helper: is `maybe_dep_id` a direct dependency of node `of_id`? */
static int content_graph_is_dep_of(const char* maybe_dep_id, const char* of_id)
{
    const char* deps[16];
    int dc =
        rogue_asset_dep_get_deps(of_id ? of_id : "", deps, (int) (sizeof deps / sizeof deps[0]));
    for (int i = 0; i < dc; ++i)
        if (deps[i] && maybe_dep_id && strcmp(deps[i], maybe_dep_id) == 0)
            return 1;
    return 0;
}

/* Helper: collect forward reachable nodes up to max_depth using a simple BFS.
 * out_ids/out_depths are parallel arrays filled with unique node ids and their depth from root.
 * out_edges holds pairs of indices (src,dst) into out_ids.
 * Returns node count; writes edge count to *out_edge_count. Caps by max_nodes and max_edges. */
static int content_graph_collect_forward(const char* root_id, int max_depth, const char** out_ids,
                                         int* out_depths, int max_nodes, int (*out_edges)[2],
                                         int max_edges, int* out_edge_count)
{
    if (!root_id || !out_ids || !out_depths || max_nodes <= 0 || !out_edges || max_edges <= 0 ||
        !out_edge_count)
        return 0;
    int nc = 0;
    int ec = 0;
    /* enqueue root */
    out_ids[nc] = root_id;
    out_depths[nc] = 0;
    int qh = 0, qt = 0;
    int qidx[128];
    qidx[qt++] = nc;
    nc++;
    while (qh < qt)
    {
        int si = qidx[qh++];
        const char* sid = out_ids[si];
        int sd = out_depths[si];
        if (sd >= max_depth)
            continue;
        const char* deps[16];
        int dc =
            rogue_asset_dep_get_deps(sid ? sid : "", deps, (int) (sizeof deps / sizeof deps[0]));
        for (int i = 0; i < dc; ++i)
        {
            const char* did = deps[i];
            if (!did)
                continue;
            /* find existing */
            int di = -1;
            for (int k = 0; k < nc; ++k)
            {
                if (out_ids[k] && strcmp(out_ids[k], did) == 0)
                {
                    di = k;
                    break;
                }
            }
            if (di < 0)
            {
                if (nc < max_nodes)
                {
                    di = nc;
                    out_ids[nc] = did;
                    out_depths[nc] = sd + 1;
                    qidx[qt++ % (int) (sizeof qidx / sizeof qidx[0])] = nc;
                    nc++;
                }
                else
                {
                    /* node cap reached; still record edge to a best-effort node 0 (root) to avoid
                     * overflow */
                    di = 0;
                }
            }
            if (ec < max_edges)
            {
                out_edges[ec][0] = si;
                out_edges[ec][1] = di;
                ec++;
            }
        }
    }
    *out_edge_count = ec;
    return nc;
}

static void panel_content_graph(void* user)
{
    (void) user;
    if (!overlay_begin_panel("Content Graph", 1920 - 380, 10, 360))
        return;
    /* Filter + selection + dependency list; group nodes by top-level prefix before '/' */
    static int sel = 0;
    static char filter[64] = "";
    static int draw_edges = 1;    /* simple on-panel edge preview for selected node */
    static int preview_depth = 2; /* multi-hop preview depth (>=1) */
    static int group_only = 0; /* when on, filter list is constrained to selected group's prefix */
    /* Navigation breadcrumbs for click-to-drill within the SDL preview */
    static const char* crumbs[32];
    static int crumb_len = 0;
    overlay_input_text("Filter (substring)", filter, sizeof filter);
    /* Quick action: compute & cache all node hashes to surface issues early */
    if (overlay_button("Compute All Hashes"))
    {
        int total = rogue_asset_dep_count();
        for (int i = 0; i < total; ++i)
        {
            const char *nid = NULL, *pp = NULL;
            if (rogue_asset_dep_get(i, &nid, &pp) == 0 && nid)
            {
                unsigned long long h = 0ULL;
                (void) rogue_asset_dep_hash(nid, &h);
            }
        }
    }
    /* Export a simple Graphviz DOT to build/content_graph.dot for offline visualization */
    if (overlay_button("Export DOT (build/content_graph.dot)"))
    {
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, "build/content_graph.dot", "wb");
#else
        f = fopen("build/content_graph.dot", "wb");
#endif
        if (f)
        {
            fputs("digraph ContentGraph {\n  rankdir=LR;\n  node [shape=box,fontname=Helvetica];\n",
                  f);
            int nall = rogue_asset_dep_count();
            for (int i = 0; i < nall; ++i)
            {
                const char *nid = NULL, *pp = NULL;
                if (rogue_asset_dep_get(i, &nid, &pp) != 0 || !nid)
                    continue;
                /* node label includes id and short path */
                char lbl[512];
                snprintf(lbl, sizeof lbl, "%s\\n%s", nid, (pp && *pp) ? pp : "<none>");
                /* escape quotes minimally by replacing with apostrophes */
                for (char* p = lbl; *p; ++p)
                    if (*p == '"')
                        *p = '\'';
                fprintf(f, "  \"%s\" [label=\"%s\"];\n", nid, lbl);
                const char* deps[16];
                int dc = rogue_asset_dep_get_deps(nid, deps, (int) (sizeof deps / sizeof deps[0]));
                for (int j = 0; j < dc; ++j)
                {
                    if (deps[j])
                        fprintf(f, "  \"%s\" -> \"%s\";\n", nid, deps[j]);
                }
            }
            fputs("}\n", f);
            fclose(f);
            overlay_label("DOT exported.");
        }
        else
        {
            overlay_label("Failed to open output file.");
        }
    }
    /* Export a simple JSON to build/content_graph.json for offline analysis */
    if (overlay_button("Export JSON (build/content_graph.json)"))
    {
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, "build/content_graph.json", "wb");
#else
        f = fopen("build/content_graph.json", "wb");
#endif
        if (f)
        {
            /* Shape: {"nodes":[{"id":"...","hash":"0x...","deps":["..."]}, ...]} */
            fputs("{\n  \"nodes\": [\n", f);
            int nall = rogue_asset_dep_count();
            int wrote = 0;
            for (int i = 0; i < nall; ++i)
            {
                const char *nid = NULL, *pp = NULL;
                if (rogue_asset_dep_get(i, &nid, &pp) != 0 || !nid)
                    continue;
                if (wrote)
                    fputs(",\n", f);
                unsigned long long hv = 0ULL;
                (void) rogue_asset_dep_hash(nid, &hv);
                fprintf(f, "    {\"id\":\"%s\",\"hash\":\"0x%016llx\",\"deps\":[", nid, hv);
                const char* deps[64];
                int dc = rogue_asset_dep_get_deps(nid, deps, (int) (sizeof deps / sizeof deps[0]));
                int wrote_dep = 0;
                for (int j = 0; j < dc; ++j)
                {
                    const char* did = deps[j];
                    if (!did || !did[0])
                        continue;
                    if (wrote_dep)
                        fputs(",", f);
                    fprintf(f, "\"%s\"", did);
                    wrote_dep = 1;
                }
                fputs("]}", f);
                wrote = 1;
            }
            fputs("\n  ]\n}\n", f);
            fclose(f);
            overlay_label("JSON exported.");
        }
        else
        {
            overlay_label("Failed to open output file.");
        }
    }
    int n = rogue_asset_dep_count();
    if (n <= 0)
    {
        overlay_label("No content graph nodes registered.");
        overlay_end_panel();
        return;
    }
    /* Build a compact list of indices that match the filter (and optionally the selected group).
       Support advanced prefixes: id:, path:, dep:X (deps of X), rev:X (nodes that depend on X),
       group:prefix, hash:HEX. Fallback: substring match over id/path. */
    int idxs[256];
    int idx_count = 0;
    /* First pass: if group_only is enabled and we already have a valid selection, derive its group
     */
    const char* cur_group = NULL;
    if (group_only && sel >= 0 && sel < n)
    {
        const char *sid = NULL, *sp = NULL;
        if (rogue_asset_dep_get(sel, &sid, &sp) == 0 && sid)
        {
            const char* s = strchr(sid, '/');
            if (s)
                cur_group = sid; /* we'll compare prefix length below */
        }
    }
    /* Pre-parse advanced filter */
    const char* f = filter;
    const char* prefix = NULL; /* id|path|dep|rev|group|hash */
    const char* farg = NULL;
    if (f && *f)
    {
        const char* colon = strchr(f, ':');
        if (colon)
        {
            static char key[16];
            size_t kl = (size_t) (colon - f);
            if (kl >= sizeof key)
                kl = sizeof key - 1;
            memcpy(key, f, kl);
            key[kl] = '\0';
            prefix = key;
            farg = colon + 1;
        }
    }

    /* Use file-static helpers defined above */

    for (int i = 0; i < n && idx_count < (int) (sizeof idxs / sizeof idxs[0]); ++i)
    {
        const char *nid = NULL, *pp = NULL;
        if (rogue_asset_dep_get(i, &nid, &pp) == 0)
        {
            int passes_text = 1;
            if (f && *f)
            {
                if (prefix && farg && *prefix)
                {
                    if (strcmp(prefix, "id") == 0)
                        passes_text = (nid && strstr(nid, farg)) ? 1 : 0;
                    else if (strcmp(prefix, "path") == 0)
                        passes_text = (pp && strstr(pp, farg)) ? 1 : 0;
                    else if (strcmp(prefix, "group") == 0)
                    {
                        const char* s2 = NULL;
                        if (nid)
                            s2 = strchr(nid, '/');
                        size_t gl = s2 && nid ? (size_t) (s2 - nid) : 0;
                        passes_text =
                            (gl > 0 && strncmp(nid, farg, gl) == 0 && farg[gl] == '\0') ? 1 : 0;
                    }
                    else if (strcmp(prefix, "hash") == 0)
                    {
                        unsigned long long hv = 0ULL;
                        (void) rogue_asset_dep_hash(nid, &hv);
                        char hx[20];
                        snprintf(hx, sizeof hx, "%016llx", (unsigned long long) hv);
                        passes_text = (strstr(hx, farg) != 0) ? 1 : 0;
                    }
                    else if (strcmp(prefix, "rev") == 0)
                    {
                        /* nodes that depend on farg */
                        passes_text = content_graph_has_dep_on(nid ? nid : "", farg);
                    }
                    else if (strcmp(prefix, "dep") == 0)
                    {
                        /* direct dependencies of farg */
                        passes_text = content_graph_is_dep_of(nid ? nid : "", farg);
                    }
                    else
                    {
                        /* unknown prefix -> fallback to substring */
                        passes_text = ((nid && strstr(nid, f)) || (pp && strstr(pp, f))) ? 1 : 0;
                    }
                }
                else
                {
                    passes_text = ((nid && strstr(nid, f)) || (pp && strstr(pp, f))) ? 1 : 0;
                }
            }
            int passes_group = 1;
            if (cur_group && nid)
            {
                const char* s1 = strchr(cur_group, '/');
                const char* s2 = strchr(nid, '/');
                if (s1 && s2)
                {
                    size_t gL = (size_t) (s1 - cur_group);
                    passes_group = (strncmp(cur_group, nid, gL) == 0) ? 1 : 0;
                }
            }
            if (passes_text && passes_group)
            {
                idxs[idx_count++] = i;
            }
        }
    }
    if (idx_count <= 0)
    {
        overlay_label("No nodes match filter.");
        overlay_end_panel();
        return;
    }
    if (sel < 0)
        sel = 0;
    if (sel >= idx_count)
        sel = idx_count - 1;
    if (overlay_checkbox("Show only selected group", &group_only))
    {
        /* no-op; toggled state applied next frame */
    }
    overlay_checkbox("Draw edges (selected)", &draw_edges);
    if (preview_depth < 1)
        preview_depth = 1;
    overlay_slider_int("Preview depth", &preview_depth, 1, 3);
    overlay_slider_int("Node", &sel, 0, idx_count - 1);
    int node_index = idxs[sel];
    const char *id = NULL, *path = NULL;
    if (rogue_asset_dep_get(node_index, &id, &path) == 0)
    {
        char line[200];
        /* Group prefix */
        /* Avoid ternary promotion/casts here to keep MSVC happy */
        const char* slash = NULL;
        if (id)
            slash = strchr(id, '/');
        char group[48];
        if (slash)
        {
            size_t gl = (size_t) (slash - id);
            if (gl >= sizeof group)
                gl = sizeof group - 1;
            memcpy(group, id, gl);
            group[gl] = '\0';
            snprintf(line, sizeof line, "Group: %s", group);
            overlay_label(line);
        }
        snprintf(line, sizeof line, "[%d/%d] id=%s", node_index, n, id ? id : "<nil>");
        overlay_label(line);
        snprintf(line, sizeof line, "path=%s", path && *path ? path : "<none>");
        overlay_label(line);
        /* Show node hash */
        if (id)
        {
            unsigned long long hv = 0ULL;
            if (rogue_asset_dep_hash(id, &hv) == 0)
            {
                snprintf(line, sizeof line, "hash=0x%016llx", (unsigned long long) hv);
                overlay_label(line);
            }
        }
        /* Show last dependency registration rejection, if any */
        {
            char k[32], nid[64], dep[64];
            if (rogue_asset_dep_get_last_reject(k, (int) sizeof k, nid, (int) sizeof nid, dep,
                                                (int) sizeof dep))
            {
                char msg[256];
                snprintf(msg, sizeof msg, "Last register reject: kind=%s id=%s dep=%s", k, nid,
                         dep[0] ? dep : "<n/a>");
                overlay_label(msg);
            }
        }
        overlay_label("Deps:");
        const char* deps[16];
        int dc = rogue_asset_dep_get_deps(id, deps, (int) (sizeof deps / sizeof deps[0]));
        if (dc > 0)
        {
            for (int i = 0; i < dc; i++)
            {
                overlay_label(deps[i]);
            }
        }
        else
        {
            overlay_label("<none>");
        }
        /* Reverse deps: who depends on this node directly? */
        overlay_label("Reverse Deps:");
        int rev_found = 0;
        for (int i = 0; i < n; ++i)
        {
            const char *oid = NULL, *opp = NULL;
            if (rogue_asset_dep_get(i, &oid, &opp) != 0 || !oid)
                continue;
            const char* tmp[16];
            int c = rogue_asset_dep_get_deps(oid, tmp, (int) (sizeof tmp / sizeof tmp[0]));
            for (int j = 0; j < c; ++j)
            {
                if (tmp[j] && id && strcmp(tmp[j], id) == 0)
                {
                    overlay_label(oid);
                    rev_found = 1;
                    break;
                }
            }
        }
        if (!rev_found)
            overlay_label("<none>");
        /* Group summary: count how many nodes share the same prefix */
        if (slash)
        {
            int group_count = 0;
            for (int i = 0; i < n; ++i)
            {
                const char *gid = NULL, *gpp = NULL;
                if (rogue_asset_dep_get(i, &gid, &gpp) != 0 || !gid)
                    continue;
                const char* s2 = strchr(gid, '/');
                if (s2)
                {
                    size_t g2l = (size_t) (s2 - gid);
                    if (g2l == (size_t) (slash - id) && strncmp(gid, id, g2l) == 0)
                        group_count++;
                }
            }
            snprintf(line, sizeof line, "Group size: %d", group_count);
            overlay_label(line);
        }
        /* Path collision marker: list other nodes that reference the same path */
        if (path && *path)
        {
            int dup_count = 0;
            for (int i = 0; i < n; ++i)
            {
                const char *oid = NULL, *opp = NULL;
                if (rogue_asset_dep_get(i, &oid, &opp) != 0 || !oid)
                    continue;
                if (oid != id && opp && strcmp(opp, path) == 0)
                {
                    if (dup_count == 0)
                        overlay_label("Path also used by:");
                    overlay_label(oid);
                    dup_count++;
                }
            }
            if (dup_count == 0)
                overlay_label("Path unique within graph.");
        }

        /* Focused subgraph export: DOT and JSON limited to the current root and preview_depth */
        if (overlay_columns_begin(2, NULL))
        {
            if (overlay_button("Export Subgraph DOT"))
            {
                /* Use literal constants for MSVC C89 compatibility (no VLAs, const int not
                 * constexpr) */
                const char* nids[128];
                int ndeps[128];
                int edges[256][2];
                int ecount = 0;
                int ncount = content_graph_collect_forward(id, preview_depth, nids, ndeps, 128,
                                                           edges, 256, &ecount);
                FILE* f = NULL;
#if defined(_MSC_VER)
                fopen_s(&f, "build/content_subgraph.dot", "wb");
#else
                f = fopen("build/content_subgraph.dot", "wb");
#endif
                if (f)
                {
                    fputs("digraph ContentSubgraph {\n  rankdir=LR;\n  node "
                          "[shape=box,fontname=Helvetica];\n",
                          f);
                    for (int i = 0; i < ncount; ++i)
                    {
                        const char* nid = nids[i];
                        if (!nid)
                            continue;
                        fprintf(f, "  \"%s\";\n", nid);
                    }
                    for (int i = 0; i < ecount; ++i)
                    {
                        int s = edges[i][0], t = edges[i][1];
                        if (s >= 0 && s < ncount && t >= 0 && t < ncount)
                            fprintf(f, "  \"%s\" -> \"%s\";\n", nids[s], nids[t]);
                    }
                    fputs("}\n", f);
                    fclose(f);
                    overlay_label("Subgraph DOT exported (build/content_subgraph.dot).");
                }
                else
                {
                    overlay_label("Failed to open subgraph DOT output.");
                }
            }
            overlay_next_column();
            if (overlay_button("Export Subgraph JSON"))
            {
                /* Use literal constants for MSVC C89 compatibility (no VLAs, const int not
                 * constexpr) */
                const char* nids[128];
                int ndeps[128];
                int edges[256][2];
                int ecount = 0;
                int ncount = content_graph_collect_forward(id, preview_depth, nids, ndeps, 128,
                                                           edges, 256, &ecount);
                FILE* f = NULL;
#if defined(_MSC_VER)
                fopen_s(&f, "build/content_subgraph.json", "wb");
#else
                f = fopen("build/content_subgraph.json", "wb");
#endif
                if (f)
                {
                    /* Shape: {"root":"id","depth":N,"nodes":[...],"edges":[{"from":"","to":""},...]
                     * } */
                    fprintf(f, "{\n  \"root\":\"%s\",\n  \"depth\":%d,\n  \"nodes\": [\n",
                            id ? id : "", preview_depth);
                    for (int i = 0; i < ncount; ++i)
                    {
                        unsigned long long hv = 0ULL;
                        (void) rogue_asset_dep_hash(nids[i] ? nids[i] : "", &hv);
                        fprintf(f, "    {\"id\":\"%s\",\"hash\":\"0x%016llx\"}%s\n",
                                nids[i] ? nids[i] : "", hv, (i + 1 < ncount) ? "," : "");
                    }
                    fputs("  ],\n  \"edges\": [\n", f);
                    for (int i = 0; i < ecount; ++i)
                    {
                        int s = edges[i][0], t = edges[i][1];
                        if (s >= 0 && s < ncount && t >= 0 && t < ncount)
                        {
                            fprintf(f, "    {\"from\":\"%s\",\"to\":\"%s\"}%s\n", nids[s], nids[t],
                                    (i + 1 < ecount) ? "," : "");
                        }
                    }
                    fputs("  ]\n}\n", f);
                    fclose(f);
                    overlay_label("Subgraph JSON exported (build/content_subgraph.json).");
                }
                else
                {
                    overlay_label("Failed to open subgraph JSON output.");
                }
            }
            overlay_columns_end();
        }
        /* Textual edges view for current group: id -> dep1, dep2 ... */
        if (slash)
        {
            static int edges_open = 1;
            if (overlay_tree_node("Edges (group)", &edges_open))
            {
                size_t gl = (size_t) (slash - id);
                int edge_lines = 0;
                for (int i = 0; i < n; ++i)
                {
                    const char *gid = NULL, *gpp = NULL;
                    if (rogue_asset_dep_get(i, &gid, &gpp) != 0 || !gid)
                        continue;
                    const char* s2 = strchr(gid, '/');
                    if (!s2)
                        continue;
                    size_t g2l = (size_t) (s2 - gid);
                    if (g2l == gl && strncmp(gid, id, gl) == 0)
                    {
                        const char* deps2[16];
                        int dc2 = rogue_asset_dep_get_deps(gid, deps2,
                                                           (int) (sizeof deps2 / sizeof deps2[0]));
                        char row[256];
                        int off = snprintf(row, sizeof row, "%s -> ", gid);
                        if (dc2 <= 0)
                        {
                            snprintf(row + off, (size_t) (sizeof row - off), "<none>");
                        }
                        else
                        {
                            for (int j = 0; j < dc2; ++j)
                            {
                                const char* dj = deps2[j] ? deps2[j] : "?";
                                int left = (int) sizeof(row) - off;
                                if (left > 4)
                                {
                                    off += snprintf(row + off, (size_t) left, "%s%s", dj,
                                                    (j + 1 < dc2) ? ", " : "");
                                }
                            }
                        }
                        overlay_label(row);
                        edge_lines++;
                    }
                }
                if (edge_lines == 0)
                    overlay_label("<no group edges>");
                overlay_tree_pop();
            }
        }

        /* SDL layered preview for selected node up to N hops (simple layout) */
#ifdef ROGUE_HAVE_SDL
        if (draw_edges && g_app.renderer)
        {
            /* Panel geometry is fixed-height in overlay; draw in lower region of this panel */
            const int panel_x = 1920 - 380;
            const int panel_y = 10;
            const int panel_w = 360;
            const int cx = panel_x + 12;
            const int cy = panel_y + 120;
            const int cw = panel_w - 24;
            const int ch = 180;
            SDL_Rect area = {cx, cy, cw, ch};
            SDL_SetRenderDrawColor(g_app.renderer, 12, 12, 12, 180);
            SDL_RenderFillRect(g_app.renderer, &area);
            SDL_SetRenderDrawColor(g_app.renderer, 100, 100, 140, 220);
            SDL_RenderDrawRect(g_app.renderer, &area);
            /* Collect nodes for preview */
            const int MAX_NODES = 48;
            const int MAX_EDGES = 96;
            const char* nids[MAX_NODES];
            int ndeps[MAX_NODES];
            int edges[MAX_EDGES][2];
            int ecount = 0;
            int ncount = content_graph_collect_forward(id, preview_depth, nids, ndeps, MAX_NODES,
                                                       edges, MAX_EDGES, &ecount);

            /* Build a simple parent map from edges (first parent wins -> BFS tree) */
            int parent[MAX_NODES];
            for (int i = 0; i < MAX_NODES; ++i)
                parent[i] = -1;
            for (int i = 0; i < ecount; ++i)
            {
                int s = edges[i][0], t = edges[i][1];
                if (s >= 0 && s < ncount && t >= 0 && t < ncount)
                {
                    if (parent[t] < 0 && t != 0) /* don't assign parent for root */
                        parent[t] = s;
                }
            }

            /* Compute per-depth counts */
            int depth_counts[8] = {0};
            int depth_first_idx[8] = {0};
            int max_d = 0;
            for (int i = 0; i < ncount; ++i)
            {
                int d = (ndeps[i] < 0) ? 0 : ndeps[i];
                if (d > 7)
                    d = 7;
                depth_counts[d]++;
                if (d > max_d)
                    max_d = d;
            }
            /* X placement per depth column */
            int col_w = (max_d + 1) > 0 ? cw / (max_d + 1) : cw;
            SDL_Rect rects[MAX_NODES];
            /* Track how many placed per depth to space vertically */
            int placed_at_depth[8] = {0};
            for (int i = 0; i < ncount; ++i)
            {
                int d = ndeps[i];
                if (d < 0)
                    d = 0;
                if (d > max_d)
                    d = max_d;
                int per = depth_counts[d] > 0 ? depth_counts[d] : 1;
                int idx = placed_at_depth[d]++;
                int x = cx + d * col_w + 6;
                int y = cy + 8 + (per == 1 ? (ch / 2 - 10) : (idx * (ch - 24) / (per - 1)));
                rects[i].x = x;
                rects[i].y = y;
                rects[i].w = (col_w > 140 ? 120 : (col_w - 20 > 60 ? col_w - 20 : 60));
                rects[i].h = 20;
            }
            /* Draw edges first */
            SDL_SetRenderDrawColor(g_app.renderer, 180, 180, 220, 220);
            for (int i = 0; i < ecount; ++i)
            {
                int s = edges[i][0], t = edges[i][1];
                if (s >= 0 && s < ncount && t >= 0 && t < ncount)
                {
                    int x0 = rects[s].x + rects[s].w;
                    int y0 = rects[s].y + rects[s].h / 2;
                    int x1 = rects[t].x;
                    int y1 = rects[t].y + rects[t].h / 2;
                    SDL_RenderDrawLine(g_app.renderer, x0, y0, x1, y1);
                }
            }
            /* If the last registration was rejected due to a cycle, visualize the attempted edge
               in red if both endpoints are present in this subgraph. */
            {
                char kind[32] = {0}, nid2[64] = {0}, dep2[64] = {0};
                if (rogue_asset_dep_get_last_reject(kind, (int) sizeof kind, nid2,
                                                    (int) sizeof nid2, dep2, (int) sizeof dep2))
                {
                    if (strcmp(kind, "cycle") == 0 && nid2[0] && dep2[0])
                    {
                        int a = -1, b = -1;
                        for (int i = 0; i < ncount; ++i)
                        {
                            if (nids[i] && strcmp(nids[i], nid2) == 0)
                                a = i;
                            if (nids[i] && strcmp(nids[i], dep2) == 0)
                                b = i;
                        }
                        if (a >= 0 && b >= 0)
                        {
                            SDL_SetRenderDrawColor(g_app.renderer, 220, 60, 60, 240);
                            int x0 = rects[a].x + rects[a].w;
                            int y0 = rects[a].y + rects[a].h / 2;
                            int x1 = rects[b].x;
                            int y1 = rects[b].y + rects[b].h / 2;
                            SDL_RenderDrawLine(g_app.renderer, x0, y0, x1, y1);
                        }
                    }
                }
            }
            /* Draw nodes */
            for (int i = 0; i < ncount; ++i)
            {
                SDL_Rect r = rects[i];
                /* root highlighted; group-based color tint for readability */
                if (i == 0)
                {
                    SDL_SetRenderDrawColor(g_app.renderer, 40, 70, 110, 220);
                }
                else
                {
                    /* derive a stable tint from group prefix */
                    unsigned ghash = 2166136261u;
                    const char* gid = nids[i];
                    const char* slash2 = gid ? strchr(gid, '/') : NULL;
                    int gl = 0;
                    if (gid && slash2)
                        gl = (int) (slash2 - gid);
                    for (int c = 0; c < gl; ++c)
                    {
                        ghash ^= (unsigned) gid[c];
                        ghash *= 16777619u;
                    }
                    unsigned r8 = 60u + (ghash & 95u);
                    unsigned g8 = 60u + ((ghash >> 8) & 95u);
                    unsigned b8 = 60u + ((ghash >> 16) & 95u);
                    SDL_SetRenderDrawColor(g_app.renderer, (Uint8) r8, (Uint8) g8, (Uint8) b8, 220);
                }
                SDL_RenderFillRect(g_app.renderer, &r);
                SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
                SDL_RenderDrawRect(g_app.renderer, &r);
                const char* label = nids[i] ? nids[i] : "?";
                rogue_font_draw_text(r.x + 4, r.y + 4, label, 1,
                                     (RogueColor){i == 0 ? 255 : 220, 255, 220, 255});
            }
            /* Click-to-drill: if user clicks a node rect, switch selection to that node
               (updates the root next frame) and build breadcrumbs from root to clicked. */
            const OverlayInputState* in = overlay_input_get();
            if (in && in->mouse_pressed)
            {
                int mx = (int) in->mouse_x, my = (int) in->mouse_y;
                if (mx >= area.x && mx <= area.x + area.w && my >= area.y && my <= area.y + area.h)
                {
                    for (int i = 0; i < ncount; ++i)
                    {
                        SDL_Rect rr = rects[i];
                        if (mx >= rr.x && mx <= rr.x + rr.w && my >= rr.y && my <= rr.y + rr.h)
                        {
                            const char* clicked = nids[i];
                            if (clicked)
                            {
                                /* Build breadcrumbs using parent map */
                                const char* tmp[32];
                                int tlen = 0;
                                int cur = i;
                                while (cur >= 0 && tlen < (int) (sizeof tmp / sizeof tmp[0]))
                                {
                                    tmp[tlen++] = nids[cur];
                                    if (cur == 0)
                                        break;
                                    cur = parent[cur];
                                }
                                /* reverse into persistent crumbs */
                                crumb_len = 0;
                                for (int k = tlen - 1;
                                     k >= 0 && crumb_len < (int) (sizeof crumbs / sizeof crumbs[0]);
                                     --k)
                                    crumbs[crumb_len++] = tmp[k];

                                /* Try to move selection to clicked within current filtered list */
                                int nall = rogue_asset_dep_count();
                                int glob = -1;
                                for (int gi = 0; gi < nall; ++gi)
                                {
                                    const char *gid2 = NULL, *gpp2 = NULL;
                                    if (rogue_asset_dep_get(gi, &gid2, &gpp2) == 0 && gid2 &&
                                        strcmp(gid2, clicked) == 0)
                                    {
                                        glob = gi;
                                        break;
                                    }
                                }
                                if (glob >= 0)
                                {
                                    /* Does this global index exist in idxs? If not, force filter to
                                     * id:clicked */
                                    int found = -1;
                                    for (int p = 0; p < idx_count; ++p)
                                    {
                                        if (idxs[p] == glob)
                                        {
                                            found = p;
                                            break;
                                        }
                                    }
                                    if (found >= 0)
                                    {
                                        sel = found;
                                    }
                                    else
                                    {
                                        /* Override filter to bring the node into view */
                                        snprintf(filter, sizeof filter, "id:%s", clicked);
                                        group_only = 0;
                                        sel = 0;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
            /* DAG note */
            rogue_font_draw_text(cx + 6, cy + ch - 14, "Graph is DAG (cycles rejected)", 1,
                                 (RogueColor){160, 200, 255, 255});
        }
#endif
    }
    /* Navigation breadcrumbs UI */
    if (crumb_len > 0)
    {
        char pathline[320];
        int off = snprintf(pathline, sizeof pathline, "Path: ");
        for (int i = 0; i < crumb_len; ++i)
        {
            const char* s = crumbs[i] ? crumbs[i] : "?";
            int left = (int) sizeof(pathline) - off;
            if (left <= 4)
                break;
            off += snprintf(pathline + off, (size_t) left, "%s%s", s,
                            (i + 1 < crumb_len) ? " -> " : "");
        }
        overlay_label(pathline);
        if (crumb_len > 1)
        {
            if (overlay_button("Back"))
            {
                /* Select the previous node in the breadcrumb chain */
                const char* target = crumbs[crumb_len - 2];
                if (target)
                {
                    int nall = rogue_asset_dep_count();
                    int glob = -1;
                    for (int gi = 0; gi < nall; ++gi)
                    {
                        const char *gid2 = NULL, *gpp2 = NULL;
                        if (rogue_asset_dep_get(gi, &gid2, &gpp2) == 0 && gid2 &&
                            strcmp(gid2, target) == 0)
                        {
                            glob = gi;
                            break;
                        }
                    }
                    if (glob >= 0)
                    {
                        /* find in current filtered list */
                        int found = -1;
                        /* reuse last built idxs (still valid in this frame) */
                        for (int p = 0; p < idx_count; ++p)
                            if (idxs[p] == glob)
                            {
                                found = p;
                                break;
                            }
                        if (found >= 0)
                            sel = found;
                        else
                        {
                            snprintf(filter, sizeof filter, "id:%s", target);
                            group_only = 0;
                            sel = 0;
                        }
                    }
                }
                if (crumb_len > 0)
                    crumb_len--; /* pop current */
            }
        }
    }
    overlay_end_panel();
}

void rogue_overlay_register_default_panels(void)
{
    overlay_register_panel("system", "System", panel_system, NULL);
    overlay_register_panel("player", "Player", panel_player, NULL);
    overlay_register_panel("skills", "Skills", panel_skills, NULL);
    overlay_register_panel("entities", "Entities", panel_entities, NULL);
    overlay_register_panel("map", "Map Editor", panel_map_editor, NULL);
    overlay_register_panel("audiovfx", "Audio / VFX", panel_audiovfx, NULL);
    overlay_register_panel("items", "Items", panel_items, NULL);
    overlay_register_panel("validation", "Validation", panel_validation, NULL);
    overlay_register_panel("content_graph", "Content Graph", panel_content_graph, NULL);
}

#else
void rogue_overlay_register_default_panels(void) {}
#endif
