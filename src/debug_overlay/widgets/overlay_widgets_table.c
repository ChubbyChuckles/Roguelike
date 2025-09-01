#include "overlay_widgets_internal.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

#include "../../core/app/app_state.h"
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
            SDL_Rect rr = {row_x, row_y, row_w, row_h};
            int sel = (selected_row && *selected_row == row_index);
            SDL_SetRenderDrawColor(g_app.renderer, sel ? 60 : 20, sel ? 80 : 20, sel ? 120 : 20,
                                   180);
            SDL_RenderFillRect(g_app.renderer, &rr);
            SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
            SDL_RenderDrawRect(g_app.renderer, &rr);
        }
#endif
        for (int c = 0; c < col_count; ++c)
            overlay_label(cells[c] ? cells[c] : "");
        overlay_columns_end();
    }
    const OverlayInputState* in = overlay_input_get();
    int hover = overlay_mouse_over(row_x, row_y, row_w, row_h);
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

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
