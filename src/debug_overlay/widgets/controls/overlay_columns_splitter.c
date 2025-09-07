/* Reformatted columns & splitter controls */
#include "../../overlay_input.h"
#include "../../overlay_prefs.h"
#include "../../overlay_theme.h"
#include "../overlay_widgets_internal.h"
#include "controls_shared.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../../../core/app/app_state.h"

int overlay_columns_begin(int cols, const int* widths)
{
    if (!g_ui.panel_active)
        return 0;
    if (cols < 1)
        cols = 1;
    if (cols > 4)
        cols = 4;
    g_ui.columns = cols;
    g_ui.col_index = 0;
    g_ui.row_start_y = g_ui.cur_y;
    g_ui.row_max_h = g_ui.line_h;
    int remaining = g_ui.width;
    for (int i = 0; i < cols; ++i)
    {
        int w = widths ? widths[i] : (g_ui.width / cols);
        if (i == cols - 1)
            w = remaining;
        g_ui.col_widths[i] = w;
        remaining -= w;
    }
    g_ui.col_x0[0] = g_ui.cur_x;
    for (int i = 1; i < cols; ++i)
        g_ui.col_x0[i] = g_ui.col_x0[i - 1] + g_ui.col_widths[i - 1] + 8;
    g_ui.cur_x = g_ui.col_x0[0];
    g_ui.cur_y = g_ui.row_start_y;
    return 1;
}

void overlay_next_column(void)
{
    if (!g_ui.panel_active || g_ui.columns <= 1)
        return;
    g_ui.col_index++;
    if (g_ui.col_index >= g_ui.columns)
    {
        g_ui.col_index = 0;
        g_ui.row_start_y += (g_ui.row_max_h > 0 ? g_ui.row_max_h : g_ui.line_h);
        g_ui.row_max_h = g_ui.line_h;
    }
    g_ui.cur_x = g_ui.col_x0[g_ui.col_index];
    g_ui.cur_y = g_ui.row_start_y;
}

void overlay_columns_end(void)
{
    if (!g_ui.panel_active)
        return;
    g_ui.cur_x = g_ui.col_x0[0];
    g_ui.row_start_y += (g_ui.row_max_h > 0 ? g_ui.row_max_h : g_ui.line_h);
    g_ui.cur_y = g_ui.row_start_y;
    g_ui.columns = 1;
    g_ui.col_index = 0;
    g_ui.row_max_h = g_ui.line_h;
}

int overlay_splitter_begin(const char* id, int* out_left_w, int min_left_w, int max_left_w)
{
    if (!g_ui.panel_active || !id || !out_left_w)
        return 0;
    int pref = overlay_prefs_get_int(id, *out_left_w > 0 ? *out_left_w : g_ui.width / 2);
    if (pref < min_left_w)
        pref = min_left_w;
    if (pref > max_left_w)
        pref = max_left_w;
    int left_w = pref;
    int handle_x = g_ui.cur_x + left_w;
    int handle_w = 6;
    int row_y = g_ui.cur_y;
    int row_h = g_ui.line_h * 20; /* big vertical span for drag */
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        SDL_Rect bar = {handle_x - handle_w / 2, row_y, handle_w, row_h};
        const OverlayTheme* th = overlay_theme_get();
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                               th->panel_border.b, th->panel_border.a);
        SDL_RenderFillRect(g_app.renderer, &bar);
    }
#endif
    const OverlayInputState* in = overlay_input_get();
    static int dragging = 0;
    static int drag_dx = 0;
    if (in)
    {
        int mx = in->mouse_x, my = in->mouse_y;
        int over = (mx >= handle_x - handle_w / 2 && mx < handle_x + handle_w / 2 && my >= row_y &&
                    my < row_y + row_h);
        if (!dragging && over && in->mouse_clicked)
        {
            dragging = 1;
            drag_dx = mx - handle_x;
        }
        else if (dragging && in->mouse_down)
        {
            handle_x = mx - drag_dx;
            left_w = handle_x - g_ui.cur_x;
            if (left_w < min_left_w)
                left_w = min_left_w;
            if (left_w > max_left_w)
                left_w = max_left_w;
        }
        else if (dragging && !in->mouse_down)
        {
            dragging = 0;
            overlay_prefs_set_int(id, left_w);
        }
    }

    int widths[2] = {left_w, g_ui.width - left_w - 8};
    *out_left_w = left_w;
    return overlay_columns_begin(2, widths);
}

void overlay_splitter_end(void) { overlay_columns_end(); }
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
