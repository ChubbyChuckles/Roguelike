#include "effects_tree_editor.h"
#include "../../../core/app/app_state.h"
#include "../../../graphics/effect_spec.h"
#include "../../overlay_core.h"
#include "../../overlay_theme.h"
#include "../../widgets/overlay_widgets.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <string.h>

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* Internal helpers (local linkage) */
static void tree_logical_to_screen_fn(int lx, int ly, int* sx, int* sy, int cv_x_, int cv_y_,
                                      int pan_x_, int pan_y_, float zoom_)
{
    int rel_x = lx - cv_x_;
    int rel_y = ly - cv_y_;
    *sx = cv_x_ + pan_x_ + (int) (rel_x * zoom_);
    *sy = cv_y_ + pan_y_ + (int) (rel_y * zoom_);
}
static void tree_screen_to_logical_origin_fn(int screen_x, int screen_y, int drag_dx_s,
                                             int drag_dy_s, int* lx, int* ly, int cv_x_, int cv_y_,
                                             int pan_x_, int pan_y_, float zoom_)
{
    int adj_x = screen_x - drag_dx_s - cv_x_ - pan_x_;
    int adj_y = screen_y - drag_dy_s - cv_y_ - pan_y_;
    if (zoom_ != 0.0f)
    {
        adj_x = (int) (adj_x / zoom_);
        adj_y = (int) (adj_y / zoom_);
    }
    *lx = cv_x_ + adj_x;
    *ly = cv_y_ + adj_y;
}
static int overlay_box_hit_local(int mx, int my, int bx, int by, int bw, int bh)
{
    return (mx >= bx && mx < bx + bw && my >= by && my < by + bh) ? 1 : 0;
}

int effects_tree_editor_draw(int skill_index, const char* overrides_path,
                             struct RogueSkillEffectTreeNodeDebug* tree_nodes, int* tree_count)
{
    int changed = 0;
    overlay_label("Tree Editor");
    int add_remove = *tree_count;
    if (overlay_slider_int("Tree Node Count (1..8)", &add_remove, 0, 8))
    {
        if (add_remove < 0)
            add_remove = 0;
        if (add_remove > 8)
            add_remove = 8;
        if (add_remove > *tree_count)
        {
            for (int i = *tree_count; i < add_remove; ++i)
            {
                tree_nodes[i].effect_spec_id = -1;
                tree_nodes[i].delay_ms = 0.0f;
                tree_nodes[i].duration_ms = 0.0f;
                tree_nodes[i].repeat_count = 1;
                tree_nodes[i].repeat_interval_ms = 0.0f;
                tree_nodes[i].require_player_health_below_pct = 0;
                tree_nodes[i].parent_index = -1;
            }
        }
        *tree_count = add_remove;
        changed = 1;
    }
    for (int ti = 0; ti < *tree_count; ++ti)
    {
        char hdr[64];
        snprintf(hdr, sizeof hdr, "Tree Node %d", ti);
        overlay_label(hdr);
        changed |= overlay_slider_int("  EffectSpec ID", &tree_nodes[ti].effect_spec_id, -1, 4096);
        changed |= overlay_slider_float("  Delay (ms)", &tree_nodes[ti].delay_ms, 0.0f, 10000.0f);
        changed |=
            overlay_slider_float("  Duration (ms)", &tree_nodes[ti].duration_ms, 0.0f, 60000.0f);
        changed |= overlay_slider_int("  Repeat Count", &tree_nodes[ti].repeat_count, 0, 100);
        changed |= overlay_slider_float("  Repeat Interval (ms)",
                                        &tree_nodes[ti].repeat_interval_ms, 0.0f, 10000.0f);
        int hp_gate_t = tree_nodes[ti].require_player_health_below_pct;
        if (overlay_slider_int("  HP Below % (gate)", &hp_gate_t, 0, 100))
        {
            tree_nodes[ti].require_player_health_below_pct = (unsigned char) hp_gate_t;
            changed = 1;
        }
        int parent_idx = (int) tree_nodes[ti].parent_index;
        if (overlay_slider_int("  Parent Index (-1 root)", &parent_idx, -1, (*tree_count) - 1))
        {
            tree_nodes[ti].parent_index = (signed char) parent_idx;
            changed = 1;
        }
    }

    overlay_label("Tree Visualization (draggable, experimental)");
    static int tree_orientation = 0;
    static int tree_layout_loaded = 0;
    typedef struct TreeNodeUIPos
    {
        int x, y;
    } TreeNodeUIPos;
    static TreeNodeUIPos tree_ui_pos[8];
    if (!tree_layout_loaded)
    {
        int lo = 0;
        int xs[8], ys[8];
        if (rogue_skill_debug_get_effect_tree_layout(skill_index, &lo, xs, ys, 8) == 0)
        {
            if (*tree_count > 0)
            {
                tree_orientation = lo;
                for (int i = 0; i < *tree_count && i < 8; ++i)
                {
                    if (xs[i] != 0 || ys[i] != 0)
                    {
                        tree_ui_pos[i].x = xs[i];
                        tree_ui_pos[i].y = ys[i];
                    }
                }
            }
        }
        tree_layout_loaded = 1;
    }
    (void) overlay_slider_int("Orientation (0 L->R,1 R->L)", &tree_orientation, 0, 1);
    int auto_arrange_request = overlay_button("Auto Arrange Tree") ? 1 : 0;
    int persist_layout_request = overlay_button("Save Layout") ? 1 : 0;

    static float tree_zoom = 1.0f;
    static int tree_pan_x = 0, tree_pan_y = 0;
    if (overlay_slider_float("Zoom (0.5..2.0)", &tree_zoom, 0.5f, 2.0f))
    {
        if (tree_zoom < 0.5f)
            tree_zoom = 0.5f;
        if (tree_zoom > 2.0f)
            tree_zoom = 2.0f;
    }
    overlay_slider_int("Pan X", &tree_pan_x, -400, 400);
    overlay_slider_int("Pan Y", &tree_pan_y, -400, 400);
    if (overlay_button("Reset View"))
    {
        tree_zoom = 1.0f;
        tree_pan_x = 0;
        tree_pan_y = 0;
    }

    static int tree_last_skill = -1;
    static int tree_last_count = 0;
    static int tree_dragging = -1;
    static int tree_drag_dx = 0, tree_drag_dy = 0;
    static int tree_selected = -1;
    static int tree_linking = 0;
    static int tree_link_source = -1;
    static unsigned int tree_selection_mask = 0;
    static int tree_multi_select_mode = 0;
    static int tree_multi_drag = 0;
    static int tree_drag_start_recorded = 0;
    typedef struct TreeDragStartPos
    {
        int x;
        int y;
    } TreeDragStartPos;
    static TreeDragStartPos tree_drag_start_pos[8];

    const int cv_x = 360, cv_y = 420, cv_w = 420, cv_h = 160;
    const int node_w = 72, node_h = 26;
    if (tree_last_skill != skill_index || tree_last_count != *tree_count || auto_arrange_request)
    {
        tree_last_skill = skill_index;
        tree_last_count = *tree_count;
        int depth[8];
        for (int i = 0; i < *tree_count; ++i)
        {
            int d = 0, v = i, guard = 0;
            while (v >= 0 && v < *tree_count && guard++ < 8)
            {
                int p = tree_nodes[v].parent_index;
                if (p < 0)
                    break;
                ++d;
                v = p;
            }
            depth[i] = d;
        }
        int col_w = 96;
        int level_counts[8] = {0};
        for (int i = 0; i < *tree_count; ++i)
        {
            int d = depth[i];
            if (d < 0)
                d = 0;
            if (d > 7)
                d = 7;
            int row = level_counts[d]++;
            if (tree_orientation == 0)
                tree_ui_pos[i].x = cv_x + 10 + d * col_w;
            else
                tree_ui_pos[i].x = cv_x + cv_w - 10 - node_w - d * col_w;
            tree_ui_pos[i].y = cv_y + 10 + row * 34;
        }
        tree_dragging = -1;
        tree_selected = -1;
        tree_linking = 0;
        tree_link_source = -1;
    }

    int mx = 0, my = 0;
    int mdown = 0;
    static int tree_was_down = 0;
#ifdef ROGUE_HAVE_SDL
    if (!g_app.headless)
    {
        Uint32 mask = SDL_GetMouseState(&mx, &my);
        mdown = (mask & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    }
#endif
    if (mdown && !tree_was_down)
    {
        for (int i = *tree_count - 1; i >= 0; --i)
        {
            int sx, sy;
            tree_logical_to_screen_fn(tree_ui_pos[i].x, tree_ui_pos[i].y, &sx, &sy, cv_x, cv_y,
                                      tree_pan_x, tree_pan_y, tree_zoom);
            int sw = (int) (node_w * tree_zoom), sh = (int) (node_h * tree_zoom);
            if (overlay_box_hit_local(mx, my, sx, sy, sw, sh))
            {
                tree_dragging = i;
                tree_drag_dx = mx - sx;
                tree_drag_dy = my - sy;
                if (tree_multi_select_mode)
                {
                    unsigned int bit = 1u << i;
                    if ((tree_selection_mask & bit) == 0)
                        tree_selection_mask |= bit;
                    else
                        tree_selection_mask &= ~bit;
                    tree_selected = i;
                }
                else
                {
                    tree_selected = i;
                    tree_selection_mask = (1u << i);
                }
                tree_multi_drag = (tree_multi_select_mode && tree_selection_mask != 0 &&
                                   (tree_selection_mask & (1u << i)) != 0);
                tree_drag_start_recorded = 0;
                break;
            }
        }
    }
    if (mdown && tree_dragging >= 0)
    {
        if (!tree_drag_start_recorded && tree_multi_drag)
        {
            for (int i = 0; i < *tree_count; ++i)
            {
                tree_drag_start_pos[i].x = tree_ui_pos[i].x;
                tree_drag_start_pos[i].y = tree_ui_pos[i].y;
            }
            tree_drag_start_recorded = 1;
        }
        int lx, ly;
        tree_screen_to_logical_origin_fn(mx, my, tree_drag_dx, tree_drag_dy, &lx, &ly, cv_x, cv_y,
                                         tree_pan_x, tree_pan_y, tree_zoom);
        if (lx < cv_x)
            lx = cv_x;
        if (ly < cv_y)
            ly = cv_y;
        if (lx > cv_x + cv_w - node_w)
            lx = cv_x + cv_w - node_w;
        if (ly > cv_y + cv_h - node_h)
            ly = cv_y + cv_h - node_h;
        int ox = tree_ui_pos[tree_dragging].x, oy = tree_ui_pos[tree_dragging].y;
        tree_ui_pos[tree_dragging].x = lx;
        tree_ui_pos[tree_dragging].y = ly;
        if (tree_multi_drag && tree_drag_start_recorded)
        {
            int dx = tree_ui_pos[tree_dragging].x - ox;
            int dy = tree_ui_pos[tree_dragging].y - oy;
            for (int i = 0; i < *tree_count; ++i)
            {
                if (i == tree_dragging)
                    continue;
                if ((tree_selection_mask & (1u << i)) == 0)
                    continue;
                int nx = tree_ui_pos[i].x + dx;
                int ny = tree_ui_pos[i].y + dy;
                if (nx < cv_x)
                    nx = cv_x;
                if (ny < cv_y)
                    ny = cv_y;
                if (nx > cv_x + cv_w - node_w)
                    nx = cv_x + cv_w - node_w;
                if (ny > cv_y + cv_h - node_h)
                    ny = cv_y + cv_h - node_h;
                tree_ui_pos[i].x = nx;
                tree_ui_pos[i].y = ny;
            }
        }
    }
    if (!mdown && tree_was_down)
    {
        if (tree_dragging >= 0)
        {
            int released_idx = tree_dragging;
            if (tree_ui_pos[released_idx].x < cv_x + 40 &&
                tree_nodes[released_idx].parent_index != -1)
            {
                tree_nodes[released_idx].parent_index = -1;
                changed = 1;
            }
            if (tree_linking)
            {
                for (int i = 0; i < *tree_count; ++i)
                {
                    if (i == tree_link_source)
                        continue;
                    if (overlay_box_hit_local(mx, my, tree_ui_pos[i].x, tree_ui_pos[i].y, node_w,
                                              node_h))
                    {
                        int cycle = 0;
                        int v = tree_link_source;
                        int guard = 0;
                        while (v >= 0 && v < *tree_count && guard++ < 16)
                        {
                            if (v == i)
                            {
                                cycle = 1;
                                break;
                            }
                            int p = tree_nodes[v].parent_index;
                            if (p < 0)
                                break;
                            v = p;
                        }
                        if (!cycle)
                        {
                            tree_nodes[i].parent_index = (signed char) tree_link_source;
                            changed = 1;
                        }
                        break;
                    }
                }
                tree_linking = 0;
                tree_link_source = -1;
            }
            else if (!tree_multi_select_mode)
            {
                tree_selected = released_idx;
                tree_selection_mask = (1u << released_idx);
            }
        }
        else if (tree_linking)
        {
            tree_linking = 0;
            tree_link_source = -1;
        }
        tree_dragging = -1;
        tree_multi_drag = 0;
    }
    tree_was_down = mdown;

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
        for (int i = 0; i < *tree_count; ++i)
        {
            int p = tree_nodes[i].parent_index;
            if (p >= 0 && p < *tree_count)
            {
                SDL_SetRenderDrawColor(g_app.renderer, th->accent_1.r, th->accent_1.g,
                                       th->accent_1.b, th->accent_1.a);
                int sx1, sy1, sx2, sy2;
                int x1l = tree_orientation == 0 ? (tree_ui_pos[p].x + node_w) : tree_ui_pos[p].x;
                int x2l = tree_orientation == 0 ? tree_ui_pos[i].x : (tree_ui_pos[i].x + node_w);
                tree_logical_to_screen_fn(x1l, tree_ui_pos[p].y, &sx1, &sy1, cv_x, cv_y, tree_pan_x,
                                          tree_pan_y, tree_zoom);
                tree_logical_to_screen_fn(x2l, tree_ui_pos[i].y, &sx2, &sy2, cv_x, cv_y, tree_pan_x,
                                          tree_pan_y, tree_zoom);
                SDL_RenderDrawLine(g_app.renderer, sx1, sy1 + (int) (node_h * tree_zoom) / 2, sx2,
                                   sy2 + (int) (node_h * tree_zoom) / 2);
            }
        }
        if (tree_linking && tree_link_source >= 0 && tree_link_source < *tree_count)
        {
            SDL_SetRenderDrawColor(g_app.renderer, th->accent_2.r, th->accent_2.g, th->accent_2.b,
                                   th->accent_2.a);
            int lx = tree_orientation == 0 ? (tree_ui_pos[tree_link_source].x + node_w)
                                           : tree_ui_pos[tree_link_source].x;
            int sx, sy;
            tree_logical_to_screen_fn(lx, tree_ui_pos[tree_link_source].y, &sx, &sy, cv_x, cv_y,
                                      tree_pan_x, tree_pan_y, tree_zoom);
            SDL_RenderDrawLine(g_app.renderer, sx, sy + (int) (node_h * tree_zoom) / 2, mx, my);
        }
        for (int i = 0; i < *tree_count; ++i)
        {
            int sx, sy;
            tree_logical_to_screen_fn(tree_ui_pos[i].x, tree_ui_pos[i].y, &sx, &sy, cv_x, cv_y,
                                      tree_pan_x, tree_pan_y, tree_zoom);
            SDL_Rect nr = {sx, sy, (int) (node_w * tree_zoom), (int) (node_h * tree_zoom)};
            int invalid = (tree_nodes[i].effect_spec_id > 0 &&
                           !rogue_effect_get(tree_nodes[i].effect_spec_id));
            int is_selected = (tree_selection_mask & (1u << i)) != 0;
            SDL_Color fill;
            if (invalid)
            {
                fill.r = th->toast_error_bg.r;
                fill.g = th->toast_error_bg.g;
                fill.b = th->toast_error_bg.b;
                fill.a = th->toast_error_bg.a;
            }
            else
            {
                fill.r = th->toast_info_bg.r;
                fill.g = th->toast_info_bg.g;
                fill.b = th->toast_info_bg.b;
                fill.a = th->toast_info_bg.a;
            }
            if (is_selected)
            {
                if (fill.r + 30 < 255)
                    fill.r = (Uint8) (fill.r + 30);
                if (fill.g + 30 < 255)
                    fill.g = (Uint8) (fill.g + 30);
                if (fill.b + 30 < 255)
                    fill.b = (Uint8) (fill.b + 30);
            }
            SDL_SetRenderDrawColor(g_app.renderer, fill.r, fill.g, fill.b, fill.a);
            SDL_RenderFillRect(g_app.renderer, &nr);
            SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                                   th->panel_border.b, th->panel_border.a);
            SDL_RenderDrawRect(g_app.renderer, &nr);
        }
    }
#endif

    if (tree_selected >= 0 && tree_selected < *tree_count)
    {
        char lab[64];
        snprintf(lab, sizeof lab, "Selected Tree Node %d", tree_selected);
        overlay_label(lab);
        if (overlay_button("Make Selected Root"))
        {
            if (tree_nodes[tree_selected].parent_index != -1)
            {
                tree_nodes[tree_selected].parent_index = -1;
                changed = 1;
            }
        }
        if (tree_nodes[tree_selected].parent_index >= 0 &&
            tree_nodes[tree_selected].parent_index < *tree_count)
        {
            int parent_idx = tree_nodes[tree_selected].parent_index;
            if (overlay_button("Reverse Link (Selected <-> Parent)"))
            {
                int grand_parent = tree_nodes[parent_idx].parent_index;
                tree_nodes[parent_idx].parent_index = (signed char) tree_selected;
                tree_nodes[tree_selected].parent_index = (signed char) grand_parent;
                changed = 1;
            }
        }
        if (!tree_linking)
        {
            if (overlay_button("Start Parent Link From Selected"))
            {
                tree_linking = 1;
                tree_link_source = tree_selected;
            }
        }
        else if (tree_linking && tree_link_source == tree_selected)
        {
            overlay_label("Link mode: click another node to set it as CHILD (target's parent "
                          "becomes selected)");
            if (overlay_button("Cancel Link"))
            {
                tree_linking = 0;
                tree_link_source = -1;
            }
        }
        overlay_checkbox("Multi-Select Mode", &tree_multi_select_mode);
        if (overlay_button("Clear Selection"))
        {
            tree_selection_mask = 0;
            tree_selected = -1;
        }
        if (tree_selected >= 0 && *tree_count < 8)
        {
            if (overlay_button("Add Child Node"))
            {
                if (*tree_count < 8)
                {
                    int ni = *tree_count;
                    tree_nodes[ni].effect_spec_id = -1;
                    tree_nodes[ni].delay_ms = 0.0f;
                    tree_nodes[ni].duration_ms = 0.0f;
                    tree_nodes[ni].repeat_count = 1;
                    tree_nodes[ni].repeat_interval_ms = 0.0f;
                    tree_nodes[ni].require_player_health_below_pct = 0;
                    tree_nodes[ni].parent_index = (signed char) tree_selected;
                    tree_ui_pos[ni].x = tree_ui_pos[tree_selected].x + 80;
                    tree_ui_pos[ni].y = tree_ui_pos[tree_selected].y + 40;
                    (*tree_count)++;
                    changed = 1;
                }
            }
        }
        if (tree_selection_mask != 0)
        {
            if (overlay_button("Delete Selected Leaf Nodes"))
            {
                int child_count[8] = {0};
                for (int i = 0; i < *tree_count; ++i)
                {
                    int p = tree_nodes[i].parent_index;
                    if (p >= 0 && p < *tree_count)
                        child_count[p]++;
                }
                int keep_map[8];
                int new_idx = 0;
                for (int i = 0; i < *tree_count; ++i)
                {
                    int selected = (tree_selection_mask & (1u << i)) != 0;
                    int deletable = selected && child_count[i] == 0;
                    if (!deletable)
                        keep_map[i] = new_idx++;
                    else
                        keep_map[i] = -1;
                }
                if (new_idx != *tree_count)
                {
                    struct RogueSkillEffectTreeNodeDebug tmp_nodes[8];
                    TreeNodeUIPos tmp_pos[8];
                    for (int i = 0; i < *tree_count; ++i)
                    {
                        if (keep_map[i] >= 0)
                        {
                            tmp_nodes[keep_map[i]] = tree_nodes[i];
                            tmp_pos[keep_map[i]] = tree_ui_pos[i];
                        }
                    }
                    for (int i = 0; i < new_idx; ++i)
                    {
                        tree_nodes[i] = tmp_nodes[i];
                        tree_ui_pos[i] = tmp_pos[i];
                    }
                    for (int i = 0; i < new_idx; ++i)
                    {
                        int p = tree_nodes[i].parent_index;
                        if (p >= 0 && p < *tree_count)
                        {
                            int np = keep_map[p];
                            tree_nodes[i].parent_index = (np < 0) ? -1 : (signed char) np;
                        }
                    }
                    *tree_count = new_idx;
                    tree_selection_mask = 0;
                    tree_selected = -1;
                    changed = 1;
                }
            }
        }
        if (overlay_button("Derive Child Delays From Parents"))
        {
            for (int i = 0; i < *tree_count; ++i)
            {
                float t_ms = 0.0f;
                int v = i;
                int guard = 0;
                while (v >= 0 && v < *tree_count && guard++ < 16)
                {
                    int p = tree_nodes[v].parent_index;
                    if (p < 0 || p >= *tree_count)
                        break;
                    float span = tree_nodes[p].duration_ms;
                    if (tree_nodes[p].repeat_count > 0 && tree_nodes[p].repeat_interval_ms > 0.0f)
                        span = tree_nodes[p].repeat_count * tree_nodes[p].repeat_interval_ms;
                    t_ms += span;
                    v = p;
                }
                tree_nodes[i].delay_ms = t_ms;
            }
            changed = 1;
        }
    }
    else
    {
        overlay_label("No Tree Node Selected");
        overlay_checkbox("Multi-Select Mode", &tree_multi_select_mode);
        if (!tree_multi_select_mode)
            tree_selection_mask = 0;
    }

    if (persist_layout_request && *tree_count > 0)
    {
        int xs[8], ys[8];
        for (int i = 0; i < *tree_count; ++i)
        {
            xs[i] = tree_ui_pos[i].x;
            ys[i] = tree_ui_pos[i].y;
        }
        (void) rogue_skill_debug_set_effect_tree_layout(skill_index, tree_orientation, xs, ys,
                                                        *tree_count);
        (void) rogue_skill_debug_save_overrides(overrides_path);
    }

    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
