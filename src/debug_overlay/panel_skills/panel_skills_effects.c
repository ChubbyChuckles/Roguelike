#include "panel_skills_effects.h"
#include "../../core/app/app_state.h"
#include "../../core/skills/skill_debug.h"
#include "../../game/buffs.h"
#include "../../graphics/effect_spec.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "panel_skills_shared.h"
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* Small utility for box hit-testing */
static int overlay_box_hit_local(int mx, int my, int bx, int by, int bw, int bh)
{
    return (mx >= bx && mx < bx + bw && my >= by && my < by + bh) ? 1 : 0;
}

/* Palette helper: map an EffectSpec to overlay categories based on kind/debuff/buff_type. */
static int palette_effect_categories_local(const RogueEffectSpec* es)
{
    if (!es)
        return 0;
    int cats = 0;
    if (es->debuff)
        cats |= ROGUE_BUFF_CAT_OFFENSIVE;
    if (es->kind == ROGUE_EFFECT_STAT_BUFF)
    {
        if (es->buff_type >= 0 && es->buff_type < ROGUE_BUFF_MAX)
            cats |= (int) rogue_buffs_type_categories((RogueBuffType) es->buff_type);
        else
            cats |= ROGUE_BUFF_CAT_UTILITY;
    }
    if (es->kind == ROGUE_EFFECT_AURA)
        cats |= ROGUE_BUFF_CAT_OFFENSIVE;
    return cats;
}

void panel_skills_draw_effects(int sel)
{
    const char* overrides_path = panel_skills_overrides_path();
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
    /* Local inline validation for quick feedback */
    {
        int local_errors = 0;
        char line[192];
        if (primary_id > 0 && rogue_effect_get(primary_id) == NULL)
        {
            snprintf(line, sizeof line, "ERROR: primary effect_spec_id=%d is invalid", primary_id);
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
    /* EffectSpec palette with filters */
    {
        static int palette_open = 1;
        static char eff_filter[64] = "";
        static int assign_target = -1;
        static int eff_selected = -1;
        static int kind_filter = -1;
        static int debuff_only = 0;
        static int cat_filter = 0;
        overlay_checkbox("Show EffectSpec Palette", &palette_open);
        if (palette_open)
        {
            overlay_input_text("Filter (id substring)", eff_filter, sizeof eff_filter);
            overlay_slider_int("Assign To: -1=Primary, 0..2 Node", &assign_target, -1, 2);
            overlay_slider_int("Kind Filter (-1 any, 0 buff, 1 dot, 2 aura)", &kind_filter, -1, 2);
            overlay_checkbox("Debuff only", &debuff_only);
            overlay_label("Category Filter (toggle to OR):");
            int widths[4] = {120, 120, 120, 120};
            overlay_columns_begin(4, widths);
            int t_off = (cat_filter & ROGUE_BUFF_CAT_OFFENSIVE) != 0;
            int t_def = (cat_filter & ROGUE_BUFF_CAT_DEFENSIVE) != 0;
            int t_mov = (cat_filter & ROGUE_BUFF_CAT_MOVEMENT) != 0;
            int t_utl = (cat_filter & ROGUE_BUFF_CAT_UTILITY) != 0;
            (void) overlay_checkbox("Offensive", &t_off);
            overlay_next_column();
            (void) overlay_checkbox("Defensive", &t_def);
            overlay_next_column();
            (void) overlay_checkbox("Movement", &t_mov);
            overlay_next_column();
            (void) overlay_checkbox("Utility", &t_utl);
            overlay_columns_end();
            cat_filter = 0;
            if (t_off)
                cat_filter |= ROGUE_BUFF_CAT_OFFENSIVE;
            if (t_def)
                cat_filter |= ROGUE_BUFF_CAT_DEFENSIVE;
            if (t_mov)
                cat_filter |= ROGUE_BUFF_CAT_MOVEMENT;
            if (t_utl)
                cat_filter |= ROGUE_BUFF_CAT_UTILITY;
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
                    if (cat_filter != 0)
                    {
                        int cats = palette_effect_categories_local(es);
                        if ((cats & cat_filter) == 0)
                            continue;
                    }
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
    /* Node Graph Editor */
    {
        static int graph_enabled = 1;
        overlay_checkbox("Enable Node Graph Editor", &graph_enabled);
        if (graph_enabled)
        {
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
                int id;
                int x, y;
            } NodeUI;
            static int last_skill = -1;
            static NodeUI ui_primary = {-1, 0, 0};
            static NodeUI ui_nodes[3];
            static int ui_inited = 0;
            static int dragging = 0;
            static int drag_dx = 0, drag_dy = 0;
            static int parent_of[3] = {-2, -2, -2};
            static int linking = 0;
            static int link_source = -2;
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
                    parent_of[i] = -2;
                }
                dragging = 99;
                linking = 0;
                link_source = -2;
                ui_inited = 1;
            }
            int mx = 0, my = 0;
            int mdown = 0;
            static int was_down = 0;
            const int bw = 64, bh = 32;
#ifdef ROGUE_HAVE_SDL
            if (!g_app.headless)
            {
                Uint32 mask = SDL_GetMouseState(&mx, &my);
                mdown = (mask & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            }
#endif
            if (mdown && !was_down)
            {
                if (overlay_box_hit_local(mx, my, ui_primary.x, ui_primary.y, bw, bh))
                {
                    dragging = -1;
                    drag_dx = mx - ui_primary.x;
                    drag_dy = my - ui_primary.y;
                }
                else
                {
                    for (int i = 0; i < node_count && i < 3; ++i)
                    {
                        if (overlay_box_hit_local(mx, my, ui_nodes[i].x, ui_nodes[i].y, bw, bh))
                        {
                            dragging = i;
                            drag_dx = mx - ui_nodes[i].x;
                            drag_dy = my - ui_nodes[i].y;
                            break;
                        }
                    }
                }
            }
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

#ifdef ROGUE_HAVE_SDL
            if (!g_app.headless && g_app.renderer)
            {
                SDL_SetRenderDrawColor(g_app.renderer, 60, 160, 200, 255);
                for (int i = 0; i < node_count && i < 3; ++i)
                {
                    if (nodes[i].effect_spec_id <= 0)
                        continue;
                    int p = parent_of[i];
                    if (p == -2)
                        continue;
                    int x1 = 0, y1 = 0;
                    if (p == -1)
                    {
                        x1 = ui_primary.x + bw;
                        y1 = ui_primary.y + bh / 2;
                    }
                    else if (p >= 0 && p < 3)
                    {
                        x1 = ui_nodes[p].x + bw;
                        y1 = ui_nodes[p].y + bh / 2;
                    }
                    int x2 = ui_nodes[i].x;
                    int y2 = ui_nodes[i].y + bh / 2;
                    SDL_RenderDrawLine(g_app.renderer, x1, y1, x2, y2);
                }
                SDL_Rect r;
                r.x = ui_primary.x;
                r.y = ui_primary.y;
                r.w = bw;
                r.h = bh;
                SDL_SetRenderDrawColor(g_app.renderer, 90, 110, 220, 230);
                SDL_RenderFillRect(g_app.renderer, &r);
                SDL_SetRenderDrawColor(g_app.renderer, 20, 20, 40, 255);
                SDL_RenderDrawRect(g_app.renderer, &r);
                for (int i = 0; i < node_count && i < 3; ++i)
                {
                    r.x = ui_nodes[i].x;
                    r.y = ui_nodes[i].y;
                    r.w = bw;
                    r.h = bh;
                    int valid = (nodes[i].effect_spec_id > 0);
                    int has_timing_issue = 0;
                    if (valid)
                    {
                        if (nodes[i].duration_ms < 0.0f)
                            has_timing_issue = 1;
                        if (nodes[i].repeat_count < 0 || nodes[i].repeat_count > 32)
                            has_timing_issue = 1;
                        if (nodes[i].repeat_count == 0 && nodes[i].duration_ms > 0.0f &&
                            nodes[i].repeat_interval_ms <= 0.0f)
                            has_timing_issue = 1;
                        if (nodes[i].require_player_health_below_pct > 100)
                            has_timing_issue = 1;
                    }
                    if (!valid)
                        SDL_SetRenderDrawColor(g_app.renderer, 160, 80, 80, 230);
                    else if (has_timing_issue)
                        SDL_SetRenderDrawColor(g_app.renderer, 200, 150, 80, 230);
                    else
                        SDL_SetRenderDrawColor(g_app.renderer, 120, 180, 120, 230);
                    SDL_RenderFillRect(g_app.renderer, &r);
                    SDL_SetRenderDrawColor(g_app.renderer, 20, 20, 40, 255);
                    SDL_RenderDrawRect(g_app.renderer, &r);
                }
            }
#endif
            static int sel_node = -1;
            if (!mdown)
            {
                if (overlay_box_hit_local(mx, my, ui_primary.x, ui_primary.y, bw, bh))
                    sel_node = -1;
                for (int i = 0; i < node_count && i < 3; ++i)
                {
                    if (overlay_box_hit_local(mx, my, ui_nodes[i].x, ui_nodes[i].y, bw, bh))
                        sel_node = i;
                }
            }
            if (sel_node == -1)
                overlay_label("Selected: Primary");
            else if (sel_node >= 0 && sel_node < node_count)
            {
                char lab[64];
                snprintf(lab, sizeof lab, "Selected: Node %d", sel_node + 1);
                overlay_label(lab);
            }
            for (int i = 0; i < node_count && i < 3; ++i)
            {
                if (nodes[i].effect_spec_id > 0 && parent_of[i] == -2)
                    parent_of[i] = -1;
            }
            if (overlay_button("Start Link from Selected"))
            {
                linking = 1;
                link_source = sel_node;
            }
            if (linking)
            {
                overlay_label("Link mode: click a target node to connect, or Cancel.");
                if (overlay_button("Cancel Link"))
                {
                    linking = 0;
                    link_source = -2;
                }
                for (int i = 0; i < node_count && i < 3; ++i)
                {
                    char btxt[64];
                    snprintf(btxt, sizeof btxt, "Connect -> Node %d", i + 1);
                    if (overlay_button(btxt))
                    {
                        int src = link_source;
                        int tgt = i;
                        int cycle = 0;
                        if (src == tgt)
                            cycle = 1;
                        if (!cycle && src >= 0)
                        {
                            int v = src;
                            for (int it = 0; it < 4; ++it)
                            {
                                if (v == -1)
                                    break;
                                if (v == tgt)
                                {
                                    cycle = 1;
                                    break;
                                }
                                int pv = parent_of[v];
                                if (pv == -2)
                                    break;
                                v = pv;
                            }
                        }
                        if (!cycle)
                        {
                            parent_of[tgt] = src;
                        }
                        linking = 0;
                        link_source = -2;
                    }
                }
            }
            if (sel_node >= 0 && sel_node < node_count)
            {
                if (overlay_button("Unlink Selected"))
                    parent_of[sel_node] = -2;
                if (overlay_button("Clear Selected Node"))
                {
                    nodes[sel_node].effect_spec_id = -1;
                    nodes[sel_node].delay_ms = 0.0f;
                    nodes[sel_node].duration_ms = 0.0f;
                    nodes[sel_node].repeat_count = 0;
                    nodes[sel_node].repeat_interval_ms = 0.0f;
                    nodes[sel_node].require_player_health_below_pct = 0;
                    parent_of[sel_node] = -2;
                    changed = 1;
                }
            }
            if (sel_node == -1)
            {
                changed |= overlay_slider_int("Primary EffectSpec ID", &primary_id, -1, 4096);
            }
            else if (sel_node >= 0 && sel_node < node_count)
            {
                changed |= overlay_slider_int("Node EffectSpec ID", &nodes[sel_node].effect_spec_id,
                                              -1, 4096);
                changed |= overlay_slider_float("Node Delay (ms)", &nodes[sel_node].delay_ms, 0.0f,
                                                10000.0f);
                changed |= overlay_slider_float("Node Duration (ms)", &nodes[sel_node].duration_ms,
                                                0.0f, 60000.0f);
                changed |=
                    overlay_slider_int("Node Repeat Count", &nodes[sel_node].repeat_count, 0, 100);
                changed |=
                    overlay_slider_float("Node Repeat Interval (ms)",
                                         &nodes[sel_node].repeat_interval_ms, 0.0f, 10000.0f);
                int hp_gate2 = nodes[sel_node].require_player_health_below_pct;
                if (overlay_slider_int("Node HP Below % (gate)", &hp_gate2, 0, 100))
                {
                    nodes[sel_node].require_player_health_below_pct = (unsigned char) hp_gate2;
                    changed = 1;
                }
                /* Quick Presets for broader node types */
                overlay_label("Presets:");
                if (overlay_button("Instant (one-shot)"))
                {
                    nodes[sel_node].duration_ms = 0.0f;
                    nodes[sel_node].repeat_count = 1;
                    nodes[sel_node].repeat_interval_ms = 0.0f;
                    changed = 1;
                }
                if (overlay_button("Periodic Window (use spec pulse if any)"))
                {
                    float pulse = 1000.0f;
                    if (nodes[sel_node].effect_spec_id > 0)
                    {
                        const RogueEffectSpec* es =
                            rogue_effect_get(nodes[sel_node].effect_spec_id);
                        if (es && es->pulse_period_ms > 0.0f)
                            pulse = (float) es->pulse_period_ms;
                    }
                    nodes[sel_node].repeat_count = 0;
                    nodes[sel_node].repeat_interval_ms = pulse;
                    if (nodes[sel_node].duration_ms <= 0.0f)
                        nodes[sel_node].duration_ms = pulse * 3.0f; /* default 3 ticks */
                    changed = 1;
                }
                if (overlay_button("Counted Pulses (derive count from spec if possible)"))
                {
                    float pulse = 1000.0f;
                    int count = 3;
                    float dur_hint = 0.0f;
                    if (nodes[sel_node].effect_spec_id > 0)
                    {
                        const RogueEffectSpec* es =
                            rogue_effect_get(nodes[sel_node].effect_spec_id);
                        if (es)
                        {
                            if (es->pulse_period_ms > 0.0f)
                                pulse = (float) es->pulse_period_ms;
                            if (es->duration_ms > 0.0f)
                                dur_hint = (float) es->duration_ms;
                        }
                    }
                    if (dur_hint > 0.0f)
                    {
                        int rc = (int) (dur_hint / pulse);
                        if (rc < 1)
                            rc = 1;
                        if (rc > 32)
                            rc = 32;
                        count = rc;
                    }
                    nodes[sel_node].repeat_count = count;
                    nodes[sel_node].repeat_interval_ms = pulse;
                    nodes[sel_node].duration_ms = 0.0f;
                    changed = 1;
                }
                /* Quick HP gate shortcuts */
                overlay_label("HP Gate:");
                if (overlay_button("0%"))
                {
                    nodes[sel_node].require_player_health_below_pct = 0;
                    changed = 1;
                }
                if (overlay_button("25%"))
                {
                    nodes[sel_node].require_player_health_below_pct = 25;
                    changed = 1;
                }
                if (overlay_button("50%"))
                {
                    nodes[sel_node].require_player_health_below_pct = 50;
                    changed = 1;
                }
                /* Repeat mode helper: 0 none, 1 count-based, 2 window-based */
                int repeat_mode = 0;
                if (nodes[sel_node].repeat_count > 0)
                    repeat_mode = 1;
                else if (nodes[sel_node].duration_ms > 0.0f &&
                         nodes[sel_node].repeat_interval_ms > 0.0f)
                    repeat_mode = 2;
                if (overlay_slider_int("Repeat Mode (0 none,1 count,2 window)", &repeat_mode, 0, 2))
                {
                    if (repeat_mode == 0)
                    {
                        nodes[sel_node].repeat_count = 0;
                        nodes[sel_node].repeat_interval_ms = 0.0f;
                        /* keep duration as-is */
                    }
                    else if (repeat_mode == 1)
                    {
                        if (nodes[sel_node].repeat_count == 0)
                            nodes[sel_node].repeat_count = 1;
                        if (nodes[sel_node].repeat_interval_ms <= 0.0f)
                            nodes[sel_node].repeat_interval_ms = 1000.0f;
                        nodes[sel_node].duration_ms = 0.0f;
                    }
                    else if (repeat_mode == 2)
                    {
                        if (nodes[sel_node].duration_ms <= 0.0f)
                            nodes[sel_node].duration_ms = nodes[sel_node].repeat_interval_ms > 0.0f
                                                              ? nodes[sel_node].repeat_interval_ms
                                                              : 1000.0f;
                        if (nodes[sel_node].repeat_interval_ms <= 0.0f)
                            nodes[sel_node].repeat_interval_ms = 1000.0f;
                        nodes[sel_node].repeat_count = 0;
                    }
                    changed = 1;
                }
                /* Contextual suggestions from EffectSpec */
                if (nodes[sel_node].effect_spec_id > 0)
                {
                    const RogueEffectSpec* es = rogue_effect_get(nodes[sel_node].effect_spec_id);
                    if (es)
                    {
                        overlay_label("Suggestions:");
                        /* Suggest using spec duration when set */
                        if (es->duration_ms > 0.0f &&
                            nodes[sel_node].duration_ms != es->duration_ms)
                        {
                            char s1[96];
                            snprintf(s1, sizeof s1, "Apply spec duration (%.0f ms)",
                                     es->duration_ms);
                            if (overlay_button(s1))
                            {
                                nodes[sel_node].duration_ms = es->duration_ms;
                                changed = 1;
                            }
                        }
                        /* Suggest pulse period for DOT/AURA when available */
                        if (es->pulse_period_ms > 0.0f &&
                            nodes[sel_node].repeat_interval_ms != es->pulse_period_ms)
                        {
                            char s2[96];
                            snprintf(s2, sizeof s2, "Use spec pulse period (%.0f ms)",
                                     es->pulse_period_ms);
                            if (overlay_button(s2))
                            {
                                nodes[sel_node].repeat_interval_ms = es->pulse_period_ms;
                                /* If using window mode, optionally derive an integer repeat_count
                                 * suggestion */
                                if (nodes[sel_node].repeat_count > 0 &&
                                    nodes[sel_node].duration_ms > 0.0f)
                                {
                                    int rc =
                                        (int) (nodes[sel_node].duration_ms / es->pulse_period_ms);
                                    if (rc < 0)
                                        rc = 0;
                                    nodes[sel_node].repeat_count = rc;
                                }
                                changed = 1;
                            }
                        }
                        /* Helpful one-click fixes for common invalid combos */
                        if (nodes[sel_node].repeat_count > 0 &&
                            nodes[sel_node].repeat_interval_ms <= 0.0f)
                        {
                            if (overlay_button("Fix: Set interval 1000ms (repeat_count > 0)"))
                            {
                                nodes[sel_node].repeat_interval_ms = 1000.0f;
                                changed = 1;
                            }
                        }
                        if (nodes[sel_node].repeat_count == 0 &&
                            nodes[sel_node].duration_ms > 0.0f &&
                            nodes[sel_node].repeat_interval_ms <= 0.0f)
                        {
                            if (overlay_button("Fix: Window mode — set interval 1000ms"))
                            {
                                nodes[sel_node].repeat_interval_ms = 1000.0f;
                                changed = 1;
                            }
                        }
                    }
                }
            }
            /* Batch helpers for nodes */
            overlay_label("Batch Helpers:");
            if (overlay_button("Fill empty nodes with Primary EffectSpec"))
            {
                for (int i = 0; i < node_count && i < 3; ++i)
                {
                    if (nodes[i].effect_spec_id <= 0 && primary_id > 0)
                    {
                        nodes[i].effect_spec_id = primary_id;
                    }
                }
                changed = 1;
            }
            if (overlay_button("Normalize window durations to whole pulses"))
            {
                for (int i = 0; i < node_count && i < 3; ++i)
                {
                    if (nodes[i].repeat_count == 0 && nodes[i].repeat_interval_ms > 0.0f &&
                        nodes[i].duration_ms > 0.0f)
                    {
                        int pulses = (int) (nodes[i].duration_ms / nodes[i].repeat_interval_ms);
                        if (pulses < 1)
                            pulses = 1;
                        nodes[i].duration_ms = pulses * nodes[i].repeat_interval_ms;
                    }
                }
                changed = 1;
            }
            if (overlay_button("Clear all HP gates"))
            {
                for (int i = 0; i < node_count && i < 3; ++i)
                    nodes[i].require_player_health_below_pct = 0;
                changed = 1;
            }
            if (overlay_button("Chain Nodes (set delays from order)"))
            {
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
                    float add = nodes[idx].duration_ms;
                    if (nodes[idx].repeat_count > 0 && nodes[idx].repeat_interval_ms > 0.0f)
                    {
                        add = nodes[idx].repeat_count * nodes[idx].repeat_interval_ms;
                    }
                    t_ms += add;
                }
                changed = 1;
            }
            if (overlay_button("Apply Connections to Delays"))
            {
                for (int i = 0; i < node_count && i < 3; ++i)
                {
                    if (nodes[i].effect_spec_id <= 0)
                        continue;
                    if (parent_of[i] == -2)
                        continue;
                    float t_ms = 0.0f;
                    int v = i;
                    int guard = 0;
                    while (parent_of[v] != -2 && guard++ < 8)
                    {
                        int p = parent_of[v];
                        if (p == -1)
                            break;
                        if (p < 0 || p >= 3)
                            break;
                        float add = nodes[p].duration_ms;
                        if (nodes[p].repeat_count > 0 && nodes[p].repeat_interval_ms > 0.0f)
                            add = nodes[p].repeat_count * nodes[p].repeat_interval_ms;
                        t_ms += add;
                        v = p;
                    }
                    nodes[i].delay_ms = t_ms;
                }
                changed = 1;
            }
        }
    }
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
        changed |= overlay_slider_float("  Duration (ms)", &nodes[i].duration_ms, 0.0f, 60000.0f);
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
        (void) rogue_skill_debug_save_overrides(overrides_path);
        panel_skills_refresh_validation();
    }
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
