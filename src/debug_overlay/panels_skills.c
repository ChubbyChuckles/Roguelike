#include "../core/app/app_state.h"
#include "../core/skills/skill_debug.h"
#include "../core/skills/skills_coeffs.h"
#include "../core/skills/skills_validate.h"
#include "../graphics/effect_spec.h"
#include "../graphics/sprite.h"
#include "overlay_core.h"
#include "overlay_input.h"
#include "overlay_widgets.h"
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* Lightweight preview texture cache helpers (file-scope) */
typedef struct PreviewTex
{
    char path[256];
    RogueTexture tex;
    int ready;
} PreviewTex;

/* Small utility for box hit-testing (C-friendly) */
static int overlay_box_hit(int mx, int my, int bx, int by, int bw, int bh)
{
    return (mx >= bx && mx < bx + bw && my >= by && my < by + bh) ? 1 : 0;
}

static void preview_ensure_tex(PreviewTex* t, const char* path)
{
    if (!t)
        return;
    if (!path)
        path = "";
    if (strcmp(t->path, path) != 0)
    {
        /* destroy old */
        if (t->ready)
        {
            rogue_texture_destroy(&t->tex);
            t->ready = 0;
        }
        t->path[0] = '\0';
        if (path[0])
        {
            /* try load */
            if (rogue_texture_load(&t->tex, path))
            {
                strncpy(t->path, path, sizeof t->path - 1);
                t->path[sizeof t->path - 1] = '\0';
                t->ready = 1;
            }
        }
    }
}

static void panel_skills(void* user)
{
    (void) user;
    // Use movable, persisted panel begin with a wider default to fit content.
    if (!overlay_begin_panel_auto("skills", "Skills", 360, 10, 480))
        return;
    /* Simple tabs (combo-based): Overview, Effects, Visuals, Audio, Testing */
    static int tab = 0;
    static const char* tab_names[] = {"Overview", "Effects", "Visuals", "Audio", "Testing"};
    overlay_combo("Tab", &tab, tab_names, 5);

    /* Persistent UI state */
    static float sim_duration_ms = 2000.0f;
    static float sim_tick_ms = 16.0f;
    static float sim_ap_regen_per_sec = 0.0f;
    static char prio_buf[128] = "";
    static char sim_result[256] = "";
    static int last_valid_ok = 1;
    static char last_valid_msg[128] = "OK";
    /* Sticky Validation Status banner (visible at top of panel) */
    {
        char vline[192];
        snprintf(vline, sizeof vline, "Validation: %s%s%s", last_valid_ok ? "OK" : "ERROR",
                 last_valid_ok ? "" : ": ", last_valid_ok ? "" : last_valid_msg);
        overlay_label(vline);
    }
    /* Template/new-skill helpers (Overview tab) */
    static int tmpl_id = -1;
    static char tmpl_filter[64] = "";
    static char new_name[64] = "";
    static int new_max_rank = 5;
    static float new_base_cd = 1000.f;
    static float new_cd_red = 0.f;
    static float new_cast_ms = 0.f;
    static int new_is_passive = 0;
    static int copy_coeffs = 1;

    const char* overrides_path = "build/skills_overrides.json";
    const char* base_skills_path = "assets/skills_uhf87f.json";
    static int auto_reload = 1;
    static int auto_reload_base = 0;

    int count = rogue_skill_debug_count();
    static int sel = 0;
    if (sel < 0)
        sel = 0;
    if (sel >= count)
        sel = count - 1;
    if (count <= 0)
    {
        overlay_label("No skills registered");
        overlay_end_panel();
        return;
    }
    overlay_slider_int("Skill Index", &sel, 0, count - 1);
    const char* name = rogue_skill_debug_name(sel);

    /* Overview -------------------------------------------------------------- */
    if (tab == 0)
    {
        overlay_label("Templates");
        overlay_input_text("Filter", tmpl_filter, sizeof tmpl_filter);
        const char* headers[] = {"ID", "Name"};
        int sort_col = 0, sort_dir = 0;
        int selected = (tmpl_id >= 0 && tmpl_id < count) ? tmpl_id : -1;
        if (overlay_table_begin("skills_tmpl", headers, 2, &sort_col, &sort_dir, NULL))
        {
            /* Clamp how many rows we draw per frame to fit the panel height; approx rows */
            int max_rows = (g_app.viewport_h - 220) / 20;
            if (max_rows < 8)
                max_rows = 8;
            int drawn = 0;
            for (int i = 0; i < count; ++i)
            {
                if (drawn >= max_rows)
                    break;
                const char* sname = rogue_skill_debug_name(i);
                if (!sname)
                    continue;
                if (tmpl_filter[0] != '\0')
                {
                    const char* p = sname;
                    const char* f = tmpl_filter;
                    const char* hit = NULL;
                    for (; *p && !hit; ++p)
                    {
                        const char* p2 = p;
                        const char* f2 = f;
                        while (*p2 && *f2 && *p2 == *f2)
                        {
                            ++p2;
                            ++f2;
                        }
                        if (*f2 == '\0')
                            hit = p;
                    }
                    if (!hit)
                        continue;
                }
                char id_s[16];
                snprintf(id_s, sizeof id_s, "%d", i);
                const char* cells[] = {id_s, sname};
                (void) overlay_table_row(cells, 2, i, &selected);
                ++drawn;
            }
            overlay_table_end();
        }
        tmpl_id = selected;
        overlay_checkbox("Copy coeffs on create", &copy_coeffs);
        if (overlay_button("Apply Template") && tmpl_id >= 0 && tmpl_id < count)
        {
            const char* tname = rogue_skill_debug_name(tmpl_id);
            if (tname)
                snprintf(new_name, sizeof new_name, "%s_Copy", tname);
            int mr = 1, pass = 0;
            if (rogue_skill_debug_get_meta(tmpl_id, &mr, &pass) == 0)
            {
                new_max_rank = mr;
                new_is_passive = pass;
            }
            float bcd = 0.f, red = 0.f, cst = 0.f;
            if (rogue_skill_debug_get_timing(tmpl_id, &bcd, &red, &cst) == 0)
            {
                new_base_cd = bcd;
                new_cd_red = red;
                new_cast_ms = cst;
            }
            if (copy_coeffs)
            {
                RogueSkillCoeffParams tcp;
                if (rogue_skill_debug_get_coeff(tmpl_id, &tcp) == 0)
                    rogue_skill_debug_set_coeff(sel, &tcp);
            }
        }
        overlay_input_text("Name", new_name, sizeof new_name);
        overlay_slider_int("Max Rank", &new_max_rank, 1, 20);
        overlay_slider_float("Base Cooldown (ms)", &new_base_cd, 0.f, 60000.f);
        overlay_slider_float("CD Reduction/rank (ms)", &new_cd_red, -1000.f, 1000.f);
        overlay_slider_float("Cast Time (ms)", &new_cast_ms, 0.f, 5000.f);
        overlay_checkbox("Passive", &new_is_passive);
        if (overlay_button("Create"))
        {
            char vmsg[192] = {0};
            if (rogue_skill_debug_validate(vmsg, (int) sizeof vmsg) != 0)
            {
                char warn[256];
                snprintf(warn, sizeof warn, "Validation failed: %s",
                         vmsg[0] ? vmsg : "(no details)");
                overlay_label(warn);
            }
            /* Update cross-rule sticky status */
            {
                char emsg[192] = {0};
                int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
                last_valid_ok = ok;
                if (!ok)
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
                else
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
            }
            int idx = rogue_skill_debug_create(new_name, new_max_rank, new_base_cd, new_cd_red,
                                               new_cast_ms, new_is_passive);
            char msg[128];
            if (idx >= 0)
            {
                snprintf(msg, sizeof msg, "Created skill '%s' at id %d", new_name, idx);
                sel = idx;
                new_name[0] = '\0';
                if (copy_coeffs && tmpl_id >= 0 && tmpl_id < count)
                {
                    RogueSkillCoeffParams tcp;
                    if (rogue_skill_debug_get_coeff(tmpl_id, &tcp) == 0)
                        rogue_skill_debug_set_coeff(idx, &tcp);
                }
            }
            else
            {
                snprintf(msg, sizeof msg, "Create failed (%d)", idx);
            }
            overlay_label(msg);
        }

        overlay_label("Meta");
        int stype = 0;
        if (rogue_skill_debug_get_type(sel, &stype) == 0)
        {
            int changed = 0;
            changed |= overlay_slider_int("Skill Type (0..9)", &stype, 0, 9);
            static const char* type_names[10] = {"UNKNOWN", "MELEE",   "RANGED", "AOE_SPELL",
                                                 "BUFF",    "DEBUFF",  "HEAL",   "SUMMON",
                                                 "PASSIVE", "ULTIMATE"};
            if (stype >= 0 && stype <= 9)
            {
                char tn[64];
                snprintf(tn, sizeof tn, "Type: %s", type_names[stype]);
                overlay_label(tn);
            }
            if (changed)
            {
                rogue_skill_debug_set_type(sel, stype);
                (void) rogue_skill_debug_save_overrides(overrides_path);
                char emsg[192] = {0};
                int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
                last_valid_ok = ok;
                if (!ok)
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
                else
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
            }

            overlay_label("Type Presets");
            if (overlay_button("Apply Preset for Type"))
            {
                float p_base_cd = 1000.f, p_cd_red = 0.f, p_cast = 0.f;
                RogueSkillCoeffParams pcp;
                memset(&pcp, 0, sizeof pcp);
                pcp.base_scalar = 1.0f;
                pcp.per_rank_scalar = 0.1f;
                switch (stype)
                {
                case 1:
                    p_base_cd = 800.f;
                    p_cast = 200.f;
                    pcp.base_scalar = 1.00f;
                    break;
                case 2:
                    p_base_cd = 900.f;
                    p_cast = 250.f;
                    pcp.base_scalar = 0.95f;
                    break;
                case 3:
                    p_base_cd = 1200.f;
                    p_cast = 400.f;
                    pcp.base_scalar = 0.85f;
                    break;
                case 4:
                    p_base_cd = 15000.f;
                    p_cast = 300.f;
                    pcp.base_scalar = 0.0f;
                    break;
                case 5:
                    p_base_cd = 12000.f;
                    p_cast = 300.f;
                    pcp.base_scalar = 0.2f;
                    break;
                case 6:
                    p_base_cd = 5000.f;
                    p_cast = 350.f;
                    pcp.base_scalar = 0.0f;
                    break;
                case 7:
                    p_base_cd = 20000.f;
                    p_cast = 500.f;
                    pcp.base_scalar = 0.0f;
                    break;
                case 8:
                    p_base_cd = 0.f;
                    p_cast = 0.f;
                    pcp.base_scalar = 0.0f;
                    break;
                case 9:
                    p_base_cd = 60000.f;
                    p_cast = 800.f;
                    pcp.base_scalar = 1.5f;
                    break;
                default:
                    p_base_cd = 1000.f;
                    p_cast = 0.f;
                    pcp.base_scalar = 0.5f;
                    break;
                }
                rogue_skill_debug_set_timing(sel, p_base_cd, p_cd_red, p_cast);
                rogue_skill_debug_set_coeff(sel, &pcp);
                (void) rogue_skill_debug_save_overrides(overrides_path);
                char emsg2[192] = {0};
                int ok2 = (rogue_skills_validate_all(emsg2, (int) sizeof emsg2) == 0);
                last_valid_ok = ok2;
                if (!ok2)
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s",
                             emsg2[0] ? emsg2 : "error");
                else
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
            }
        }

        float base_cd = 0.f, cd_red = 0.f, cast_ms = 0.f;
        if (rogue_skill_debug_get_timing(sel, &base_cd, &cd_red, &cast_ms) == 0)
        {
            if (overlay_slider_float("Base Cooldown (ms)", &base_cd, 0.f, 60000.f) ||
                overlay_slider_float("CD Reduction/rank (ms)", &cd_red, -1000.f, 1000.f) ||
                overlay_slider_float("Cast Time (ms)", &cast_ms, 0.f, 5000.f))
            {
                rogue_skill_debug_set_timing(sel, base_cd, cd_red, cast_ms);
                (void) rogue_skill_debug_save_overrides(overrides_path);
                char emsg[192] = {0};
                int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
                last_valid_ok = ok;
                if (!ok)
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
                else
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
            }
        }

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
                char emsg[192] = {0};
                int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
                last_valid_ok = ok;
                if (!ok)
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
                else
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
            }
        }
    }

    float base_cd = 0.f, cd_red = 0.f, cast_ms = 0.f;
    if (rogue_skill_debug_get_timing(sel, &base_cd, &cd_red, &cast_ms) == 0)
    {
        if (overlay_slider_float("Base Cooldown (ms)", &base_cd, 0.f, 60000.f) ||
            overlay_slider_float("CD Reduction/rank (ms)", &cd_red, -1000.f, 1000.f) ||
            overlay_slider_float("Cast Time (ms)", &cast_ms, 0.f, 5000.f))
        {
            rogue_skill_debug_set_timing(sel, base_cd, cd_red, cast_ms);
            (void) rogue_skill_debug_save_overrides(overrides_path);
            /* Refresh validation status after edit */
            char emsg[192] = {0};
            int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
            last_valid_ok = ok;
            if (!ok)
                snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
            else
                snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
        }
    }

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
            /* Refresh validation status after edit */
            char emsg[192] = {0};
            int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
            last_valid_ok = ok;
            if (!ok)
                snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
            else
                snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
        }
    }

    /* Visuals tab -------------------------------------------------------------- */
    if (tab == 2)
    {
        overlay_label("Visuals");
        static RogueSkillVisualParams vis;
        if (rogue_skill_debug_get_visuals(sel, &vis) == 0)
        {
            int stype = 0;
            (void) rogue_skill_debug_get_type(sel, &stype);
            int vchanged = 0;
            /* Core animation/sprite-sheet (non-passive) */
            if (stype != 8 /* PASSIVE */)
            {
                vchanged |= overlay_input_text("Cast Sprite Sheet", vis.cast_sprite_sheet,
                                               (int) sizeof vis.cast_sprite_sheet);
                vchanged |= overlay_slider_int("Frame Count", &vis.frame_count, 0, 512);
                vchanged |= overlay_slider_float("Frame Duration (ms)", &vis.frame_duration_ms,
                                                 0.0f, 2000.0f);
                vchanged |= overlay_checkbox("Animation Loops", &vis.animation_loops);
                vchanged |= overlay_slider_int("Grid Width", &vis.grid_width, 0, 128);
                vchanged |= overlay_slider_int("Grid Height", &vis.grid_height, 0, 128);
            }
            /* Impact sprite is common */
            vchanged |= overlay_input_text("Impact Sprite", vis.impact_sprite,
                                           (int) sizeof vis.impact_sprite);
            /* AoE fields for AoE spells */
            if (stype == 3 /* AOE_SPELL */)
            {
                vchanged |=
                    overlay_input_text("AoE Sprite", vis.aoe_sprite, (int) sizeof vis.aoe_sprite);
                vchanged |= overlay_slider_int("AoE Shape (0 none,1 circle,2 cone,3 line)",
                                               &vis.aoe_shape, 0, 3);
                vchanged |= overlay_slider_float("AoE Radius", &vis.aoe_radius, 0.0f, 1000.0f);
                vchanged |= overlay_slider_float("AoE Angle", &vis.aoe_angle, 0.0f, 360.0f);
            }
            /* Projectile fields for ranged */
            if (stype == 2 /* RANGED */)
            {
                vchanged |= overlay_input_text("Projectile Sprite", vis.projectile_sprite,
                                               (int) sizeof vis.projectile_sprite);
                vchanged |= overlay_slider_float("Projectile Velocity", &vis.projectile_velocity,
                                                 0.0f, 5000.0f);
                vchanged |= overlay_slider_int("Trajectory Type (0 lin,1 arc,2 homing,3 scatter)",
                                               &vis.trajectory_type, 0, 3);
                vchanged |= overlay_slider_int("Pierce Count", &vis.pierce_count, 0, 50);
                vchanged |=
                    overlay_slider_float("Homing Strength", &vis.homing_strength, 0.0f, 100.0f);
            }
            if (vchanged)
            {
                (void) rogue_skill_debug_set_visuals(sel, &vis);
                (void) rogue_skill_debug_save_overrides(overrides_path);
                /* Refresh validation status after edit */
                char emsg[192] = {0};
                int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
                last_valid_ok = ok;
                if (!ok)
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
                else
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
            }
        }
    }

    /* Audio tab --------------------------------------------------------------- */
    if (tab == 3)
    {
        overlay_label("Audio");
        static RogueSkillVisualParams vis;
        if (rogue_skill_debug_get_visuals(sel, &vis) == 0)
        {
            int vchanged = 0;
            vchanged |= overlay_input_text("Cast Sound Id", vis.cast_sound_id,
                                           (int) sizeof vis.cast_sound_id);
            vchanged |= overlay_input_text("Impact Sound Id", vis.impact_sound_id,
                                           (int) sizeof vis.impact_sound_id);
            vchanged |= overlay_input_text("Loop Sound Id", vis.loop_sound_id,
                                           (int) sizeof vis.loop_sound_id);
            vchanged |= overlay_slider_int("Sound Volume", &vis.sound_volume, 0, 100);
            vchanged |=
                overlay_slider_float("Sound Pitch Var", &vis.sound_pitch_variance, 0.0f, 12.0f);
            if (vchanged)
            {
                (void) rogue_skill_debug_set_visuals(sel, &vis);
                (void) rogue_skill_debug_save_overrides(overrides_path);
                char emsg[192] = {0};
                int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
                last_valid_ok = ok;
                if (!ok)
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
                else
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
            }
        }
    }

    /* Effects tab: effect composition editor (primary + up to 3 nodes) -------- */
    if (tab == 1)
    {
        overlay_label("Effects");
        int changed = 0;
        int primary_id = -1;
        struct RogueSkillEffectNode nodes[3];
        int node_count = 3;
        memset(nodes, 0, sizeof nodes);
        for (int i = 0; i < 3; ++i)
            nodes[i].effect_spec_id = -1;
        if (rogue_skill_debug_get_effects(sel, &primary_id, nodes, &node_count) != 0)
        {
            overlay_label("Failed to fetch effects for skill.");
            node_count = 0;
            primary_id = -1;
        }
        /* Local inline validation for quick feedback (mirrors core rules) */
        {
            int local_errors = 0;
            char line[192];
            /* Primary id validity (treat 0 as unset like core validator) */
            if (primary_id > 0 && rogue_effect_get(primary_id) == NULL)
            {
                snprintf(line, sizeof line, "ERROR: primary effect_spec_id=%d is invalid",
                         primary_id);
                overlay_label(line);
                if (overlay_button("Fix: Clear Primary"))
                {
                    primary_id = -1;
                    changed = 1;
                }
                ++local_errors;
            }
            for (int ni = 0; ni < node_count; ++ni)
            {
                const int eid = nodes[ni].effect_spec_id;
                if (eid > 0 && rogue_effect_get(eid) == NULL)
                {
                    snprintf(line, sizeof line, "ERROR: node %d effect_spec_id=%d invalid", ni + 1,
                             eid);
                    overlay_label(line);
                    if (overlay_button("Fix: Clear Node"))
                    {
                        nodes[ni].effect_spec_id = -1;
                        changed = 1;
                    }
                    ++local_errors;
                }
                if (nodes[ni].duration_ms < 0.0f)
                {
                    snprintf(line, sizeof line, "ERROR: node %d duration_ms < 0", ni + 1);
                    overlay_label(line);
                    if (overlay_button("Fix: Set duration 0"))
                    {
                        nodes[ni].duration_ms = 0.0f;
                        changed = 1;
                    }
                    ++local_errors;
                }
                if (nodes[ni].repeat_count < 0 || nodes[ni].repeat_count > 32)
                {
                    snprintf(line, sizeof line, "ERROR: node %d repeat_count out of range (0..32)",
                             ni + 1);
                    overlay_label(line);
                    if (overlay_button("Fix: Clamp 0..32"))
                    {
                        if (nodes[ni].repeat_count < 0)
                            nodes[ni].repeat_count = 0;
                        if (nodes[ni].repeat_count > 32)
                            nodes[ni].repeat_count = 32;
                        changed = 1;
                    }
                    ++local_errors;
                }
                if (nodes[ni].repeat_count == 0 && nodes[ni].duration_ms > 0.0f &&
                    nodes[ni].repeat_interval_ms <= 0.0f)
                {
                    snprintf(line, sizeof line,
                             "ERROR: node %d duration set but repeat_interval_ms <= 0", ni + 1);
                    overlay_label(line);
                    if (overlay_button("Fix: Set interval 1000ms"))
                    {
                        nodes[ni].repeat_interval_ms = 1000.0f;
                        changed = 1;
                    }
                    ++local_errors;
                }
                if (nodes[ni].require_player_health_below_pct > 100)
                {
                    snprintf(line, sizeof line, "ERROR: node %d HP gate > 100%% (value=%u)", ni + 1,
                             (unsigned) nodes[ni].require_player_health_below_pct);
                    overlay_label(line);
                    if (overlay_button("Fix: Clamp to 100%"))
                    {
                        nodes[ni].require_player_health_below_pct = 100;
                        changed = 1;
                    }
                    ++local_errors;
                }
            }
            if (local_errors == 0)
                overlay_label("Local check: OK");
        }
        /* Minimal EffectSpec palette with filter and selectable target (primary or node index) */
        {
            static int palette_open = 1;
            static char eff_filter[64] = "";
            static int assign_target = -1; /* -1 = primary, >=0 assigns to node index */
            static int eff_selected = -1;  /* last selected effect id */
            static int kind_filter = -1;   /* -1=Any, 0=BUFF,1=DOT,2=AURA */
            static int debuff_only = 0;
            overlay_checkbox("Show EffectSpec Palette", &palette_open);
            if (palette_open)
            {
                overlay_input_text("Filter (id substring)", eff_filter, sizeof eff_filter);
                overlay_slider_int("Assign To: -1=Primary, 0..2 Node", &assign_target, -1, 2);
                overlay_slider_int("Kind Filter (-1 any, 0 buff, 1 dot, 2 aura)", &kind_filter, -1,
                                   2);
                overlay_checkbox("Debuff only", &debuff_only);
                int ec = rogue_effect_count();
                const char* headers[] = {"ID", "Kind", "Debuff", "Dur"};
                int sort_col = 0, sort_dir = 0;
                int tmp_sel = eff_selected;
                if (overlay_table_begin("effect_palette", headers, 4, &sort_col, &sort_dir, NULL))
                {
                    char id_s[16], kind_s[8], deb_s[8], dur_s[16];
                    for (int i = 0; i < ec; ++i)
                    {
                        const RogueEffectSpec* es = rogue_effect_get(i);
                        if (!es)
                            continue;
                        /* simple filter: match id substring text */
                        snprintf(id_s, sizeof id_s, "%d", i);
                        if (eff_filter[0] != '\0')
                        {
                            const char* p = id_s;
                            const char* f = eff_filter;
                            const char* hit = NULL;
                            for (; *p && !hit; ++p)
                            {
                                const char* p2 = p;
                                const char* f2 = f;
                                while (*p2 && *f2 && *p2 == *f2)
                                {
                                    ++p2;
                                    ++f2;
                                }
                                if (*f2 == '\0')
                                    hit = p;
                            }
                            if (!hit)
                                continue;
                        }
                        if (kind_filter >= 0 && (int) es->kind != kind_filter)
                            continue;
                        if (debuff_only && es->debuff == 0)
                            continue;
                        snprintf(kind_s, sizeof kind_s, "%u", (unsigned) es->kind);
                        snprintf(deb_s, sizeof deb_s, "%u", (unsigned) es->debuff);
                        snprintf(dur_s, sizeof dur_s, "%.0f", es->duration_ms);
                        const char* cells[] = {id_s, kind_s, deb_s, dur_s};
                        (void) overlay_table_row(cells, 4, i, &tmp_sel);
                    }
                    overlay_table_end();
                }
                eff_selected = tmp_sel;
                if (overlay_button("Assign Selected"))
                {
                    if (eff_selected >= 0)
                    {
                        if (assign_target < 0)
                        {
                            primary_id = eff_selected;
                            changed = 1;
                        }
                        else if (assign_target < node_count)
                        {
                            nodes[assign_target].effect_spec_id = eff_selected;
                            changed = 1;
                        }
                    }
                }
                if (overlay_button("Clear Nodes"))
                {
                    for (int i = 0; i < node_count; ++i)
                    {
                        nodes[i].effect_spec_id = -1;
                        nodes[i].delay_ms = 0.0f;
                        nodes[i].duration_ms = 0.0f;
                        nodes[i].repeat_count = 0;
                        nodes[i].repeat_interval_ms = 0.0f;
                        nodes[i].require_player_health_below_pct = 0;
                    }
                    changed = 1;
                }
            }
        }
        /* Node Graph Editor (drag-and-drop + simple chaining via delays) */
        {
            static int graph_enabled = 1;
            overlay_checkbox("Enable Node Graph Editor", &graph_enabled);
            if (graph_enabled)
            {
                /* Simple canvas anchored to panel origin (uses fixed coords) */
                const int panel_x = 380;
                const int panel_y = 10;
                const int cv_x = panel_x + 12;
                const int cv_y = panel_y + 410;
                const int cv_w = 396;
                const int cv_h = 160;

#ifdef ROGUE_HAVE_SDL
                if (!g_app.headless && g_app.renderer)
                {
                    SDL_Rect r = {cv_x, cv_y, cv_w, cv_h};
                    SDL_SetRenderDrawColor(g_app.renderer, 14, 14, 20, 220);
                    SDL_RenderFillRect(g_app.renderer, &r);
                    SDL_SetRenderDrawColor(g_app.renderer, 80, 90, 140, 230);
                    SDL_RenderDrawRect(g_app.renderer, &r);
                }
#endif

                typedef struct NodeUI
                {
                    int id;   /* -1 primary, >=0 node index */
                    int x, y; /* top-left */
                } NodeUI;
                static int last_skill = -1;
                static NodeUI ui_primary = {-1, 0, 0};
                static NodeUI ui_nodes[3];
                static int ui_inited = 0;
                static int dragging = 0; /* index: -1 primary, 0..2 nodes, 99 none */
                static int drag_dx = 0, drag_dy = 0;
                if (last_skill != sel)
                {
                    ui_inited = 0;
                    last_skill = sel;
                }
                if (!ui_inited)
                {
                    ui_primary.x = cv_x + 20;
                    ui_primary.y = cv_y + cv_h / 2 - 16;
                    for (int i = 0; i < 3; ++i)
                    {
                        ui_nodes[i].id = i;
                        ui_nodes[i].x = cv_x + 140 + i * 80;
                        ui_nodes[i].y = cv_y + 24 + (i % 2) * 56;
                    }
                    dragging = 99;
                    ui_inited = 1;
                }

                /* Mouse handling via SDL (headless-safe guarded) */
                int mx = 0, my = 0;
                int mdown = 0;
#ifdef ROGUE_HAVE_SDL
                if (!g_app.headless)
                {
                    Uint32 mask = SDL_GetMouseState(&mx, &my);
                    mdown = (mask & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
                }
#endif
                /* Hit test boxes */
                const int bw = 64, bh = 32;
                /* Start drag */
                static int was_down = 0;
                if (mdown && !was_down)
                {
                    if (overlay_box_hit(mx, my, ui_primary.x, ui_primary.y, bw, bh))
                    {
                        dragging = -1;
                        drag_dx = mx - ui_primary.x;
                        drag_dy = my - ui_primary.y;
                    }
                    else
                    {
                        for (int i = 0; i < node_count && i < 3; ++i)
                        {
                            if (overlay_box_hit(mx, my, ui_nodes[i].x, ui_nodes[i].y, bw, bh))
                            {
                                dragging = i;
                                drag_dx = mx - ui_nodes[i].x;
                                drag_dy = my - ui_nodes[i].y;
                                break;
                            }
                        }
                    }
                }
                /* Dragging */
                if (mdown && dragging != 99)
                {
                    if (dragging == -1)
                    {
                        ui_primary.x = mx - drag_dx;
                        ui_primary.y = my - drag_dy;
                    }
                    else if (dragging >= 0 && dragging < 3)
                    {
                        ui_nodes[dragging].x = mx - drag_dx;
                        ui_nodes[dragging].y = my - drag_dy;
                    }
                }
                if (!mdown && was_down)
                {
                    dragging = 99;
                }
                was_down = mdown;

                /* Draw connections from primary to nodes with valid effects */
#ifdef ROGUE_HAVE_SDL
                if (!g_app.headless && g_app.renderer)
                {
                    SDL_SetRenderDrawColor(g_app.renderer, 60, 160, 200, 255);
                    for (int i = 0; i < node_count && i < 3; ++i)
                    {
                        if (nodes[i].effect_spec_id > 0)
                        {
                            int x1 = ui_primary.x + bw;
                            int y1 = ui_primary.y + bh / 2;
                            int x2 = ui_nodes[i].x;
                            int y2 = ui_nodes[i].y + bh / 2;
                            SDL_RenderDrawLine(g_app.renderer, x1, y1, x2, y2);
                        }
                    }
                    /* Draw boxes */
                    SDL_Rect r;
                    /* Primary box */
                    r.x = ui_primary.x;
                    r.y = ui_primary.y;
                    r.w = bw;
                    r.h = bh;
                    SDL_SetRenderDrawColor(g_app.renderer, 90, 110, 220, 230);
                    SDL_RenderFillRect(g_app.renderer, &r);
                    SDL_SetRenderDrawColor(g_app.renderer, 20, 20, 40, 255);
                    SDL_RenderDrawRect(g_app.renderer, &r);
                    /* Node boxes */
                    for (int i = 0; i < node_count && i < 3; ++i)
                    {
                        r.x = ui_nodes[i].x;
                        r.y = ui_nodes[i].y;
                        r.w = bw;
                        r.h = bh;
                        int valid = (nodes[i].effect_spec_id > 0);
                        if (valid)
                            SDL_SetRenderDrawColor(g_app.renderer, 120, 180, 120, 230);
                        else
                            SDL_SetRenderDrawColor(g_app.renderer, 160, 100, 100, 230);
                        SDL_RenderFillRect(g_app.renderer, &r);
                        SDL_SetRenderDrawColor(g_app.renderer, 20, 20, 40, 255);
                        SDL_RenderDrawRect(g_app.renderer, &r);
                    }
                }
#endif

                /* Selection and parameter panel */
                static int sel_node = -1; /* -1 primary, 0..2 nodes */
                if (!mdown)
                {
                    if (overlay_box_hit(mx, my, ui_primary.x, ui_primary.y, bw, bh))
                        sel_node = -1;
                    for (int i = 0; i < node_count && i < 3; ++i)
                    {
                        if (overlay_box_hit(mx, my, ui_nodes[i].x, ui_nodes[i].y, bw, bh))
                            sel_node = i;
                    }
                }
                if (sel_node == -1)
                {
                    overlay_label("Selected: Primary");
                }
                else if (sel_node >= 0 && sel_node < node_count)
                {
                    char lab[64];
                    snprintf(lab, sizeof lab, "Selected: Node %d", sel_node + 1);
                    overlay_label(lab);
                }
                /* Parameter edit for selected */
                if (sel_node == -1)
                {
                    changed |= overlay_slider_int("Primary EffectSpec ID", &primary_id, -1, 4096);
                }
                else if (sel_node >= 0 && sel_node < node_count)
                {
                    changed |= overlay_slider_int("Node EffectSpec ID",
                                                  &nodes[sel_node].effect_spec_id, -1, 4096);
                    changed |= overlay_slider_float("Node Delay (ms)", &nodes[sel_node].delay_ms,
                                                    0.0f, 10000.0f);
                    changed |= overlay_slider_float("Node Duration (ms)",
                                                    &nodes[sel_node].duration_ms, 0.0f, 60000.0f);
                    changed |= overlay_slider_int("Node Repeat Count",
                                                  &nodes[sel_node].repeat_count, 0, 100);
                    changed |=
                        overlay_slider_float("Node Repeat Interval (ms)",
                                             &nodes[sel_node].repeat_interval_ms, 0.0f, 10000.0f);
                    int hp_gate2 = nodes[sel_node].require_player_health_below_pct;
                    if (overlay_slider_int("Node HP Below % (gate)", &hp_gate2, 0, 100))
                    {
                        nodes[sel_node].require_player_health_below_pct = (unsigned char) hp_gate2;
                        changed = 1;
                    }
                }

                /* Simple chaining helper: chain nodes in order left->right */
                if (overlay_button("Chain Nodes (set delays from order)"))
                {
                    /* Sort a temp array of indices by x position */
                    int order[3];
                    int ocount = node_count < 3 ? node_count : 3;
                    for (int i = 0; i < ocount; ++i)
                        order[i] = i;
                    for (int a = 0; a < ocount; ++a)
                        for (int b = a + 1; b < ocount; ++b)
                            if (ui_nodes[order[a]].x > ui_nodes[order[b]].x)
                            {
                                int t = order[a];
                                order[a] = order[b];
                                order[b] = t;
                            }
                    float t_ms = 0.0f;
                    for (int i = 0; i < ocount; ++i)
                    {
                        int idx = order[i];
                        nodes[idx].delay_ms = t_ms;
                        /* Advance by this node's own timing */
                        float add = nodes[idx].duration_ms;
                        if (nodes[idx].repeat_count > 0 && nodes[idx].repeat_interval_ms > 0.0f)
                        {
                            add = nodes[idx].repeat_count * nodes[idx].repeat_interval_ms;
                        }
                        t_ms += add;
                    }
                    changed = 1;
                }
            }
        }
        /* Continue with direct ID editing controls */
        changed |= overlay_slider_int("Primary EffectSpec ID", &primary_id, -1, 4096);
        if (primary_id > 0)
        {
            const RogueEffectSpec* s = rogue_effect_get(primary_id);
            overlay_label(s ? "Primary: OK" : "Primary: INVALID id");
        }
        else
        {
            overlay_label("Primary: (unset)");
        }
        int display_count = node_count;
        if (overlay_slider_int("Additional Nodes (0..3)", &display_count, 0, 3))
        {
            if (display_count < 0)
                display_count = 0;
            if (display_count > 3)
                display_count = 3;
            if (display_count > node_count)
            {
                for (int i = node_count; i < display_count; ++i)
                {
                    nodes[i].effect_spec_id = -1;
                    nodes[i].delay_ms = 0.0f;
                    nodes[i].duration_ms = 0.0f;
                    nodes[i].repeat_count = 0;
                    nodes[i].repeat_interval_ms = 0.0f;
                    nodes[i].require_player_health_below_pct = 0;
                }
            }
            node_count = display_count;
            changed = 1;
        }
        for (int i = 0; i < node_count; ++i)
        {
            char hdr[64];
            snprintf(hdr, sizeof hdr, "Node %d", i + 1);
            overlay_label(hdr);
            changed |= overlay_slider_int("  EffectSpec ID", &nodes[i].effect_spec_id, -1, 4096);
            if (nodes[i].effect_spec_id > 0)
            {
                const RogueEffectSpec* s = rogue_effect_get(nodes[i].effect_spec_id);
                overlay_label(s ? "  Effect: OK" : "  Effect: INVALID id");
            }
            else
            {
                overlay_label("  Effect: (unset)");
            }
            changed |= overlay_slider_float("  Delay (ms)", &nodes[i].delay_ms, 0.0f, 10000.0f);
            changed |=
                overlay_slider_float("  Duration (ms)", &nodes[i].duration_ms, 0.0f, 60000.0f);
            changed |= overlay_slider_int("  Repeat Count", &nodes[i].repeat_count, 0, 100);
            changed |= overlay_slider_float("  Repeat Interval (ms)", &nodes[i].repeat_interval_ms,
                                            0.0f, 10000.0f);
            int hp_gate = nodes[i].require_player_health_below_pct;
            changed |= overlay_slider_int("  HP Below % (gate)", &hp_gate, 0, 100);
            nodes[i].require_player_health_below_pct = (unsigned char) hp_gate;
        }
        if (changed)
        {
            (void) rogue_skill_debug_set_effects(sel, primary_id, nodes, node_count);
            (void) rogue_skill_debug_save_overrides("build/skills_overrides.json");
            char emsg[192] = {0};
            int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
            last_valid_ok = ok;
            if (!ok)
                snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
            else
                snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
        }
    }

    /* Testing tab: move simulation here and add a lightweight preview -------- */
    if (tab == 4)
    {
        overlay_label("Simulation Profile");
        overlay_slider_float("Duration (ms)", &sim_duration_ms, 50.0f, 60000.0f);
        overlay_slider_float("Tick (ms)", &sim_tick_ms, 1.0f, 100.0f);
        overlay_slider_float("AP regen (/sec)", &sim_ap_regen_per_sec, 0.0f, 200.0f);
        overlay_input_text("Priority IDs (comma)", prio_buf, sizeof prio_buf);

        if (overlay_button("Simulate"))
        {
            char profile[256];
            char prio_json[128] = {0};
            int pj = 0;
            prio_json[pj++] = '[';
            if (prio_buf[0] == '\0')
                pj += snprintf(prio_json + pj, (int) sizeof prio_json - pj, "%d", sel);
            else
            {
                for (const char* p = prio_buf; *p && pj + 1 < (int) sizeof prio_json; ++p)
                {
                    char c = *p;
                    if ((c >= '0' && c <= '9') || c == ',' || c == '-')
                        prio_json[pj++] = c;
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
            snprintf(
                profile, sizeof profile,
                "{\"duration_ms\":%d,\"tick_ms\":%.1f,\"ap_regen_per_sec\":%.1f,\"priority\":%s}",
                (int) sim_duration_ms, sim_tick_ms, sim_ap_regen_per_sec, prio_json);
            if (rogue_skill_debug_simulate(profile, sim_result, (int) sizeof sim_result) != 0)
                snprintf(sim_result, sizeof sim_result, "Simulation failed");
        }
        if (sim_result[0])
            overlay_label(sim_result);

        /* Real-time preview window (sprite-based) */
        {
            static int preview_enabled = 1;
            static int preview_autoplay = 1;
            static int preview_zoom = 2; /* 1..8 */
            overlay_checkbox("Enable Real-time Preview", &preview_enabled);
            overlay_checkbox("Auto-animate", &preview_autoplay);
            overlay_slider_int("Zoom", &preview_zoom, 1, 8);

            /* Preview area (fixed rectangle inside this panel) */
            const int panel_x = 380; /* must match overlay_begin_panel call */
            const int panel_y = 10;
            const int pv_x = panel_x + 12;
            const int pv_y = panel_y + 260; /* placed beneath controls */
            const int pv_w = 396;
            const int pv_h = 140;

#ifdef ROGUE_HAVE_SDL
            if (!g_app.headless && g_app.renderer)
            {
                SDL_Rect r = {pv_x, pv_y, pv_w, pv_h};
                SDL_SetRenderDrawColor(g_app.renderer, 12, 12, 18, 220);
                SDL_RenderFillRect(g_app.renderer, &r);
                SDL_SetRenderDrawColor(g_app.renderer, 70, 90, 130, 230);
                SDL_RenderDrawRect(g_app.renderer, &r);
            }
#endif

            if (preview_enabled)
            {
                static PreviewTex t_cast = {"", {0}};
                static PreviewTex t_proj = {"", {0}};
                static PreviewTex t_impact = {"", {0}};
                static PreviewTex t_aoe = {"", {0}};

                int stype = 0;
                (void) rogue_skill_debug_get_type(sel, &stype);
                RogueSkillVisualParams vis;
                if (rogue_skill_debug_get_visuals(sel, &vis) == 0)
                {
                    preview_ensure_tex(&t_cast, vis.cast_sprite_sheet);
                    preview_ensure_tex(&t_proj, vis.projectile_sprite);
                    preview_ensure_tex(&t_impact, vis.impact_sprite);
                    preview_ensure_tex(&t_aoe, vis.aoe_sprite);

                    static float anim_t = 0.0f;
                    if (preview_autoplay)
                        anim_t += overlay_last_dt();

                    /* Choose what to show by type; fallbacks if missing */
                    const PreviewTex* show = NULL;
                    int is_sheet = 0;
                    if (stype == 2 && t_proj.ready)
                        show = &t_proj; /* RANGED */
                    else if (stype == 3 && t_aoe.ready)
                        show = &t_aoe; /* AOE */
                    else if (t_cast.ready)
                    {
                        show = &t_cast; /* general cast animation */
                        is_sheet = 1;
                    }
                    else if (t_impact.ready)
                        show = &t_impact;

                    if (show && show->ready)
                    {
                        /* Compute sprite frame */
                        RogueSprite spr = {0};
                        spr.tex = (RogueTexture*) &show->tex;
                        int cell_w = show->tex.w;
                        int cell_h = show->tex.h;
                        int grid_w = vis.grid_width > 0 ? vis.grid_width : 1;
                        int grid_h = vis.grid_height > 0 ? vis.grid_height : 1;
                        int frames = vis.frame_count > 0 ? vis.frame_count : (grid_w * grid_h);
                        if (is_sheet && grid_w > 0 && grid_h > 0)
                        {
                            cell_w = (grid_w > 0 ? show->tex.w / grid_w : show->tex.w);
                            cell_h = (grid_h > 0 ? show->tex.h / grid_h : show->tex.h);
                            float fd = (vis.frame_duration_ms > 0 ? vis.frame_duration_ms : 100.0f);
                            int f = (int) (anim_t * 1000.0f / fd);
                            if (!vis.animation_loops && f >= frames)
                                f = frames - 1;
                            if (frames > 0)
                                f = f % frames;
                            int fx = (frames > 0 ? (f % grid_w) : 0);
                            int fy = (frames > 0 ? (f / grid_w) : 0);
                            spr.sx = fx * cell_w;
                            spr.sy = fy * cell_h;
                            spr.sw = cell_w;
                            spr.sh = cell_h;
                        }
                        else
                        {
                            spr.sx = 0;
                            spr.sy = 0;
                            spr.sw = cell_w;
                            spr.sh = cell_h;
                        }

                        /* Center in preview rect */
                        int scale = (preview_zoom < 1 ? 1 : preview_zoom);
                        int dx = pv_x + (pv_w - spr.sw * scale) / 2;
                        int dy = pv_y + (pv_h - spr.sh * scale) / 2;
                        rogue_sprite_draw(&spr, dx, dy, scale);
                    }
                    else
                    {
                        overlay_label("Preview: (no sprite configured)");
                    }
                }
            }
        }
    }

    if (overlay_button("Save Overrides JSON"))
    {
        char vmsg[192] = {0};
        int v = rogue_skill_debug_validate(vmsg, (int) sizeof vmsg);
        if (v != 0)
        {
            char warn[256];
            snprintf(warn, sizeof warn, "Validation failed: %s", vmsg[0] ? vmsg : "(no details)");
            overlay_label(warn);
        }
        int rc = (v == 0) ? rogue_skill_debug_save_overrides(overrides_path) : -3;
        char msg[128];
        snprintf(msg, sizeof msg, "Save: %s (%d)", (rc == 0 ? "OK" : "ERR"), rc);
        overlay_label(msg);
        /* Update sticky status based on cross-rule validation */
        char emsg[192] = {0};
        int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
        last_valid_ok = ok;
        if (!ok)
            snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
        else
            snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
    }
    if (overlay_button("Load Overrides JSON"))
    {
        int applied = rogue_skill_debug_load_overrides_file(overrides_path);
        char msg[128];
        snprintf(msg, sizeof msg, "Load: %s (%d)", (applied >= 0 ? "OK" : "ERR"), applied);
        overlay_label(msg);
        /* Update sticky status after load */
        char emsg[192] = {0};
        int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
        last_valid_ok = ok;
        if (!ok)
            snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
        else
            snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
    }

    if (overlay_checkbox("Auto-Reload Overrides", &auto_reload))
    {
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

void rogue_overlay_register_panel_skills(void)
{
    overlay_register_panel("skills", "Skills", panel_skills, NULL);
}

#endif
