#include "../core/app/app_state.h"
#include "../core/skills/skill_debug.h"
#include "../core/skills/skills_coeffs.h"
#include "../core/skills/skills_validate.h"
#include "../graphics/effect_spec.h"
#include "../graphics/sprite.h"
#include "overlay_core.h"
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
    if (!overlay_begin_panel("Skills", 380, 10, 420))
        return;
    /* Simple tabs (combo-based): Overview, Effects, Visuals, Audio, Testing */
    static int tab = 0;
    static const char* tab_names[] = {"Overview", "Effects", "Visuals", "Audio", "Testing"};
    overlay_combo("Tab", &tab, tab_names, 5);
    static float sim_duration_ms = 2000.0f;
    static float sim_tick_ms = 16.0f;
    static float sim_ap_regen_per_sec = 0.0f;
    static char prio_buf[128] = "";
    static char sim_result[256] = "";
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
    char buf[256];
    snprintf(buf, sizeof buf, "[%d] %s", sel, name ? name : "<noname>");
    overlay_label(buf);
    /*
     * Validation Status (prominent)
     * - Sticky status updated on edits to surface problems early.
     */
    static int last_valid_ok = 1;
    static char last_valid_msg[196] = "OK";
    {
        char line[256];
        if (last_valid_ok)
            snprintf(line, sizeof line, "[Validation] OK");
        else
            snprintf(line, sizeof line, "[Validation] ERROR: %s", last_valid_msg);
        overlay_label(line);
    }

    /* Overview tab content ----------------------------------------------------- */
    if (tab == 0)
    {
        /* Creation wizard */
        overlay_label("Create New Skill");
        static char new_name[64] = "";
        static int new_max_rank = 5;
        static float new_base_cd = 1000.0f;
        static float new_cd_red = 0.0f;
        static float new_cast_ms = 0.0f;
        static int new_is_passive = 0;
        static int tmpl_id = -1;
        static int copy_coeffs = 1;
        /* Template from existing skill */
        overlay_slider_int("Template Skill Id", &tmpl_id, -1, count - 1);
        overlay_checkbox("Copy Coeffs", &copy_coeffs);
        /* Searchable template picker */
        static char tmpl_filter[64] = "";
        overlay_input_text("Template Filter", tmpl_filter, sizeof tmpl_filter);
        {
            const char* headers[] = {"ID", "Name"};
            int sort_col = 0;
            int sort_dir = 0;
            int selected = (tmpl_id >= 0 && tmpl_id < count) ? tmpl_id : -1;
            if (overlay_table_begin("skills_tmpl", headers, 2, &sort_col, &sort_dir, NULL))
            {
                for (int i = 0; i < count; ++i)
                {
                    const char* sname = rogue_skill_debug_name(i);
                    if (!sname)
                        continue;
                    /* Simple substring filter (case-sensitive) */
                    if (tmpl_filter[0] != '\0')
                    {
                        const char* p = sname;
                        const char* f = tmpl_filter;
                        const char* hit = NULL;
                        /* naive strstr to avoid including string.h here */
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
                                hit = p; /* matched */
                        }
                        if (!hit)
                            continue;
                    }
                    char id_s[16];
                    snprintf(id_s, sizeof id_s, "%d", i);
                    const char* cells[] = {id_s, sname};
                    (void) overlay_table_row(cells, 2, i, &selected);
                }
                overlay_table_end();
            }
            tmpl_id = selected;
        }
        if (overlay_button("Apply Template") && tmpl_id >= 0 && tmpl_id < count)
        {
            /* Prefill fields from template */
            const char* tname = rogue_skill_debug_name(tmpl_id);
            if (tname)
            {
                snprintf(new_name, sizeof new_name, "%s_Copy", tname);
            }
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
                {
                    /* Apply coeffs to current selection for preview; creation will only set timing.
                     */
                    rogue_skill_debug_set_coeff(sel, &tcp);
                }
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
            /* Validate current registry before allowing a new create to be committed */
            char vmsg[192] = {0};
            if (rogue_skill_debug_validate(vmsg, (int) sizeof vmsg) != 0)
            {
                char warn[256];
                snprintf(warn, sizeof warn, "Validation failed: %s",
                         vmsg[0] ? vmsg : "(no details)");
                overlay_label(warn);
            }
            /* Also run cross-rule validation to update sticky status */
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
                /* Reset name for next */
                new_name[0] = '\0';
                /* If template coeffs were requested and available, copy them into the new id */
                if (copy_coeffs && tmpl_id >= 0 && tmpl_id < count)
                {
                    RogueSkillCoeffParams tcp;
                    if (rogue_skill_debug_get_coeff(tmpl_id, &tcp) == 0)
                    {
                        rogue_skill_debug_set_coeff(idx, &tcp);
                    }
                }
            }
            else
            {
                snprintf(msg, sizeof msg, "Create failed (%d)", idx);
            }
            overlay_label(msg);
        }

        /* Meta */
        overlay_label("Meta");
        int stype = 0;
        if (rogue_skill_debug_get_type(sel, &stype) == 0)
        {
            int changed = 0;
            /* Simple slider for enum 0..9, with a helper label */
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
                /* Refresh validation status */
                char emsg[192] = {0};
                int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
                last_valid_ok = ok;
                if (!ok)
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
                else
                    snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
            }

            /* Type Presets: apply recommended timing/coeff/params by type (asset paths left blank)
             */
            overlay_label("Type Presets");
            static int preset_idx = 0;
            /* One preset per type for now */
            preset_idx = stype; /* default to current type */
            int apply = overlay_button("Apply Preset for Type");
            if (apply)
            {
                /* Timing defaults */
                float p_base_cd = 1000.f, p_cd_red = 0.f, p_cast = 0.f;
                RogueSkillCoeffParams pcp;
                memset(&pcp, 0, sizeof pcp);
                pcp.base_scalar = 1.0f;
                pcp.per_rank_scalar = 0.1f;
                switch (stype)
                {
                case 1: /* MELEE */
                    p_base_cd = 800.f;
                    p_cast = 200.f;
                    pcp.base_scalar = 1.00f;
                    break;
                case 2: /* RANGED */
                    p_base_cd = 900.f;
                    p_cast = 250.f;
                    pcp.base_scalar = 0.95f;
                    break;
                case 3: /* AOE_SPELL */
                    p_base_cd = 1200.f;
                    p_cast = 400.f;
                    pcp.base_scalar = 0.85f;
                    break;
                case 4: /* BUFF */
                    p_base_cd = 15000.f;
                    p_cast = 300.f;
                    pcp.base_scalar = 0.0f; /* non-damage by default */
                    break;
                case 5: /* DEBUFF */
                    p_base_cd = 12000.f;
                    p_cast = 300.f;
                    pcp.base_scalar = 0.2f;
                    break;
                case 6: /* HEAL */
                    p_base_cd = 5000.f;
                    p_cast = 350.f;
                    pcp.base_scalar = 0.0f;
                    break;
                case 7: /* SUMMON */
                    p_base_cd = 20000.f;
                    p_cast = 500.f;
                    pcp.base_scalar = 0.0f;
                    break;
                case 8: /* PASSIVE */
                    p_base_cd = 0.f;
                    p_cast = 0.f;
                    pcp.base_scalar = 0.0f;
                    break;
                case 9: /* ULTIMATE */
                    p_base_cd = 60000.f;
                    p_cast = 800.f;
                    pcp.base_scalar = 1.5f;
                    break;
                default: /* UNKNOWN */
                    p_base_cd = 1000.f;
                    p_cast = 0.f;
                    pcp.base_scalar = 0.5f;
                    break;
                }
                rogue_skill_debug_set_timing(sel, p_base_cd, p_cd_red, p_cast);
                rogue_skill_debug_set_coeff(sel, &pcp);
                (void) rogue_skill_debug_save_overrides(overrides_path);
                /* Refresh validation status */
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
        int changed = 0;
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
