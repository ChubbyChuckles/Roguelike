#include "../../overlay_icon.h"
#include "../../overlay_input.h"
#include "../../overlay_theme.h"
#include "../../overlay_tooltip.h"
#include "../overlay_widgets_internal.h"
#include <stdio.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../../../core/app/app_state.h"
#include "../../../graphics/font.h"
#include "controls_shared.h" /* shade_color */

void overlay_label(const char* text)
{
    if (!g_ui.panel_active)
        return;
    const OverlayTheme* th = overlay_theme_get();
    rogue_font_draw_text(g_ui.cur_x, g_ui.cur_y + 4, text ? text : "", 1,
                         (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
    if (g_ui.row_max_h < 20)
        g_ui.row_max_h = 20;
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
}

int overlay_button(const char* label)
{
    if (!g_ui.panel_active)
        return 0;
    int h = 20;
    int x = g_ui.cur_x, y = g_ui.cur_y;
    int w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width);
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    if (g_ui.row_max_h < h)
        g_ui.row_max_h = h;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect r = {x, y, w, h};
        int hot = overlay_mouse_over(x, y, w, h);
        const OverlayInputState* in = overlay_input_get();
        int pressed = hot && in && in->mouse_down;
        int focused = (g_ui.focus_index == id);
        OverlayColor bg = hot ? th->button_bg_hot : th->button_bg;
        if (pressed)
            bg = shade_color(bg, -20);
        SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->button_border.r, th->button_border.g,
                               th->button_border.b, th->button_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
        if (focused)
        {
            SDL_Rect fr = {x + 1, y + 1, w - 2, h - 2};
            SDL_SetRenderDrawColor(g_app.renderer, th->accent_1.r, th->accent_1.g, th->accent_1.b,
                                   th->accent_1.a);
            SDL_RenderDrawRect(g_app.renderer, &fr);
        }
    }
#endif
    if (tip)
        overlay_tooltip_track(id, x, y, w, h, tip);
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(x + 6, y + 3, label ? label : "", 1,
                             (RogueColor){th->button_text.r, th->button_text.g, th->button_text.b,
                                          th->button_text.a});
    }
    const OverlayInputState* in = overlay_input_get();
    if (overlay_mouse_over(x, y, w, h) && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    int clicked = (overlay_mouse_over(x, y, w, h) && in->mouse_clicked) ||
                  (g_ui.focus_index == id && (in->key_enter_pressed || in->key_space_pressed));
    if (clicked)
        overlay_input_set_capture(1, 1);
    if (g_ui.columns > 1)
    {
        overlay_next_column();
        if (g_ui.col_index == 0)
            ui_next_line();
    }
    else
        ui_next_line();
    return clicked;
}

int overlay_icon_button(const char* label, int icon)
{
    if (!g_ui.panel_active)
        return 0;
    int h = 20;
    int x = g_ui.cur_x, y = g_ui.cur_y;
    int w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width);
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    if (g_ui.row_max_h < h)
        g_ui.row_max_h = h;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect r = {x, y, w, h};
        int hot = overlay_mouse_over(x, y, w, h);
        const OverlayInputState* in = overlay_input_get();
        int pressed = hot && in && in->mouse_down;
        int focused = (g_ui.focus_index == id);
        OverlayColor bg = hot ? th->button_bg_hot : th->button_bg;
        if (pressed)
            bg = shade_color(bg, -20);
        SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->button_border.r, th->button_border.g,
                               th->button_border.b, th->button_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
        if (focused)
        {
            SDL_Rect fr = {x + 1, y + 1, w - 2, h - 2};
            SDL_SetRenderDrawColor(g_app.renderer, th->accent_1.r, th->accent_1.g, th->accent_1.b,
                                   th->accent_1.a);
            SDL_RenderDrawRect(g_app.renderer, &fr);
        }
    }
#endif
    if (tip)
        overlay_tooltip_track(id, x, y, w, h, tip);
    overlay_icon_draw((OverlayIcon) icon, x + 4, y + 3, 1);
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(x + 4 + 12 + 4, y + 3, label ? label : "", 1,
                             (RogueColor){th->button_text.r, th->button_text.g, th->button_text.b,
                                          th->button_text.a});
    }
    const OverlayInputState* in = overlay_input_get();
    if (overlay_mouse_over(x, y, w, h) && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    int clicked = (overlay_mouse_over(x, y, w, h) && in->mouse_clicked) ||
                  (g_ui.focus_index == id && (in->key_enter_pressed || in->key_space_pressed));
    if (clicked)
        overlay_input_set_capture(1, 1);
    if (g_ui.columns > 1)
    {
        overlay_next_column();
        if (g_ui.col_index == 0)
            ui_next_line();
    }
    else
        ui_next_line();
    return clicked;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
