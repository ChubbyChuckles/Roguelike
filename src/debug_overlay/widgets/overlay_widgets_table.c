#include "overlay_widgets_internal.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

#include "../../core/app/app_state.h"
#include "../overlay_theme.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

int overlay_table_begin(const char* id, const char* const* headers, int col_count, int* sort_col,
                        int* sort_dir, const char* filter_text)
{
    (void) id;
    (void) filter_text;
    if (!g_ui.panel_active || !headers || col_count <= 0)
        return 0;
    int widths[4] = {0, 0, 0, 0};
    if (col_count > 4)
        col_count = 4;
    for (int i = 0; i < col_count; ++i)
        widths[i] = (g_ui.width / col_count) - 2;
    overlay_columns_begin(col_count, widths);
    g_ui.table_active = 1;
    g_ui.table_cols = col_count;
    g_ui.table_row_h = 18;
    g_ui.table_hovered = 0;
    for (int c = 0; c < col_count; ++c)
    {
        int clicked = overlay_button(headers[c] ? headers[c] : "");
        if (clicked && sort_col && sort_dir)
        {
            if (*sort_col == c)
                *sort_dir = (*sort_dir >= 0) ? -1 : 1;
            else
            {
                *sort_col = c;
                *sort_dir = 1;
            }
        }
    }
    overlay_columns_end();
    /* Consider header area hovered if mouse in the last header row bounds */
    {
        int row_x = g_ui.cur_x - 8;
        int row_y = g_ui.cur_y - g_ui.line_h + 2;
        int row_w = g_ui.width;
        int row_h = g_ui.table_row_h;
        if (overlay_mouse_over(row_x, row_y, row_w, row_h))
            g_ui.table_hovered = 1;
    }
    return 1;
}

int overlay_table_row(const char* const* cells, int col_count, int row_index, int* selected_row)
{
    if (!g_ui.panel_active || !g_ui.table_active || !cells || col_count <= 0)
        return 0;
    if (col_count > g_ui.table_cols)
        col_count = g_ui.table_cols;
    int changed = 0;
    int widths[4] = {0, 0, 0, 0};
    for (int i = 0; i < col_count; ++i)
        widths[i] = (g_ui.width / col_count) - 2;
    int row_x = g_ui.cur_x - 8;
    int row_y = g_ui.cur_y + 2;
    int row_w = g_ui.width;
    int row_h = g_ui.table_row_h;
    if (overlay_columns_begin(col_count, widths))
    {
#ifdef ROGUE_HAVE_SDL
        if (g_app.renderer)
        {
            const OverlayTheme* th = overlay_theme_get();
            SDL_Rect rr = {row_x, row_y, row_w, row_h};
            int sel = (selected_row && *selected_row == row_index);
            OverlayColor bg = sel ? th->table_row_bg_sel : th->table_row_bg;
            SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(g_app.renderer, &rr);
            SDL_SetRenderDrawColor(g_app.renderer, th->table_border.r, th->table_border.g,
                                   th->table_border.b, th->table_border.a);
            SDL_RenderDrawRect(g_app.renderer, &rr);
        }
#endif
        for (int c = 0; c < col_count; ++c)
        {
            const char* t = cells[c] ? cells[c] : "";
            /* overlay_label already uses theme text color */
            overlay_label(t);
        }
        overlay_columns_end();
    }
    const OverlayInputState* in = overlay_input_get();
    int hover = overlay_mouse_over(row_x, row_y, row_w, row_h);
    if (hover)
        g_ui.table_hovered = 1;
    if (hover && in->mouse_clicked && selected_row)
    {
        if (*selected_row != row_index)
        {
            *selected_row = row_index;
            changed = 1;
        }
        overlay_input_set_capture(1, 1);
    }
    return changed;
}

void overlay_table_end(void)
{
    if (!g_ui.panel_active || !g_ui.table_active)
        return;
    ui_next_line();
    g_ui.table_active = 0;
    g_ui.table_cols = 0;
}

/* Helper to query table hover and wheel delta for scrolling lists */
int overlay_table_hover_wheel(int* out_wheel_y)
{
    if (!g_ui.panel_active)
        return 0;
    const OverlayInputState* in = overlay_input_get();
    if (out_wheel_y)
        *out_wheel_y = in ? in->mouse_wheel_y : 0;
    return g_ui.table_hovered;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
