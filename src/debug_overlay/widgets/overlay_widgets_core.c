#include "../overlay_tooltip.h"
#include "overlay_widgets_internal.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

#include "../../core/app/app_state.h"
#include "../overlay_theme.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

UiCtx g_ui = {.focus_index = -1};

int overlay_mouse_over(int x, int y, int w, int h)
{
    const OverlayInputState* in = overlay_input_get();
    return (in->mouse_x >= x && in->mouse_x < x + w && in->mouse_y >= y && in->mouse_y < y + h);
}

void overlay_style_set(OverlayStyle s) { g_ui.style = s; }

void overlay_set_next_tooltip(const char* text) { g_ui.next_tooltip = text; }

int overlay_begin_panel(const char* title, int x, int y, int w)
{
    if (!overlay_is_enabled())
        return 0;
    overlay_tooltip_new_frame();
    g_ui.panel_active = 1;
    g_ui.cur_x = x + 8;
    g_ui.cur_y = y + 28;
    g_ui.width = w - 16;
    g_ui.line_h = 22;
    g_ui.columns = 1;
    g_ui.col_index = 0;
    g_ui.col_widths[0] = g_ui.width;
    g_ui.col_x0[0] = g_ui.cur_x;
    g_ui.row_start_y = g_ui.cur_y;
    g_ui.row_max_h = g_ui.line_h;
    g_ui.total_widgets = 0;
    g_ui.table_active = 0;
    g_ui.table_cols = 0;
    g_ui.table_row_h = 18;
    g_ui.next_tooltip = NULL;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect panel = {x, y, w, 200};
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r, th->panel_bg.g, th->panel_bg.b,
                               th->panel_bg.a);
        SDL_RenderFillRect(g_app.renderer, &panel);
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                               th->panel_border.b, th->panel_border.a);
        SDL_RenderDrawRect(g_app.renderer, &panel);
    }
#endif
    if (title)
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(
            x + 6, y + 6, title, 1,
            (RogueColor){th->title_text.r, th->title_text.g, th->title_text.b, th->title_text.a});
    }
    return 1;
}

void overlay_end_panel(void)
{
    const OverlayInputState* in = overlay_input_get();
    if (g_ui.total_widgets > 0)
    {
        if (g_ui.focus_index < 0 && in->key_tab_pressed)
        {
            g_ui.focus_index = 0;
            g_ui.caret_pos = 0;
            overlay_input_set_capture(1, 1);
        }
        else if (in->key_tab_pressed)
        {
            if (in->key_shift_down)
                g_ui.focus_index = (g_ui.focus_index - 1 + g_ui.total_widgets) % g_ui.total_widgets;
            else
                g_ui.focus_index = (g_ui.focus_index + 1) % g_ui.total_widgets;
            g_ui.caret_pos = 0;
            overlay_input_set_capture(1, 1);
        }
    }
    g_ui.panel_active = 0;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
