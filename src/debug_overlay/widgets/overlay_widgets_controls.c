#include "../overlay_icon.h"
#include "../overlay_prefs.h"
#include "../overlay_tooltip.h"
#include "overlay_widgets_internal.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

#include "../../core/app/app_state.h"
#include "../../graphics/font.h"
#include "../overlay_theme.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <stdio.h>
#include <string.h>

/* Lightweight color shade helper (clamps to 0..255). Positive delta lightens, negative darkens. */
static OverlayColor shade_color(OverlayColor c, int delta)
{
    int r = (int) c.r + delta;
    int g = (int) c.g + delta;
    int b = (int) c.b + delta;
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    if (b > 255)
        b = 255;
    OverlayColor out = {(unsigned char) r, (unsigned char) g, (unsigned char) b, c.a};
    return out;
}

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
        /* Focus ring (subtle accent outline inside the button bounds) */
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
    {
        ui_next_line();
    }
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
    /* Icon (left), then label */
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
    {
        ui_next_line();
    }
    return clicked;
}

int overlay_checkbox(const char* label, int* value)
{
    if (!g_ui.panel_active)
        return 0;
    int changed = 0;
    int sz = 16;
    int x = g_ui.cur_x, y = g_ui.cur_y + 2;
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
        /* Subtle focus tick outline */
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
    if (overlay_mouse_over(x, y, sz, sz) && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    if (((overlay_mouse_over(x, y, sz, sz) && in->mouse_clicked) ||
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

int overlay_slider_int(const char* label, int* value, int minv, int maxv)
{
    if (!g_ui.panel_active || !value)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y,
        w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width), h = 18;
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect bar = {x, y + 2, w, h};
        /* Hover/focus styling */
        int hover = overlay_mouse_over(x, y + 2, w, h);
        OverlayColor bg = th->input_bg;
        if (hover)
            bg = shade_color(bg, +6);
        SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(g_app.renderer, &bar);
        SDL_SetRenderDrawColor(g_app.renderer, th->input_border.r, th->input_border.g,
                               th->input_border.b, th->input_border.a);
        SDL_RenderDrawRect(g_app.renderer, &bar);
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
    const OverlayInputState* in = overlay_input_get();
    int changed = 0;
    if (overlay_mouse_over(x, y + 2, w, h) && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    if ((overlay_mouse_over(x, y + 2, w, h) && in->mouse_clicked) ||
        (g_ui.focus_index == id && (in->key_left_pressed || in->key_right_pressed)))
    {
        float t;
        if (g_ui.focus_index == id && (in->key_left_pressed || in->key_right_pressed))
        {
            t = (float) (*value - minv) / (float) (maxv - minv);
            t += (in->key_right_pressed ? (1.0f / (float) (maxv - minv))
                                        : -(1.0f / (float) (maxv - minv)));
        }
        else
        {
            t = (float) (in->mouse_x - x) / (float) w;
        }
        if (t < 0.f)
            t = 0.f;
        if (t > 1.f)
            t = 1.f;
        int nv = (int) (minv + t * (maxv - minv));
        if (nv != *value)
        {
            *value = nv;
            changed = 1;
        }
        overlay_input_set_capture(1, 1);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: %d", label ? label : "", value ? *value : 0);
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(x + 6, y + 2, buf, 1,
                             (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
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

int overlay_slider_float(const char* label, float* value, float minv, float maxv)
{
    if (!g_ui.panel_active || !value)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y,
        w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width), h = 18;
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect bar = {x, y + 2, w, h};
        int hover = overlay_mouse_over(x, y + 2, w, h);
        OverlayColor bg = th->input_bg;
        if (hover)
            bg = shade_color(bg, +6);
        SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(g_app.renderer, &bar);
        SDL_SetRenderDrawColor(g_app.renderer, th->input_border.r, th->input_border.g,
                               th->input_border.b, th->input_border.a);
        SDL_RenderDrawRect(g_app.renderer, &bar);
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
    const OverlayInputState* in = overlay_input_get();
    int changed = 0;
    if (overlay_mouse_over(x, y + 2, w, h) && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    if ((overlay_mouse_over(x, y + 2, w, h) && in->mouse_clicked) ||
        (g_ui.focus_index == id && (in->key_left_pressed || in->key_right_pressed)))
    {
        float t;
        if (g_ui.focus_index == id && (in->key_left_pressed || in->key_right_pressed))
        {
            float cur = (*value - minv) / (maxv - minv);
            t = cur + (in->key_right_pressed ? 0.01f : -0.01f);
        }
        else
        {
            t = (float) (in->mouse_x - x) / (float) w;
        }
        if (t < 0.f)
            t = 0.f;
        if (t > 1.f)
            t = 1.f;
        float nv = minv + t * (maxv - minv);
        if (nv != *value)
        {
            *value = nv;
            changed = 1;
        }
        overlay_input_set_capture(1, 1);
    }
    char buf[128];
    snprintf(buf, sizeof buf, "%s: %.3f", label ? label : "", value ? *value : 0.0f);
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(x + 6, y + 2, buf, 1,
                             (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
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

int overlay_input_text(const char* label, char* buf, size_t buf_size)
{
    if (!g_ui.panel_active || !buf || buf_size == 0)
        return 0;
    const OverlayInputState* in = overlay_input_get();
    int changed = 0;
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    int h = 18;
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
    int x = g_ui.cur_x, y = g_ui.cur_y,
        w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width), h2 = h;
    int will_focus = overlay_mouse_over(x, y + 2, w, h2) && in->mouse_clicked;
    if (will_focus)
    {
        overlay_input_set_capture(1, 1);
        g_ui.focus_index = id;
        g_ui.caret_pos = (int) strlen(buf);
    }
    int has_focus = (g_ui.focus_index == id);
    if (has_focus)
    {
        size_t len = strlen(buf);
        if (in->key_home_pressed)
            g_ui.caret_pos = 0;
        if (in->key_end_pressed)
            g_ui.caret_pos = (int) len;
        if (g_ui.caret_pos < 0)
            g_ui.caret_pos = 0;
        if (g_ui.caret_pos > (int) len)
            g_ui.caret_pos = (int) len;
    }
    if (has_focus && in->key_backspace_pressed && strlen(buf) > 0)
    {
        size_t len = strlen(buf);
        if (g_ui.caret_pos > 0 && g_ui.caret_pos <= (int) len)
        {
            memmove(buf + g_ui.caret_pos - 1, buf + g_ui.caret_pos,
                    len - (size_t) g_ui.caret_pos + 1);
            g_ui.caret_pos--;
        }
        else if (len > 0)
        {
            buf[len - 1] = '\0';
        }
        changed = 1;
    }
    if (has_focus && in->text_input[0] != '\0')
    {
        size_t cur = strlen(buf);
        size_t add = strlen(in->text_input);
        if (g_ui.caret_pos < 0 || g_ui.caret_pos > (int) cur)
            g_ui.caret_pos = (int) cur;
        if (cur + add >= buf_size)
            add = buf_size - 1 - cur;
        if (add > 0)
        {
            memmove(buf + g_ui.caret_pos + add, buf + g_ui.caret_pos,
                    cur - (size_t) g_ui.caret_pos + 1);
            memcpy(buf + g_ui.caret_pos, in->text_input, add);
            g_ui.caret_pos += (int) add;
            changed = 1;
        }
    }
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect r = {x, y + 2, w, h2};
        SDL_SetRenderDrawColor(g_app.renderer, th->input_bg.r, th->input_bg.g, th->input_bg.b,
                               th->input_bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->input_border.r, th->input_border.g,
                               th->input_border.b, th->input_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
    }
#endif
    if (tip)
        overlay_tooltip_track(id, x, y + 2, w, h2, tip);
    char line[256];
    snprintf(line, sizeof(line), "%s: %s", label ? label : "", buf);
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(
            x + 6, y + 2, line, 1,
            (RogueColor){th->input_text.r, th->input_text.g, th->input_text.b, th->input_text.a});
    }
    if (overlay_mouse_over(x, y + 2, w, h2) && overlay_input_get()->mouse_clicked)
    {
        overlay_input_set_capture(1, 1);
        g_ui.focus_index = id;
        g_ui.caret_pos = (int) strlen(buf);
    }
    if (has_focus)
    {
        if (in->key_left_pressed && g_ui.caret_pos > 0)
            g_ui.caret_pos--;
        if (in->key_right_pressed && g_ui.caret_pos < (int) strlen(buf))
            g_ui.caret_pos++;
        if (in->key_escape_pressed)
            g_ui.focus_index = -1;
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

int overlay_combo(const char* label, int* current_index, const char* const* items, int count)
{
    if (!g_ui.panel_active || !current_index || !items || count <= 0)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y,
        w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width), h = 18;
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
    if (overlay_mouse_over(x, y + 2, w, h) && in->mouse_clicked)
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
    {
        ui_next_line();
    }
    return changed;
}

int overlay_input_vec2(const char* label, float* x, float* y, float minv, float maxv)
{
    if (!g_ui.panel_active || !x || !y)
        return 0;
    int changed = 0;
    /* Label */
    overlay_label(label ? label : "");
    /* Two columns for X/Y sliders */
    int widths[2] = {(g_ui.width - 8) / 2, (g_ui.width - 8) / 2};
    if (overlay_columns_begin(2, widths))
    {
        char lx[32];
        snprintf(lx, sizeof lx, "X: %.2f", *x);
        /* Reuse float slider visuals by drawing bar and capturing input the same way. */
        int cx = g_ui.cur_x, cy = g_ui.cur_y, w = g_ui.col_widths[g_ui.col_index], h = 18;
        int id_x = g_ui.total_widgets++;
        const OverlayInputState* in = overlay_input_get();
#ifdef ROGUE_HAVE_SDL
        if (g_app.renderer)
        {
            const OverlayTheme* th = overlay_theme_get();
            SDL_Rect bar = {cx, cy + 2, w, h};
            int hover = overlay_mouse_over(cx, cy + 2, w, h);
            OverlayColor bg = th->input_bg;
            if (hover)
                bg = shade_color(bg, +6);
            SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(g_app.renderer, &bar);
            SDL_SetRenderDrawColor(g_app.renderer, th->input_border.r, th->input_border.g,
                                   th->input_border.b, th->input_border.a);
            SDL_RenderDrawRect(g_app.renderer, &bar);
            if (g_ui.focus_index == id_x)
            {
                SDL_Rect fr = {cx + 1, cy + 3, w - 2, h - 2};
                SDL_SetRenderDrawColor(g_app.renderer, th->accent_2.r, th->accent_2.g,
                                       th->accent_2.b, th->accent_2.a);
                SDL_RenderDrawRect(g_app.renderer, &fr);
            }
        }
#endif
        if (overlay_mouse_over(cx, cy + 2, w, h) && in->mouse_clicked)
        {
            g_ui.focus_index = id_x;
            overlay_input_set_capture(1, 1);
        }
        if ((overlay_mouse_over(cx, cy + 2, w, h) && in->mouse_clicked) ||
            (g_ui.focus_index == id_x && (in->key_left_pressed || in->key_right_pressed)))
        {
            float t;
            if (g_ui.focus_index == id_x && (in->key_left_pressed || in->key_right_pressed))
            {
                float rang = (maxv - minv);
                if (rang <= 0.0f)
                    rang = 1.0f;
                t = (*x - minv) / rang;
                t += (in->key_right_pressed ? (1.0f / rang) : -(1.0f / rang));
            }
            else
            {
                t = (float) (in->mouse_x - cx) / (float) w;
            }
            if (t < 0.f)
                t = 0.f;
            if (t > 1.f)
                t = 1.f;
            float nv = minv + t * (maxv - minv);
            if (nv != *x)
            {
                *x = nv;
                changed = 1;
            }
            overlay_input_set_capture(1, 1);
        }
        {
            const OverlayTheme* th = overlay_theme_get();
            rogue_font_draw_text(cx + 6, cy + 2, lx, 1,
                                 (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
        }
        overlay_next_column();

        char ly[32];
        snprintf(ly, sizeof ly, "Y: %.2f", *y);
        int cx2 = g_ui.cur_x, cy2 = g_ui.cur_y, w2 = g_ui.col_widths[g_ui.col_index];
        int id_y = g_ui.total_widgets++;
#ifdef ROGUE_HAVE_SDL
        if (g_app.renderer)
        {
            const OverlayTheme* th = overlay_theme_get();
            SDL_Rect bar = {cx2, cy2 + 2, w2, h};
            int hover = overlay_mouse_over(cx2, cy2 + 2, w2, h);
            OverlayColor bg = th->input_bg;
            if (hover)
                bg = shade_color(bg, +6);
            SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(g_app.renderer, &bar);
            SDL_SetRenderDrawColor(g_app.renderer, th->input_border.r, th->input_border.g,
                                   th->input_border.b, th->input_border.a);
            SDL_RenderDrawRect(g_app.renderer, &bar);
            if (g_ui.focus_index == id_y)
            {
                SDL_Rect fr = {cx2 + 1, cy2 + 3, w2 - 2, h - 2};
                SDL_SetRenderDrawColor(g_app.renderer, th->accent_2.r, th->accent_2.g,
                                       th->accent_2.b, th->accent_2.a);
                SDL_RenderDrawRect(g_app.renderer, &fr);
            }
        }
#endif
        if (overlay_mouse_over(cx2, cy2 + 2, w2, h) && overlay_input_get()->mouse_clicked)
        {
            g_ui.focus_index = id_y;
            overlay_input_set_capture(1, 1);
        }
        const OverlayInputState* in2 = overlay_input_get();
        if ((overlay_mouse_over(cx2, cy2 + 2, w2, h) && in2->mouse_clicked) ||
            (g_ui.focus_index == id_y && (in2->key_left_pressed || in2->key_right_pressed)))
        {
            float t;
            if (g_ui.focus_index == id_y && (in2->key_left_pressed || in2->key_right_pressed))
            {
                float rang = (maxv - minv);
                if (rang <= 0.0f)
                    rang = 1.0f;
                t = (*y - minv) / rang;
                t += (in2->key_right_pressed ? (1.0f / rang) : -(1.0f / rang));
            }
            else
            {
                t = (float) (in2->mouse_x - cx2) / (float) w2;
            }
            if (t < 0.f)
                t = 0.f;
            if (t > 1.f)
                t = 1.f;
            float nv = minv + t * (maxv - minv);
            if (nv != *y)
            {
                *y = nv;
                changed = 1;
            }
            overlay_input_set_capture(1, 1);
        }
        {
            const OverlayTheme* th = overlay_theme_get();
            rogue_font_draw_text(cx2 + 6, cy2 + 2, ly, 1,
                                 (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
        }
        overlay_columns_end();
        ui_next_line();
    }
    return changed;
}

int overlay_tree_node(const char* label, int* open)
{
    if (!g_ui.panel_active || !label || !open)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y,
        w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width), h = 18;
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
    if (overlay_mouse_over(x, y + 2, w, h) && in->mouse_clicked)
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
    {
        ui_next_line();
    }
    return *open ? 1 : 0;
}

void overlay_tree_pop(void) { /* placeholder for future indent/stack */ }

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
    int row_h = g_ui.line_h * 20; /* generous block; caller content should fit */
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
    /* Set up columns using computed left_w and remaining as right */
    int widths[2] = {left_w, g_ui.width - left_w - 8};
    *out_left_w = left_w;
    return overlay_columns_begin(2, widths);
}

void overlay_splitter_end(void) { overlay_columns_end(); }

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
