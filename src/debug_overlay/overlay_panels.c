#include "../core/app/app_state.h"
#include "../core/entities/entity_debug.h"
#include "../core/player/player_debug.h"
#include "../core/skills/skill_debug.h"
#include "../core/skills/skills_coeffs.h"
#include "../core/world/map_debug.h"
#include "overlay_core.h"
#include "overlay_input.h"
#include "overlay_widgets.h"

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
    /* Minimal scaffold: brush radius and tile value input */
    static int brush_radius = 1;
    static int tile_val = 1; /* default GRASS */
    if (brush_radius < 0)
        brush_radius = 0;
    overlay_slider_int("Brush Radius", &brush_radius, 0, 16);
    overlay_slider_int("Tile Value", &tile_val, 0, 255);
    if (overlay_button("Paint 9x9 at Center"))
    {
        int cx = g_app.world_map.width / 2;
        int cy = g_app.world_map.height / 2;
        (void) rogue_map_debug_brush_square(cx, cy, brush_radius, (unsigned char) tile_val);
    }
    if (overlay_button("Save JSON -> build/map.json"))
    {
        int rc = rogue_map_debug_save_json("build/map.json");
        char msg[64];
        snprintf(msg, sizeof msg, "save rc=%d", rc);
        overlay_label(msg);
    }
    if (overlay_button("Load JSON <- build/map.json"))
    {
        int rc = rogue_map_debug_load_json("build/map.json");
        char msg[64];
        snprintf(msg, sizeof msg, "load rc=%d", rc);
        overlay_label(msg);
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
}

#else
void rogue_overlay_register_default_panels(void) {}
#endif
