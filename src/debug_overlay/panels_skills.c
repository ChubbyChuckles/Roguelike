#include "../core/skills/skill_debug.h"
#include "../core/skills/skills_coeffs.h"
#include "overlay_core.h"
#include "overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_skills(void* user)
{
    (void) user;
    if (!overlay_begin_panel("Skills", 380, 10, 420))
        return;
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
                /* Apply coeffs to current selection for preview; creation will only set timing. */
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

    float base_cd = 0.f, cd_red = 0.f, cast_ms = 0.f;
    if (rogue_skill_debug_get_timing(sel, &base_cd, &cd_red, &cast_ms) == 0)
    {
        if (overlay_slider_float("Base Cooldown (ms)", &base_cd, 0.f, 60000.f) ||
            overlay_slider_float("CD Reduction/rank (ms)", &cd_red, -1000.f, 1000.f) ||
            overlay_slider_float("Cast Time (ms)", &cast_ms, 0.f, 5000.f))
        {
            rogue_skill_debug_set_timing(sel, base_cd, cd_red, cast_ms);
            (void) rogue_skill_debug_save_overrides(overrides_path);
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
        }
    }

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
        snprintf(profile, sizeof profile,
                 "{\"duration_ms\":%d,\"tick_ms\":%.1f,\"ap_regen_per_sec\":%.1f,\"priority\":%s}",
                 (int) sim_duration_ms, sim_tick_ms, sim_ap_regen_per_sec, prio_json);
        if (rogue_skill_debug_simulate(profile, sim_result, (int) sizeof sim_result) != 0)
            snprintf(sim_result, sizeof sim_result, "Simulation failed");
    }
    if (sim_result[0])
        overlay_label(sim_result);

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
