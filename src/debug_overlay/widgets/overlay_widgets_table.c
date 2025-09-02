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
    g_ui.table_row_pad = 2;
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
    int row_y = g_ui.cur_y + (g_ui.table_row_pad > 0 ? g_ui.table_row_pad : 0);
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

/* Adjust row height and padding for current table */
void overlay_table_set_row_style(int row_height_px, int row_padding_px)
{
    if (!g_ui.panel_active || !g_ui.table_active)
        return;
    if (row_height_px > 8 && row_height_px < 128)
        g_ui.table_row_h = row_height_px;
    if (row_padding_px >= 0 && row_padding_px < 32)
        g_ui.table_row_pad = row_padding_px;
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

/* Vertical scrollbar for current table (drawn to the right of the table area). */
int overlay_table_scrollbar(int total_rows, int visible_rows, int* row_offset)
{
    if (!g_ui.panel_active || !g_ui.table_active || !row_offset)
        return 0;
    if (visible_rows <= 0)
        visible_rows = 1;
    if (total_rows < 0)
        total_rows = 0;
    int changed = 0;
    int max_first = (total_rows > visible_rows) ? (total_rows - visible_rows) : 0;
    if (*row_offset < 0)
    {
        *row_offset = 0;
        changed = 1;
    }
    if (*row_offset > max_first)
    {
        *row_offset = max_first;
        changed = 1;
    }

#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        /* Place scrollbar on the far right of the current row; 8px wide */
        int sb_w = 8;
        int sb_x = g_ui.cur_x + g_ui.width - sb_w - 2;
        /* Compute table area height covered by rows drawn so far. For simplicity, use
           visible_rows * row_h as the track height. */
        int track_h = visible_rows * (g_ui.table_row_h + g_ui.table_row_pad);
        int track_x = sb_x;
        int track_y = g_ui.cur_y - (visible_rows * (g_ui.table_row_h + g_ui.table_row_pad)) +
                      (g_ui.table_row_pad > 0 ? g_ui.table_row_pad : 0);
        if (track_y < 0)
            track_y = 0;
        SDL_Rect track = {track_x, track_y, sb_w, track_h};
        /* Use table_border as track color */
        SDL_SetRenderDrawColor(g_app.renderer, th->table_border.r, th->table_border.g,
                               th->table_border.b, th->table_border.a);
        SDL_RenderFillRect(g_app.renderer, &track);

        /* Thumb size proportional to visible/total; min height 12px */
        int thumb_h = (total_rows > 0) ? (track_h * visible_rows / (total_rows)) : track_h;
        if (thumb_h < 12)
            thumb_h = 12;
        if (thumb_h > track_h)
            thumb_h = track_h;
        int range = (track_h - thumb_h);
        int thumb_y = track_y;
        if (max_first > 0 && range > 0)
            thumb_y = track_y + (range * (*row_offset)) / max_first;
        SDL_Rect thumb = {track_x, thumb_y, sb_w, thumb_h};
        int hover = overlay_mouse_over(track_x, track_y, sb_w, track_h);
        /* Use accent colors for thumb */
        OverlayColor tcol = hover ? th->accent_2 : th->accent_1;
        SDL_SetRenderDrawColor(g_app.renderer, tcol.r, tcol.g, tcol.b, tcol.a);
        SDL_RenderFillRect(g_app.renderer, &thumb);

        /* Interaction: click on track to page, drag thumb to scroll. */
        const OverlayInputState* in = overlay_input_get();
        static int s_dragging = 0;
        static int s_drag_offset = 0; /* pixel offset inside thumb at drag start */
        if (in->mouse_clicked && hover)
        {
            if (in->mouse_y < thumb_y)
            {
                /* Page up */
                *row_offset -= visible_rows;
                if (*row_offset < 0)
                    *row_offset = 0;
                changed = 1;
            }
            else if (in->mouse_y > (thumb_y + thumb_h))
            {
                /* Page down */
                *row_offset += visible_rows;
                if (*row_offset > max_first)
                    *row_offset = max_first;
                changed = 1;
            }
            else
            {
                /* Begin drag */
                s_dragging = 1;
                s_drag_offset = in->mouse_y - thumb_y;
                overlay_input_set_capture(1, 1);
            }
        }
        if (s_dragging && in->mouse_down)
        {
            int new_thumb_y = in->mouse_y - s_drag_offset;
            if (new_thumb_y < track_y)
                new_thumb_y = track_y;
            if (new_thumb_y > track_y + range)
                new_thumb_y = track_y + range;
            int new_offset = 0;
            if (range > 0 && max_first > 0)
                new_offset = (int) (((long long) (new_thumb_y - track_y) * max_first) / range);
            if (new_offset != *row_offset)
            {
                *row_offset = new_offset;
                changed = 1;
            }
        }
        if (s_dragging && !in->mouse_down)
        {
            s_dragging = 0;
        }
    }
#endif
    return changed;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
