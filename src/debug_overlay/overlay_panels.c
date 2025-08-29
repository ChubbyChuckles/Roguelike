#include "../core/app/app_state.h"
#include "../core/player/player_debug.h"
#include "../core/skills/skill_debug.h"
#include "../core/skills/skills_coeffs.h"
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

    /* Quick simulate button with default profile over 2s using selected id only */
    if (overlay_button("Simulate 2s (selected only)"))
    {
        char out[256];
        char profile[128];
        snprintf(profile, sizeof profile, "{\"duration_ms\":2000,\"priority\":[%d]}", sel);
        if (rogue_skill_debug_simulate(profile, out, (int) sizeof out) == 0)
        {
            overlay_label(out);
        }
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

void rogue_overlay_register_default_panels(void)
{
    overlay_register_panel("system", "System", panel_system, NULL);
    overlay_register_panel("player", "Player", panel_player, NULL);
    overlay_register_panel("skills", "Skills", panel_skills, NULL);
}

#else
void rogue_overlay_register_default_panels(void) {}
#endif
