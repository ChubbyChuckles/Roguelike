#include "effects_node_graph.h"
#include "../../../core/app/app_state.h"
#include "../../../graphics/effect_spec.h"
#include "../../overlay_core.h"
#include "../../overlay_theme.h"
#include "../../widgets/overlay_widgets.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* Extracted legacy mini graph editor (simplified from original monolith). */
int effects_node_graph_editor_draw(int skill_index, int* primary_id,
                                   struct RogueSkillEffectNode* nodes, int node_count)
{
    (void) skill_index; /* currently unused but kept for future enhancements */
    int changed = 0;
    static int graph_enabled = 1;
    overlay_checkbox("Enable Node Graph Editor", &graph_enabled);
    if (!graph_enabled)
        return 0;

    typedef struct NodeUI
    {
        int id;
        int x, y;
    } NodeUI;
    static int last_skill = -1; /* not strictly needed now; placeholder for future */
    static NodeUI ui_primary = {-1, 0, 0};
    static NodeUI ui_nodes[3];
    static int ui_inited = 0;
    static int dragging = 0;
    static int drag_dx = 0, drag_dy = 0;
    static int parent_of[3] = {-2, -2, -2};
    static int linking = 0;
    static int link_source = -2;

    const int panel_x = 380;
    const int panel_y = 10;
    const int cv_x = panel_x + 12;
    const int cv_y = panel_y + 410;
    const int cv_w = 396;
    const int cv_h = 160;

#ifdef ROGUE_HAVE_SDL
    if (!g_app.headless && g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect r = {cv_x, cv_y, cv_w, cv_h};
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r, th->panel_bg.g, th->panel_bg.b,
                               th->panel_bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                               th->panel_border.b, th->panel_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
    }
#endif

    if (last_skill != skill_index)
    {
        ui_inited = 0;
        last_skill = skill_index;
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
/* local hit test (C89 friendly) */
#define EFFECTS_NG_HIT(mx2, my2, bx, by, bw2, bh2)                                                 \
    ((mx2) >= (bx) && (mx2) < (bx) + (bw2) && (my2) >= (by) && (my2) < (by) + (bh2))

    if (mdown && !was_down)
    {
        if (EFFECTS_NG_HIT(mx, my, ui_primary.x, ui_primary.y, bw, bh))
        {
            dragging = -1;
            drag_dx = mx - ui_primary.x;
            drag_dy = my - ui_primary.y;
        }
        else
        {
            for (int i = 0; i < node_count && i < 3; ++i)
            {
                if (EFFECTS_NG_HIT(mx, my, ui_nodes[i].x, ui_nodes[i].y, bw, bh))
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
        dragging = 99;
    was_down = mdown;

#ifdef ROGUE_HAVE_SDL
    if (!g_app.headless && g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_SetRenderDrawColor(g_app.renderer, th->accent_1.r, th->accent_1.g, th->accent_1.b,
                               th->accent_1.a);
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
        SDL_SetRenderDrawColor(g_app.renderer, th->button_bg_hot.r, th->button_bg_hot.g,
                               th->button_bg_hot.b, th->button_bg_hot.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                               th->panel_border.b, th->panel_border.a);
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
                SDL_SetRenderDrawColor(g_app.renderer, th->toast_error_bg.r, th->toast_error_bg.g,
                                       th->toast_error_bg.b, th->toast_error_bg.a);
            else if (has_timing_issue)
                SDL_SetRenderDrawColor(g_app.renderer, th->toast_warn_bg.r, th->toast_warn_bg.g,
                                       th->toast_warn_bg.b, th->toast_warn_bg.a);
            else
                SDL_SetRenderDrawColor(g_app.renderer, th->toast_info_bg.r, th->toast_info_bg.g,
                                       th->toast_info_bg.b, th->toast_info_bg.a);
            SDL_RenderFillRect(g_app.renderer, &r);
            SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                                   th->panel_border.b, th->panel_border.a);
            SDL_RenderDrawRect(g_app.renderer, &r);
        }
    }
#endif

    static int sel_node = -1;
    if (!mdown)
    {
        if (EFFECTS_NG_HIT(mx, my, ui_primary.x, ui_primary.y, bw, bh))
            sel_node = -1;
        for (int i = 0; i < node_count && i < 3; ++i)
            if (EFFECTS_NG_HIT(mx, my, ui_nodes[i].x, ui_nodes[i].y, bw, bh))
                sel_node = i;
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
        if (nodes[i].effect_spec_id > 0 && parent_of[i] == -2)
            parent_of[i] = -1;

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
                    parent_of[tgt] = src;
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
        /* Advanced presets */
        overlay_label("Advanced Presets:");
        if (overlay_button("Periodic Window (Dur=5000, Rep=5 every 1000)"))
        {
            nodes[sel_node].duration_ms = 5000.0f;
            nodes[sel_node].repeat_count = 5;
            nodes[sel_node].repeat_interval_ms = 1000.0f;
            changed = 1;
        }
        if (overlay_button("Counted Pulses (Rep=10 every 500ms, Dur=0)"))
        {
            nodes[sel_node].duration_ms = 0.0f;
            nodes[sel_node].repeat_count = 10;
            nodes[sel_node].repeat_interval_ms = 500.0f;
            changed = 1;
        }
        if (overlay_button("Snapshot Chain (Dur=0, Rep=1)"))
        {
            nodes[sel_node].duration_ms = 0.0f;
            nodes[sel_node].repeat_count = 1;
            nodes[sel_node].repeat_interval_ms = 0.0f;
            changed = 1;
        }
    }
    if (sel_node == -1)
        changed |= overlay_slider_int("Primary EffectSpec ID", primary_id, -1, 4096);
    else if (sel_node >= 0 && sel_node < node_count)
    {
        changed |=
            overlay_slider_int("Node EffectSpec ID", &nodes[sel_node].effect_spec_id, -1, 4096);
        changed |=
            overlay_slider_float("Node Delay (ms)", &nodes[sel_node].delay_ms, 0.0f, 20000.0f);
        changed |= overlay_slider_float("Node Duration (ms)", &nodes[sel_node].duration_ms, 0.0f,
                                        120000.0f);
        changed |= overlay_slider_int("Node Repeat Count", &nodes[sel_node].repeat_count, 0, 128);
        changed |= overlay_slider_float("Node Repeat Interval (ms)",
                                        &nodes[sel_node].repeat_interval_ms, 0.0f, 20000.0f);
        int hp_gate2 = nodes[sel_node].require_player_health_below_pct;
        if (overlay_slider_int("Node HP Below % (gate)", &hp_gate2, 0, 100))
        {
            nodes[sel_node].require_player_health_below_pct = (unsigned char) hp_gate2;
            changed = 1;
        }
        overlay_label("Presets:");
        if (overlay_button("Instant (one-shot)"))
        {
            nodes[sel_node].duration_ms = 0.0f;
            nodes[sel_node].repeat_count = 1;
            nodes[sel_node].repeat_interval_ms = 0.0f;
            changed = 1;
        }
        if (overlay_button("Pulse 3x 250ms"))
        {
            nodes[sel_node].duration_ms = 0.0f;
            nodes[sel_node].repeat_count = 3;
            nodes[sel_node].repeat_interval_ms = 250.0f;
            changed = 1;
        }
        if (overlay_button("Sustain 8s (interval 0)"))
        {
            nodes[sel_node].duration_ms = 8000.0f;
            nodes[sel_node].repeat_count = 0;
            nodes[sel_node].repeat_interval_ms = 0.0f;
            changed = 1;
        }
    }

    overlay_label("Batch Helpers:");
    if (overlay_button("Fill empty nodes with Primary EffectSpec"))
    {
        for (int i = 0; i < node_count && i < 3; ++i)
            if (nodes[i].effect_spec_id <= 0 && *primary_id > 0)
                nodes[i].effect_spec_id = *primary_id;
        changed = 1;
    }
    if (overlay_button("Clear all HP gates"))
    {
        for (int i = 0; i < node_count && i < 3; ++i)
            nodes[i].require_player_health_below_pct = 0;
        changed = 1;
    }
    if (overlay_button(
            "Normalize repeat intervals (ensure interval if duration>0 and repeat_count==0)"))
    {
        for (int i = 0; i < node_count && i < 3; ++i)
        {
            if (nodes[i].effect_spec_id > 0 && nodes[i].duration_ms > 0.0f &&
                nodes[i].repeat_count == 0 && nodes[i].repeat_interval_ms <= 0.0f)
            {
                nodes[i].repeat_interval_ms = nodes[i].duration_ms;
                changed = 1;
            }
        }
    }
    if (overlay_button("Apply connection order to delays (cumulative 250ms)"))
    {
        /* simple BFS layering from primary to nodes via parent_of */
        int depth[3] = {0, 0, 0};
        for (int i = 0; i < 3; ++i)
        {
            int d = 0;
            int v = i;
            int guard = 0;
            while (v >= 0 && v < 3 && guard++ < 6)
            {
                int p = parent_of[v];
                if (p == -2)
                    break; /* no parent */
                if (p < 0)
                {
                    d++;
                    break; /* primary */
                }
                d++;
                v = p;
            }
            depth[i] = d;
        }
        for (int i = 0; i < node_count && i < 3; ++i)
        {
            if (nodes[i].effect_spec_id > 0)
            {
                nodes[i].delay_ms = (float) (depth[i] * 250.0f);
                changed = 1;
            }
        }
    }
    if (overlay_button("Suggest: set empty durations to sum(repeat_count*repeat_interval)"))
    {
        for (int i = 0; i < node_count && i < 3; ++i)
        {
            if (nodes[i].effect_spec_id > 0 && nodes[i].duration_ms <= 0.0f &&
                nodes[i].repeat_count > 0 && nodes[i].repeat_interval_ms > 0.0f)
            {
                nodes[i].duration_ms = nodes[i].repeat_count * nodes[i].repeat_interval_ms;
                changed = 1;
            }
        }
    }
    if (overlay_button("Clamp repeats >32 to 32"))
    {
        for (int i = 0; i < node_count && i < 3; ++i)
        {
            if (nodes[i].repeat_count > 32)
            {
                nodes[i].repeat_count = 32;
                changed = 1;
            }
        }
    }
    if (overlay_button("Zero negative durations/intervals"))
    {
        for (int i = 0; i < node_count && i < 3; ++i)
        {
            if (nodes[i].duration_ms < 0.0f)
            {
                nodes[i].duration_ms = 0.0f;
                changed = 1;
            }
            if (nodes[i].repeat_interval_ms < 0.0f)
            {
                nodes[i].repeat_interval_ms = 0.0f;
                changed = 1;
            }
        }
    }
    if (overlay_button("Clear all nodes"))
    {
        for (int i = 0; i < node_count && i < 3; ++i)
        {
            nodes[i].effect_spec_id = -1;
            nodes[i].delay_ms = 0.0f;
            nodes[i].duration_ms = 0.0f;
            nodes[i].repeat_count = 0;
            nodes[i].repeat_interval_ms = 0.0f;
            nodes[i].require_player_health_below_pct = 0;
            parent_of[i] = -2;
        }
        changed = 1;
    }
    if (overlay_button("Copy primary id into all valid nodes (non -1)"))
    {
        if (*primary_id > 0)
        {
            for (int i = 0; i < node_count && i < 3; ++i)
                if (nodes[i].effect_spec_id > 0)
                    nodes[i].effect_spec_id = *primary_id;
            changed = 1;
        }
    }
    if (overlay_button("Set HP gate=0 where >100 (safety)"))
    {
        for (int i = 0; i < node_count && i < 3; ++i)
        {
            if (nodes[i].require_player_health_below_pct > 100)
            {
                nodes[i].require_player_health_below_pct = 0;
                changed = 1;
            }
        }
    }
#undef EFFECTS_NG_HIT
    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
