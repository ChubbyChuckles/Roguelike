/* asset_browser_dir_view.c - extracted from panels_asset_browser.c (refactor slice)
   Contains only the directory navigation / listing / scrollbar logic. */

#include "asset_browser_dir_view.h"
#include "../../core/app/app_state.h" /* g_app for renderer */
#include "../overlay_core.h"
#include "../overlay_theme.h"
#include "../widgets/overlay_widgets.h"
#include "../widgets/overlay_widgets_internal.h" /* g_ui, overlay_mouse_over */
#include "asset_browser_dir.h"
#include "asset_browser_json.h" /* (indirect state usage; safe) */
#include "asset_browser_state.h"
#include "asset_browser_util.h" /* for rogue_ab_truncate_ellipsis */

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(ROGUE_HAVE_SDL)
#include <SDL.h>
#endif

#include <string.h>

#define g_ab_state (*rogue_asset_browser_state())

void rogue_asset_browser_draw_directory_browser(void)
{
    rogue_asset_browser_dir_init_if_needed();
    if (g_ab_state.dir_count == 0)
        rogue_asset_browser_dir_refresh();
    overlay_label("Directory Browser:");
    overlay_label(g_ab_state.dir_cwd);
    if (overlay_button("Up") && strcmp(g_ab_state.dir_cwd, g_ab_state.dir_root) != 0)
    {
        rogue_asset_browser_dir_parent(g_ab_state.dir_cwd);
        rogue_asset_browser_dir_refresh();
        g_ab_state.dir_scroll = 0;
        g_ab_state.dir_selected = -1;
    }
    if (overlay_button("Refresh"))
    {
        rogue_asset_browser_dir_refresh();
        g_ab_state.dir_scroll = 0;
        g_ab_state.dir_selected = -1;
    }
    if (overlay_button("PgUp"))
    {
        g_ab_state.dir_scroll -= 12;
        if (g_ab_state.dir_scroll < 0)
            g_ab_state.dir_scroll = 0;
    }
    if (overlay_button("PgDn"))
    {
        g_ab_state.dir_scroll += 12;
    }
    int visible = 12; /* viewport rows */
    if (g_ab_state.dir_scroll < 0)
        g_ab_state.dir_scroll = 0;
    if (g_ab_state.dir_scroll > g_ab_state.dir_count - visible)
        g_ab_state.dir_scroll =
            (g_ab_state.dir_count - visible) < 0 ? 0 : (g_ab_state.dir_count - visible);
    const OverlayInputState* din = overlay_input_get();
    int viewport_h = visible * (g_ui.table_row_h + g_ui.table_row_pad);
    if (din && din->mouse_wheel_y &&
        overlay_mouse_over(g_ui.cur_x, g_ui.cur_y, g_ui.width, viewport_h))
    {
        g_ab_state.dir_scroll -= din->mouse_wheel_y;
        if (g_ab_state.dir_scroll < 0)
            g_ab_state.dir_scroll = 0;
        if (g_ab_state.dir_scroll > g_ab_state.dir_count - visible)
            g_ab_state.dir_scroll =
                (g_ab_state.dir_count - visible) < 0 ? 0 : (g_ab_state.dir_count - visible);
    }
    int end = g_ab_state.dir_scroll + visible;
    if (end > g_ab_state.dir_count)
        end = g_ab_state.dir_count;
    /* List */
    {
        int row_h = (g_ui.table_row_h + g_ui.table_row_pad);
        int base_y = g_ui.cur_y;
        int sb_w_reserved = 10;
        for (int i = g_ab_state.dir_scroll; i < end; ++i)
        {
            char line[ROGUE_FILE_DIALOG_PATH_MAX + 32];
            char name_buf[ROGUE_FILE_DIALOG_PATH_MAX];
            rogue_ab_truncate_ellipsis(name_buf, sizeof name_buf, g_ab_state.dir_entries[i].name,
                                       40);
            snprintf(line, sizeof line, "%s %s",
                     g_ab_state.dir_entries[i].is_dir ? "[DIR]" : "FILE ", name_buf);
#if defined(ROGUE_HAVE_SDL)
            if (g_app.renderer)
            {
                const OverlayTheme* th = overlay_theme_get();
                int row_x = g_ui.cur_x - 8;
                int row_y = base_y + (i - g_ab_state.dir_scroll) * row_h;
                SDL_Rect rr = {row_x, row_y, g_ui.width - sb_w_reserved - 2, g_ui.table_row_h};
                int sel = (i == g_ab_state.dir_selected);
                OverlayColor bg = sel ? th->table_row_bg_sel : th->table_row_bg;
                SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
                SDL_RenderFillRect(g_app.renderer, &rr);
            }
#endif
            overlay_label(line);
        }
    }
#if defined(ROGUE_HAVE_SDL)
    /* Scrollbar */
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        int sb_w = 10;
        int row_h = (g_ui.table_row_h + g_ui.table_row_pad);
        int track_h = visible * row_h;
        int track_x = g_ui.cur_x + g_ui.width - sb_w - 2;
        int track_y = g_ui.cur_y - track_h;
        if (track_y < 0)
            track_y = 0;
        SDL_Rect track = {track_x, track_y, sb_w, track_h};
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r / 2, th->panel_bg.g / 2,
                               th->panel_bg.b / 2, (Uint8) th->panel_bg.a);
        SDL_RenderFillRect(g_app.renderer, &track);
        if (g_ab_state.dir_count > visible)
        {
            int max_first = (g_ab_state.dir_count - visible);
            if (max_first < 1)
                max_first = 1;
            int thumb_h = (track_h * visible) / g_ab_state.dir_count;
            if (thumb_h < 14)
                thumb_h = 14;
            if (thumb_h > track_h)
                thumb_h = track_h;
            int range = track_h - thumb_h;
            int thumb_y = track_y;
            if (range > 0)
                thumb_y = track_y + (range * g_ab_state.dir_scroll) / max_first;
            SDL_Rect thumb = {track_x + 1, thumb_y, sb_w - 2, thumb_h};
            const OverlayInputState* in_sb = overlay_input_get();
            int hover_sb = overlay_mouse_over(track_x, track_y, sb_w, track_h);
            OverlayColor tcol = hover_sb ? th->accent_2 : th->accent_1;
            SDL_SetRenderDrawColor(g_app.renderer, tcol.r, tcol.g, tcol.b, tcol.a);
            SDL_RenderFillRect(g_app.renderer, &thumb);
            static int s_drag = 0;
            static int s_drag_off = 0;
            if (in_sb->mouse_clicked && hover_sb)
            {
                if (in_sb->mouse_y < thumb_y)
                {
                    g_ab_state.dir_scroll -= visible;
                    if (g_ab_state.dir_scroll < 0)
                        g_ab_state.dir_scroll = 0;
                }
                else if (in_sb->mouse_y > thumb_y + thumb_h)
                {
                    g_ab_state.dir_scroll += visible;
                }
                else
                {
                    s_drag = 1;
                    s_drag_off = in_sb->mouse_y - thumb_y;
                    overlay_input_set_capture(1, 1);
                }
            }
            if (s_drag && in_sb->mouse_down)
            {
                int new_thumb_y = in_sb->mouse_y - s_drag_off;
                if (new_thumb_y < track_y)
                    new_thumb_y = track_y;
                if (new_thumb_y > track_y + range)
                    new_thumb_y = track_y + range;
                if (range > 0)
                {
                    int new_off = (int) (((long long) (new_thumb_y - track_y) * max_first) /
                                         (range ? range : 1));
                    if (new_off != g_ab_state.dir_scroll)
                        g_ab_state.dir_scroll = new_off;
                }
            }
            if (s_drag && !in_sb->mouse_down)
                s_drag = 0;
        }
    }
#endif
    /* Click selection */
    if (din && din->mouse_clicked)
    {
        int sb_w = 10;
        int track_x = g_ui.cur_x + g_ui.width - sb_w - 2;
        int clickable_w = g_ui.width - sb_w - 2;
        int over_list =
            overlay_mouse_over(g_ui.cur_x, g_ui.cur_y - viewport_h, clickable_w, viewport_h);
        int over_scrollbar =
            (din->mouse_x >= track_x && din->mouse_x < track_x + sb_w &&
             din->mouse_y >= (g_ui.cur_y - viewport_h) && din->mouse_y < g_ui.cur_y);
        if (over_list && !over_scrollbar)
        {
            int rel_y = din->mouse_y - (g_ui.cur_y - viewport_h);
            int row_h = (g_ui.table_row_h + g_ui.table_row_pad);
            if (row_h <= 0)
                row_h = 14;
            int idx = rel_y / row_h + g_ab_state.dir_scroll;
            if (idx >= 0 && idx < g_ab_state.dir_count)
            {
                g_ab_state.dir_selected = idx;
                if (g_ab_state.dir_entries[idx].is_dir)
                {
                    char joined[ROGUE_FILE_DIALOG_PATH_MAX * 2];
                    rogue_asset_browser_dir_join(joined, sizeof joined, g_ab_state.dir_cwd,
                                                 g_ab_state.dir_entries[idx].name);
                    strncpy(g_ab_state.dir_cwd, joined, sizeof g_ab_state.dir_cwd);
                    g_ab_state.dir_cwd[sizeof g_ab_state.dir_cwd - 1] = '\0';
                    rogue_asset_browser_dir_refresh();
                    g_ab_state.dir_scroll = 0;
                    g_ab_state.dir_selected = -1;
                }
                else
                {
                    char full[ROGUE_FILE_DIALOG_PATH_MAX * 2];
                    rogue_asset_browser_dir_join(full, sizeof full, g_ab_state.dir_cwd,
                                                 g_ab_state.dir_entries[idx].name);
                    strncpy(g_ab_state.pending_import_path, full,
                            sizeof g_ab_state.pending_import_path);
                    g_ab_state.pending_import_path[sizeof g_ab_state.pending_import_path - 1] =
                        '\0';
                }
            }
        }
    }
}
