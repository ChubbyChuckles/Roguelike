/* Reformatted combo, tree, color edit controls */
#include "../../overlay_input.h"
#include "../../overlay_theme.h"
#include "../../overlay_tooltip.h"
#include "../overlay_widgets_internal.h"
#include "controls_shared.h"
#include <stdio.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../../../core/app/app_state.h"
#include "../../../graphics/font.h"

int overlay_combo(const char* label, int* current_index, const char* const* items, int count)
{
    if (!g_ui.panel_active || !current_index || !items || count <= 0)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y;
    int w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width);
    int h = 18;
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
    const OverlayInputState* in = overlay_input_get();
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect r = {x, y + 2, w, h};
        int hover = overlay_mouse_over(x, y + 2, w, h);
        OverlayColor bg = th->input_bg;
        if (hover)
            bg = shade_color(bg, +6);
        SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->input_border.r, th->input_border.g,
                               th->input_border.b, th->input_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
        if (g_ui.focus_index == id)
        {
            SDL_Rect fr = {x + 1, y + 3, w - 2, h - 2};
            SDL_SetRenderDrawColor(g_app.renderer, th->accent_2.r, th->accent_2.g, th->accent_2.b,
                                   th->accent_2.a);
            SDL_RenderDrawRect(g_app.renderer, &fr);
        }
    }
#endif
    if (tip)
        overlay_tooltip_track(id, x, y + 2, w, h, tip);
    int changed = 0;
    int over = overlay_mouse_over(x, y + 2, w, h);
    if (over && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
        *current_index = (*current_index + 1) % count;
        changed = 1;
    }
    if (g_ui.focus_index == id && (in->key_left_pressed || in->key_right_pressed))
    {
        int nv = *current_index + (in->key_right_pressed ? 1 : -1);
        if (nv < 0)
            nv = count - 1;
        if (nv >= count)
            nv = 0;
        if (nv != *current_index)
        {
            *current_index = nv;
            changed = 1;
        }
        overlay_input_set_capture(1, 1);
    }
    const char* cur = items[*current_index >= 0 && *current_index < count ? *current_index : 0];
    char line[256];
    snprintf(line, sizeof line, "%s: %s", label ? label : "", cur ? cur : "<none>");
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(x + 6, y + 2, line, 1,
                             (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
    }
    if (g_ui.columns > 1)
    {
        overlay_next_column();
        if (g_ui.col_index == 0)
            ui_next_line();
    }
    else
        ui_next_line();
    return changed;
}

int overlay_tree_node(const char* label, int* open)
{
    if (!g_ui.panel_active || !label || !open)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y;
    int w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width);
    int h = 18;
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect r = {x, y + 2, w, h};
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r, th->panel_bg.g, th->panel_bg.b,
                               th->panel_bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                               th->panel_border.b, th->panel_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
        if (g_ui.focus_index == id)
        {
            SDL_Rect fr = {x + 1, y + 3, w - 2, h - 2};
            SDL_SetRenderDrawColor(g_app.renderer, th->accent_1.r, th->accent_1.g, th->accent_1.b,
                                   th->accent_1.a);
            SDL_RenderDrawRect(g_app.renderer, &fr);
        }
    }
#endif
    if (tip)
        overlay_tooltip_track(id, x, y + 2, w, h, tip);
    const char* arrow = (*open ? "▾" : "▸");
    char line[256];
    snprintf(line, sizeof line, "%s %s", arrow, label);
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(x + 6, y + 2, line, 1,
                             (RogueColor){th->text_accent.r, th->text_accent.g, th->text_accent.b,
                                          th->text_accent.a});
    }
    const OverlayInputState* in = overlay_input_get();
    int over = overlay_mouse_over(x, y + 2, w, h);
    if (over && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        *open = !*open;
        overlay_input_set_capture(1, 1);
    }
    if (g_ui.focus_index == id && (in->key_enter_pressed || in->key_space_pressed))
    {
        *open = !*open;
        overlay_input_set_capture(1, 1);
    }
    if (g_ui.columns > 1)
    {
        overlay_next_column();
        if (g_ui.col_index == 0)
            ui_next_line();
    }
    else
        ui_next_line();
    return *open ? 1 : 0;
}

void overlay_tree_pop(void) {}

int overlay_color_edit_rgba(const char* label, unsigned char rgba[4])
{
    if (!g_ui.panel_active || !rgba)
        return 0;
    int changed = 0;
    int widths[5] = {40, 0, 0, 0, 0};
    int remaining = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width) - widths[0];
    for (int i = 1; i < 5; ++i)
        widths[i] = remaining / 4;
    if (overlay_columns_begin(5, widths))
    {
#ifdef ROGUE_HAVE_SDL
        if (g_app.renderer)
        {
            SDL_Rect sw = {g_ui.cur_x, g_ui.cur_y + 2, widths[0] - 6, 16};
            SDL_SetRenderDrawColor(g_app.renderer, rgba[0], rgba[1], rgba[2], rgba[3]);
            SDL_RenderFillRect(g_app.renderer, &sw);
            const OverlayTheme* th = overlay_theme_get();
            SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                                   th->panel_border.b, th->panel_border.a);
            SDL_RenderDrawRect(g_app.renderer, &sw);
        }
#endif
        overlay_next_column();
        int r = rgba[0], g = rgba[1], b = rgba[2], a = rgba[3];
        changed |= overlay_slider_int("R", &r, 0, 255);
        overlay_next_column();
        changed |= overlay_slider_int("G", &g, 0, 255);
        overlay_next_column();
        changed |= overlay_slider_int("B", &b, 0, 255);
        overlay_next_column();
        changed |= overlay_slider_int("A", &a, 0, 255);
        overlay_columns_end();
        if (changed)
        {
            rgba[0] = (unsigned char) r;
            rgba[1] = (unsigned char) g;
            rgba[2] = (unsigned char) b;
            rgba[3] = (unsigned char) a;
        }
    }
    char cap[64];
    snprintf(cap, sizeof cap, "%s: #%02X%02X%02X %u", label ? label : "Color", rgba[0], rgba[1],
             rgba[2], (unsigned) rgba[3]);
    overlay_label(cap);
    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
