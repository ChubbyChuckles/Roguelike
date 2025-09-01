#include "panel_skills_overview.h"
#include "../../core/app/app_state.h"
#include "../../core/skills/skill_debug.h"
#include "../../core/skills/skills_validate.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "panel_skills_shared.h"
#include <string.h>

#if ROGUE_ENABLE_DEBUG_OVERLAY

void panel_skills_draw_overview(int* sel)
{
    if (!sel)
        return;
    const char* overrides_path = panel_skills_overrides_path();
    /* Sticky validation banner */
    {
        const char* msg = panel_skills_last_valid_msg();
        char vline[192];
        snprintf(vline, sizeof vline, "Validation: %s%s%s",
                 panel_skills_last_valid_ok() ? "OK" : "ERROR",
                 panel_skills_last_valid_ok() ? "" : ": ", panel_skills_last_valid_ok() ? "" : msg);
        overlay_label(vline);
    }

    int count = rogue_skill_debug_count();
    if (count <= 0)
    {
        overlay_label("No skills registered");
        return;
    }

    overlay_slider_int("Skill Index", sel, 0, count - 1);

    /* Template/new-skill helpers */
    static int tmpl_id = -1;
    static char tmpl_filter[64] = "";
    static char new_name[64] = "";
    static int new_max_rank = 5;
    static float new_base_cd = 1000.f;
    static float new_cd_red = 0.f;
    static float new_cast_ms = 0.f;
    static int new_is_passive = 0;
    static int copy_coeffs = 1;

    overlay_label("Templates");
    overlay_input_text("Filter", tmpl_filter, sizeof tmpl_filter);
    const char* headers[] = {"ID", "Name"};
    int sort_col = 0, sort_dir = 0;
    int selected = (tmpl_id >= 0 && tmpl_id < count) ? tmpl_id : -1;
    if (overlay_table_begin("skills_tmpl", headers, 2, &sort_col, &sort_dir, NULL))
    {
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
                rogue_skill_debug_set_coeff(*sel, &tcp);
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
            snprintf(warn, sizeof warn, "Validation failed: %s", vmsg[0] ? vmsg : "(no details)");
            overlay_label(warn);
        }
        panel_skills_refresh_validation();
        int idx = rogue_skill_debug_create(new_name, new_max_rank, new_base_cd, new_cd_red,
                                           new_cast_ms, new_is_passive);
        char msg[128];
        if (idx >= 0)
        {
            snprintf(msg, sizeof msg, "Created skill '%s' at id %d", new_name, idx);
            *sel = idx;
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
    if (rogue_skill_debug_get_type(*sel, &stype) == 0)
    {
        int changed = 0;
        changed |= overlay_slider_int("Skill Type (0..9)", &stype, 0, 9);
        static const char* type_names[10] = {"UNKNOWN", "MELEE", "RANGED", "AOE_SPELL", "BUFF",
                                             "DEBUFF",  "HEAL",  "SUMMON", "PASSIVE",   "ULTIMATE"};
        if (stype >= 0 && stype <= 9)
        {
            char tn[64];
            snprintf(tn, sizeof tn, "Type: %s", type_names[stype]);
            overlay_label(tn);
        }
        if (changed)
        {
            rogue_skill_debug_set_type(*sel, stype);
            panel_skills_save_overrides_and_refresh();
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
            rogue_skill_debug_set_timing(*sel, p_base_cd, p_cd_red, p_cast);
            rogue_skill_debug_set_coeff(*sel, &pcp);
            panel_skills_save_overrides_and_refresh();
        }
    }

    float base_cd = 0.f, cd_red = 0.f, cast_ms = 0.f;
    if (rogue_skill_debug_get_timing(*sel, &base_cd, &cd_red, &cast_ms) == 0)
    {
        if (overlay_slider_float("Base Cooldown (ms)", &base_cd, 0.f, 60000.f) ||
            overlay_slider_float("CD Reduction/rank (ms)", &cd_red, -1000.f, 1000.f) ||
            overlay_slider_float("Cast Time (ms)", &cast_ms, 0.f, 5000.f))
        {
            rogue_skill_debug_set_timing(*sel, base_cd, cd_red, cast_ms);
            panel_skills_save_overrides_and_refresh();
        }
    }

    RogueSkillCoeffParams cp;
    if (rogue_skill_debug_get_coeff(*sel, &cp) == 0)
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
            rogue_skill_debug_set_coeff(*sel, &cp);
            panel_skills_save_overrides_and_refresh();
        }
    }
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
