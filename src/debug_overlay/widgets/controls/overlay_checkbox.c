/* Reformatted checkbox control */
#include "../../overlay_input.h"
#include "../../overlay_theme.h"
#include "../../overlay_tooltip.h"
#include "../overlay_widgets_internal.h"
#include "controls_shared.h"
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../../../core/app/app_state.h"
#include "../../../graphics/font.h"

int overlay_checkbox(const char* label, int* value)
{
    if (!g_ui.panel_active)
        return 0;
    int sz = 16;
    int x = g_ui.cur_x;
    int y = g_ui.cur_y + 2;
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    if (g_ui.row_max_h < sz + 4)
        g_ui.row_max_h = sz + 4;

#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect box = {x, y, sz, sz};
        int hover = overlay_mouse_over(x, y, sz, sz);
        OverlayColor cbg = th->checkbox_bg;
        if (hover)
            cbg = shade_color(cbg, +10);
        SDL_SetRenderDrawColor(g_app.renderer, cbg.r, cbg.g, cbg.b, cbg.a);
        SDL_RenderFillRect(g_app.renderer, &box);
        SDL_SetRenderDrawColor(g_app.renderer, th->checkbox_border.r, th->checkbox_border.g,
                               th->checkbox_border.b, th->checkbox_border.a);
        SDL_RenderDrawRect(g_app.renderer, &box);
        if (value && *value)
        {
            SDL_Rect inner = {x + 3, y + 3, sz - 6, sz - 6};
            SDL_SetRenderDrawColor(g_app.renderer, th->checkbox_tick.r, th->checkbox_tick.g,
                                   th->checkbox_tick.b, th->checkbox_tick.a);
            SDL_RenderFillRect(g_app.renderer, &inner);
        }
        if (g_ui.focus_index == id)
        {
            SDL_Rect fr = {x + 1, y + 1, sz - 2, sz - 2};
            SDL_SetRenderDrawColor(g_app.renderer, th->accent_1.r, th->accent_1.g, th->accent_1.b,
                                   th->accent_1.a);
            SDL_RenderDrawRect(g_app.renderer, &fr);
        }
    }
#endif

    if (tip)
        overlay_tooltip_track(id, x, y, sz, sz, tip);
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(x + sz + 6, g_ui.cur_y + 2, label ? label : "", 1,
                             (RogueColor){th->checkbox_label.r, th->checkbox_label.g,
                                          th->checkbox_label.b, th->checkbox_label.a});
    }

    const OverlayInputState* in = overlay_input_get();
    int over = overlay_mouse_over(x, y, sz, sz);
    if (over && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    int changed = 0;
    if (((over && in->mouse_clicked) ||
         (g_ui.focus_index == id && (in->key_space_pressed || in->key_enter_pressed))) &&
        value)
    {
        *value = !*value;
        changed = 1;
        overlay_input_set_capture(1, 1);
    }

    if (g_ui.columns > 1)
    {
        overlay_next_column();
        if (g_ui.col_index == 0)
            ui_next_line();
    }
    else
    {
        ui_next_line();
    }
    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
