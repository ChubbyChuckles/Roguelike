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

/* Range slider (int): renders two stacked int sliders with constrained ordering.
 * Simpler than true dual-handle in one bar (future enhancement). Ensures *min_value <= *max_value.
 */
int overlay_range_slider_int(const char* label, int* min_value, int* max_value, int abs_min,
                             int abs_max)
{
    if (!g_ui.panel_active || !min_value || !max_value)
        return 0;
    int changed = 0;
    char lbl_min[128], lbl_max[128];
    snprintf(lbl_min, sizeof lbl_min, "%s Min", label ? label : "");
    snprintf(lbl_max, sizeof lbl_max, "%s Max", label ? label : "");
    int mn = *min_value, mx = *max_value;
    if (mn < abs_min)
        mn = abs_min;
    if (mx > abs_max)
        mx = abs_max;
    if (mx < mn)
        mx = mn;
    if (overlay_slider_int(lbl_min, &mn, abs_min, abs_max))
    {
        if (mn > mx)
            mx = mn; /* maintain ordering */
        changed = 1;
    }
    if (overlay_slider_int(lbl_max, &mx, abs_min, abs_max))
    {
        if (mx < mn)
            mn = mx; /* maintain ordering */
        changed = 1;
    }
    if (changed)
    {
        *min_value = mn;
        *max_value = mx;
    }
    return changed;
}

/* Range slider (float): two stacked float sliders enforcing ordering */
int overlay_range_slider_float(const char* label, float* min_value, float* max_value, float abs_min,
                               float abs_max)
{
    if (!g_ui.panel_active || !min_value || !max_value)
        return 0;
    int changed = 0;
    char lbl_min[128], lbl_max[128];
    snprintf(lbl_min, sizeof lbl_min, "%s Min", label ? label : "");
    snprintf(lbl_max, sizeof lbl_max, "%s Max", label ? label : "");
    float mn = *min_value, mx = *max_value;
    if (mn < abs_min)
        mn = abs_min;
    if (mx > abs_max)
        mx = abs_max;
    if (mx < mn)
        mx = mn;
    if (overlay_slider_float(lbl_min, &mn, abs_min, abs_max))
    {
        if (mn > mx)
            mx = mn;
        changed = 1;
    }
    if (overlay_slider_float(lbl_max, &mx, abs_min, abs_max))
    {
        if (mx < mn)
            mn = mx;
        changed = 1;
    }
    if (changed)
    {
        *min_value = mn;
        *max_value = mx;
    }
    return changed;
}

/* Simple Curve Editor (design helper, non-persistent v1)
 * - xs/ys arrays hold point coordinates within [x_min,x_max] / [y_min,y_max]
 * - count points (2..max_points). Maintains ascending X ordering.
 * Interaction (SDL build only):
 *   * Left click near point -> select
 *   * Left click empty space -> add point (if capacity) at click location
 *   * Drag selected point (while mouse held) to move (clamped, reorders)
 *   * Buttons: Add Mid (inserts midpoint between selected and its next neighbor), Remove
 */
int overlay_curve_editor(const char* label, float* xs, float* ys, int* count, int max_points,
                         float x_min, float x_max, float y_min, float y_max)
{
    if (!g_ui.panel_active || !xs || !ys || !count || max_points < 2)
        return 0;
    if (*count < 2)
    {
        xs[0] = x_min;
        ys[0] = y_min;
        xs[1] = x_max;
        ys[1] = y_max;
        *count = 2;
    }
    int changed = 0;
    /* Layout reserve */
    int area_w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width) - 4;
    if (area_w < 50)
        area_w = 50;
    int area_h = 120; /* fixed */
    int x = g_ui.cur_x + 2;
    int y = g_ui.cur_y + 18; /* leave room for label */
    if (g_ui.row_max_h < area_h + 20)
        g_ui.row_max_h = area_h + 20;
    int id = g_ui.total_widgets++;
    /* Draw label */
    {
        char lbl[128];
        snprintf(lbl, sizeof lbl, "%s%s", label ? label : "Curve", " (click to add / drag)");
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(g_ui.cur_x + 2, g_ui.cur_y + 2, lbl, 1,
                             (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
    }
    static int s_selected = -1; /* single global selection acceptable for v1 */
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect r = {x, y, area_w, area_h};
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r, th->panel_bg.g, th->panel_bg.b,
                               th->panel_bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                               th->panel_border.b, th->panel_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
        /* Axes (0 lines) if within range */
        if (y_min < 0 && y_max > 0)
        {
            int zy = y + (int) ((1.f - (-y_min) / (y_max - y_min)) * area_h);
            SDL_RenderDrawLine(g_app.renderer, x, zy, x + area_w, zy);
        }
        if (x_min < 0 && x_max > 0)
        {
            int zx = x + (int) ((0 - x_min) / (x_max - x_min) * area_w);
            SDL_RenderDrawLine(g_app.renderer, zx, y, zx, y + area_h);
        }
        /* Polyline */
        SDL_SetRenderDrawColor(g_app.renderer, th->accent_1.r, th->accent_1.g, th->accent_1.b,
                               th->accent_1.a);
        for (int i = 0; i < *count - 1; ++i)
        {
            int x0 = x + (int) ((xs[i] - x_min) / (x_max - x_min) * area_w);
            int y0 = y + area_h - (int) ((ys[i] - y_min) / (y_max - y_min) * area_h);
            int x1 = x + (int) ((xs[i + 1] - x_min) / (x_max - x_min) * area_w);
            int y1 = y + area_h - (int) ((ys[i + 1] - y_min) / (y_max - y_min) * area_h);
            SDL_RenderDrawLine(g_app.renderer, x0, y0, x1, y1);
        }
        /* Points */
        for (int i = 0; i < *count; ++i)
        {
            int px = x + (int) ((xs[i] - x_min) / (x_max - x_min) * area_w);
            int py = y + area_h - (int) ((ys[i] - y_min) / (y_max - y_min) * area_h);
            SDL_Rect pr = {px - 3, py - 3, 6, 6};
            OverlayColor pc = (i == s_selected) ? th->accent_2 : th->accent_1;
            SDL_SetRenderDrawColor(g_app.renderer, pc.r, pc.g, pc.b, pc.a);
            SDL_RenderFillRect(g_app.renderer, &pr);
        }
        const OverlayInputState* in = overlay_input_get();
        if (in)
        {
            int inside = (in->mouse_x >= x && in->mouse_x < x + area_w && in->mouse_y >= y &&
                          in->mouse_y < y + area_h);
            if (inside && in->mouse_clicked)
            {
                /* hit test existing */
                int found = -1;
                for (int i = 0; i < *count; ++i)
                {
                    int px = x + (int) ((xs[i] - x_min) / (x_max - x_min) * area_w);
                    int py = y + area_h - (int) ((ys[i] - y_min) / (y_max - y_min) * area_h);
                    int dx = in->mouse_x - px;
                    int dy = in->mouse_y - py;
                    if (dx * dx + dy * dy <= 64)
                    {
                        found = i;
                        break;
                    }
                }
                if (found >= 0)
                {
                    s_selected = found;
                }
                else if (*count < max_points)
                {
                    /* add */
                    float nx = x_min + (float) (in->mouse_x - x) / (float) area_w * (x_max - x_min);
                    float ny = y_min + (float) (area_h - (in->mouse_y - y)) / (float) area_h *
                                           (y_max - y_min);
                    if (ny < y_min)
                        ny = y_min;
                    if (ny > y_max)
                        ny = y_max;
                    if (nx < x_min)
                        nx = x_min;
                    if (nx > x_max)
                        nx = x_max;
                    xs[*count] = nx;
                    ys[*count] = ny;
                    ++(*count);
                    /* sort */
                    for (int a = 0; a < *count - 1; ++a)
                        for (int b = a + 1; b < *count; ++b)
                            if (xs[a] > xs[b])
                            {
                                float tx = xs[a];
                                xs[a] = xs[b];
                                xs[b] = tx;
                                tx = ys[a];
                                ys[a] = ys[b];
                                ys[b] = tx;
                            }
                    /* re-find selection index */
                    s_selected = -1;
                    for (int i = 0; i < *count; ++i)
                        if (xs[i] == xs[*count - 1])
                        {
                            s_selected = i;
                            break;
                        }
                    changed = 1;
                }
            }
            static int dragging = 0;
            if (s_selected >= 0 && inside && in->mouse_down && !dragging && in->mouse_clicked)
                dragging = 1;
            if (dragging && in->mouse_down && s_selected >= 0)
            {
                float nx = x_min + (float) (in->mouse_x - x) / (float) area_w * (x_max - x_min);
                float ny =
                    y_min + (float) (area_h - (in->mouse_y - y)) / (float) area_h * (y_max - y_min);
                if (nx < x_min)
                    nx = x_min;
                if (nx > x_max)
                    nx = x_max;
                if (ny < y_min)
                    ny = y_min;
                if (ny > y_max)
                    ny = y_max;
                xs[s_selected] = nx;
                ys[s_selected] = ny;
                /* keep ordering: bubble around selection */
                for (int i = s_selected; i > 0 && xs[i] < xs[i - 1]; --i)
                {
                    float tx = xs[i];
                    xs[i] = xs[i - 1];
                    xs[i - 1] = tx;
                    tx = ys[i];
                    ys[i] = ys[i - 1];
                    ys[i - 1] = tx;
                    s_selected = i - 1;
                }
                for (int i = s_selected; i < *count - 1 && xs[i] > xs[i + 1]; ++i)
                {
                    float tx = xs[i];
                    xs[i] = xs[i + 1];
                    xs[i + 1] = tx;
                    tx = ys[i];
                    ys[i] = ys[i + 1];
                    ys[i + 1] = tx;
                    s_selected = i + 1;
                }
                changed = 1;
            }
            if (dragging && !in->mouse_down)
                dragging = 0;
        }
    }
#endif
    /* Buttons row under editor */
    g_ui.cur_y += area_h + 20; /* advance inside same row cell */
    if (overlay_button("Add Mid") && *count < max_points)
    {
        int ins_after = (s_selected >= 0 ? s_selected : *count - 1);
        int nxt = ins_after + 1;
        if (nxt >= *count)
            nxt = ins_after - 1;
        if (nxt < 0)
            nxt = ins_after;
        float x0 = xs[ins_after], y0 = ys[ins_after];
        float x1 = xs[nxt], y1 = ys[nxt];
        float nx = (x0 + x1) * 0.5f;
        float ny = (y0 + y1) * 0.5f;
        if (*count < max_points)
        {
            xs[*count] = nx;
            ys[*count] = ny;
            ++(*count);
            changed = 1;
            /* sort */
            for (int a = 0; a < *count - 1; ++a)
                for (int b = a + 1; b < *count; ++b)
                    if (xs[a] > xs[b])
                    {
                        float tx = xs[a];
                        xs[a] = xs[b];
                        xs[b] = tx;
                        tx = ys[a];
                        ys[a] = ys[b];
                        ys[b] = tx;
                    }
            /* select inserted (approx locate) */
            for (int i = 0; i < *count; ++i)
                if (xs[i] == nx && ys[i] == ny)
                {
                    s_selected = i;
                    break;
                }
        }
    }
    if (overlay_button("Remove") && s_selected >= 0 && *count > 2)
    {
        for (int i = s_selected; i < *count - 1; ++i)
        {
            xs[i] = xs[i + 1];
            ys[i] = ys[i + 1];
        }
        --(*count);
        if (s_selected >= *count)
            s_selected = *count - 1;
        changed = 1;
    }
    /* Finish layout cell */
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
    (void) id; /* reserved for future focus/tooltip */
    return changed;
}

/* Gradient Editor (design helper v1, non-persistent)
 * stops: array of stop positions [0,1] ascending (size *count)
 * colors: packed RGBA bytes per stop (count*4)
 * Ensures at least 2 stops (0 at index 0 and 1 at last index). Adds default black→white.
 * Interaction (SDL build):
 *  - Click bar to add stop (if capacity) – color cloned from nearest neighbor.
 *  - Click existing stop marker to select.
 *  - Drag selected stop horizontally to reposition (clamped, maintains ordering; endpoints fixed).
 *  - With a stop selected (not endpoints) buttons: Remove. Color editor always shown for selected.
 */
int overlay_gradient_editor(const char* label, float* stops, unsigned char* colors, int* count,
                            int max_stops)
{
    if (!g_ui.panel_active || !stops || !colors || !count || max_stops < 2)
        return 0;
    if (*count < 2)
    {
        stops[0] = 0.f;
        colors[0 * 4 + 0] = 0;
        colors[0 * 4 + 1] = 0;
        colors[0 * 4 + 2] = 0;
        colors[0 * 4 + 3] = 255;
        stops[1] = 1.f;
        colors[1 * 4 + 0] = 255;
        colors[1 * 4 + 1] = 255;
        colors[1 * 4 + 2] = 255;
        colors[1 * 4 + 3] = 255;
        *count = 2;
    }
    int changed = 0;
    int bar_h = 18;
    int w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width) - 4;
    if (w < 80)
        w = 80;
    int x = g_ui.cur_x + 2;
    int y = g_ui.cur_y + 2;
    int id = g_ui.total_widgets++;
    if (g_ui.row_max_h < bar_h + 4 + 40) /* accommodate color edit */
        g_ui.row_max_h = bar_h + 4 + 40;
    {
        char lbl[128];
        snprintf(lbl, sizeof lbl, "%s%s", label ? label : "Gradient", " (click/add drag)");
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(x, y - 14, lbl, 1,
                             (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
    }
    static int s_sel = 0;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        /* Draw gradient bar by coarse sampling */
        for (int px = 0; px < w; ++px)
        {
            float t = (float) px / (float) (w - 1);
            /* find segment */
            int a = 0;
            for (int i = 0; i < *count - 1; ++i)
            {
                if (t >= stops[i] && t <= stops[i + 1])
                {
                    a = i;
                    break;
                }
            }
            int b = a + 1;
            if (b >= *count)
                b = *count - 1;
            float ta = stops[a], tb = stops[b];
            float local = (tb > ta) ? (t - ta) / (tb - ta) : 0.f;
            unsigned char* ca = &colors[a * 4];
            unsigned char* cb = &colors[b * 4];
            unsigned char r = (unsigned char) (ca[0] + (int) (local * (cb[0] - ca[0])));
            unsigned char g = (unsigned char) (ca[1] + (int) (local * (cb[1] - ca[1])));
            unsigned char bcol = (unsigned char) (ca[2] + (int) (local * (cb[2] - ca[2])));
            unsigned char aalpha = (unsigned char) (ca[3] + (int) (local * (cb[3] - ca[3])));
            SDL_SetRenderDrawColor(g_app.renderer, r, g, bcol, aalpha);
            SDL_RenderDrawLine(g_app.renderer, x + px, y, x + px, y + bar_h);
        }
        SDL_Rect br = {x, y, w, bar_h};
        SDL_SetRenderDrawColor(g_app.renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(g_app.renderer, &br);
        /* Stop markers */
        for (int i = 0; i < *count; ++i)
        {
            int px = x + (int) (stops[i] * w);
            SDL_Rect mr = {px - 3, y + bar_h / 2 - 5, 6, 10};
            unsigned char* c = &colors[i * 4];
            SDL_SetRenderDrawColor(g_app.renderer, c[0], c[1], c[2], 255);
            SDL_RenderFillRect(g_app.renderer, &mr);
            SDL_SetRenderDrawColor(g_app.renderer,
                                   (i == s_sel) ? th->accent_2.r : th->panel_border.r,
                                   (i == s_sel) ? th->accent_2.g : th->panel_border.g,
                                   (i == s_sel) ? th->accent_2.b : th->panel_border.b, 255);
            SDL_RenderDrawRect(g_app.renderer, &mr);
        }
        const OverlayInputState* in = overlay_input_get();
        int hover = overlay_mouse_over(x, y, w, bar_h);
        static int dragging = 0;
        if (hover && in->mouse_clicked)
        {
            /* hit test markers */
            int found = -1;
            for (int i = 0; i < *count; ++i)
            {
                int px = x + (int) (stops[i] * w);
                if (in->mouse_x >= px - 4 && in->mouse_x <= px + 4 && in->mouse_y >= y &&
                    in->mouse_y <= y + bar_h)
                {
                    found = i;
                    break;
                }
            }
            if (found >= 0)
            {
                s_sel = found;
                dragging = 1;
            }
            else if (*count < max_stops)
            {
                float t = (float) (in->mouse_x - x) / (float) w;
                if (t < 0)
                    t = 0;
                if (t > 1)
                    t = 1;
                /* clone nearest color */
                int nearest = 0;
                float best = 1e9f;
                for (int i = 0; i < *count; ++i)
                {
                    float d = fabsf(stops[i] - t);
                    if (d < best)
                    {
                        best = d;
                        nearest = i;
                    }
                }
                /* insert */
                stops[*count] = t;
                memcpy(&colors[*count * 4], &colors[nearest * 4], 4);
                ++(*count);
                /* sort by stop */
                for (int a = 0; a < *count - 1; ++a)
                    for (int b = a + 1; b < *count; ++b)
                        if (stops[a] > stops[b])
                        {
                            float ts = stops[a];
                            stops[a] = stops[b];
                            stops[b] = ts;
                            unsigned char tmp[4];
                            memcpy(tmp, &colors[a * 4], 4);
                            memcpy(&colors[a * 4], &colors[b * 4], 4);
                            memcpy(&colors[b * 4], tmp, 4);
                        }
                /* find new index */
                for (int i = 0; i < *count; ++i)
                    if (fabsf(stops[i] - t) < 1e-6f)
                    {
                        s_sel = i;
                        break;
                    }
                dragging = 1;
                changed = 1;
            }
        }
        if (dragging && in->mouse_down)
        {
            if (s_sel > 0 && s_sel < *count - 1) /* lock endpoints */
            {
                float t = (float) (in->mouse_x - x) / (float) w;
                if (t < 0)
                    t = 0;
                if (t > 1)
                    t = 1;
                stops[s_sel] = t;
                /* re-order local */
                for (int i = s_sel; i > 0 && stops[i] < stops[i - 1]; --i)
                {
                    float ts = stops[i];
                    stops[i] = stops[i - 1];
                    stops[i - 1] = ts;
                    unsigned char tmp[4];
                    memcpy(tmp, &colors[i * 4], 4);
                    memcpy(&colors[i * 4], &colors[(i - 1) * 4], 4);
                    memcpy(&colors[(i - 1) * 4], tmp, 4);
                    s_sel = i - 1;
                }
                for (int i = s_sel; i < *count - 1 && stops[i] > stops[i + 1]; ++i)
                {
                    float ts = stops[i];
                    stops[i] = stops[i + 1];
                    stops[i + 1] = ts;
                    unsigned char tmp[4];
                    memcpy(tmp, &colors[i * 4], 4);
                    memcpy(&colors[i * 4], &colors[(i + 1) * 4], 4);
                    memcpy(&colors[(i + 1) * 4], tmp, 4);
                    s_sel = i + 1;
                }
                changed = 1;
            }
        }
        if (dragging && !in->mouse_down)
            dragging = 0;
    }
#endif
    g_ui.cur_y += bar_h + 6;
    if (s_sel >= 0 && s_sel < *count)
    {
        unsigned char* c = &colors[s_sel * 4];
        int col_changed = overlay_color_edit_rgba("Color", c);
        if (col_changed)
            changed = 1;
    }
    if (s_sel > 0 && s_sel < *count - 1)
    {
        if (overlay_button("Remove Stop") && *count > 2)
        {
            for (int i = s_sel; i < *count - 1; ++i)
            {
                stops[i] = stops[i + 1];
                memcpy(&colors[i * 4], &colors[(i + 1) * 4], 4);
            }
            --(*count);
            if (s_sel >= *count)
                s_sel = *count - 1;
            changed = 1;
        }
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
    (void) id;
    return changed;
}

/* Timeline Editor (design helper v1, non-persistent)
 * Blocks represented by parallel arrays: starts_ms[i], durations_ms[i], types[i].
 * Editing operations:
 *   - Click block to select.
 *   - Drag center area to move (clamped) preserving duration.
 *   - Drag near left/right edge (<4px) to resize that edge (maintains min duration).
 *   - Click empty space inside bar to add new block (default 200ms) if capacity.
 *   - Button: Remove Block (if selected) or Add Block (adds at end) below editor.
 * Sorting by start applied each frame; overlaps permitted for now (future constraint).
 */
int overlay_timeline_editor(const char* label, float* starts_ms, float* durations_ms, int* types,
                            int* count, int max_blocks, float total_duration_ms)
{
    if (!g_ui.panel_active || !starts_ms || !durations_ms || !types || !count || max_blocks <= 0 ||
        total_duration_ms <= 0.f)
        return 0;
    const OverlayInputState* in = overlay_input_get();
    int changed = 0;
    int h = 54; /* reserve space */
    int w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width) - 4;
    if (w < 120)
        w = 120;
    int x = g_ui.cur_x + 2;
    int y = g_ui.cur_y + 2;
    int id = g_ui.total_widgets++;
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
    {
        char lbl[128];
        snprintf(lbl, sizeof lbl, "%s%s", label ? label : "Timeline",
                 " (drag move/edges; click add)");
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(x, y - 14, lbl, 1,
                             (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
    }
    /* keep blocks sorted */
    for (int a = 0; a < *count - 1; ++a)
        for (int b = a + 1; b < *count; ++b)
            if (starts_ms[a] > starts_ms[b])
            {
                float ts = starts_ms[a];
                starts_ms[a] = starts_ms[b];
                starts_ms[b] = ts;
                ts = durations_ms[a];
                durations_ms[a] = durations_ms[b];
                durations_ms[b] = ts;
                int ty = types[a];
                types[a] = types[b];
                types[b] = ty;
            }

    /* ensure durations positive */
    for (int i = 0; i < *count; ++i)
        if (durations_ms[i] < 1.f)
            durations_ms[i] = 1.f;

    float px_per_ms = (float) w / total_duration_ms;
    int bar_y = y;
    int bar_h = 40;
    int label_h_off = 0;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect bg = {x, bar_y, w, bar_h};
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r, th->panel_bg.g, th->panel_bg.b,
                               th->panel_bg.a);
        SDL_RenderFillRect(g_app.renderer, &bg);
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                               th->panel_border.b, th->panel_border.a);
        SDL_RenderDrawRect(g_app.renderer, &bg);
    }
#endif
    /* static interaction state */
    static int s_sel = -1;
    static int s_dragging = 0;    /* 0 none, 1 move, 2 resize L, 3 resize R */
    static float s_drag_dx = 0.f; /* offset for move or original edge reference */
    const float edge_px = 4.f;
    const float min_dur = 10.f;

    int hover = overlay_mouse_over(x, bar_y, w, bar_h);
    int hover_block = -1;
    int hover_edge = 0;
    if (hover)
    {
        for (int i = 0; i < *count; ++i)
        {
            float sx = starts_ms[i] * px_per_ms + x;
            float ex = (starts_ms[i] + durations_ms[i]) * px_per_ms + x;
            if (ex < sx + 1)
                ex = sx + 1;
            if (in->mouse_x >= (int) sx && in->mouse_x <= (int) ex && in->mouse_y >= bar_y &&
                in->mouse_y <= bar_y + bar_h)
            {
                hover_block = i;
                if (in->mouse_x < sx + edge_px)
                    hover_edge = -1;
                else if (in->mouse_x > ex - edge_px)
                    hover_edge = 1;
                break;
            }
        }
    }
    if (!s_dragging && in->mouse_clicked && hover)
    {
        if (hover_block >= 0)
        {
            s_sel = hover_block;
            if (hover_edge == -1)
            {
                s_dragging = 2;
                s_drag_dx = (starts_ms[s_sel] + durations_ms[s_sel]);
            }
            else if (hover_edge == 1)
            {
                s_dragging = 3;
            }
            else
            {
                s_dragging = 1;
                s_drag_dx = (float) in->mouse_x - starts_ms[s_sel] * px_per_ms - x;
            }
        }
        else if (*count < max_blocks)
        {
            float click_ms = ((float) in->mouse_x - (float) x) / px_per_ms;
            if (click_ms < 0)
                click_ms = 0;
            if (click_ms > total_duration_ms - 1)
                click_ms = total_duration_ms - 1;
            starts_ms[*count] = click_ms;
            durations_ms[*count] = 200.f;
            if (starts_ms[*count] + durations_ms[*count] > total_duration_ms)
                durations_ms[*count] = total_duration_ms - starts_ms[*count];
            types[*count] = 0;
            s_sel = *count;
            ++(*count);
            changed = 1;
            s_dragging = 1;
            s_drag_dx = (float) in->mouse_x - starts_ms[s_sel] * px_per_ms - x;
        }
    }
    if (s_dragging && in->mouse_down && s_sel >= 0 && s_sel < *count)
    {
        if (s_dragging == 1) /* move */
        {
            float new_start = ((float) in->mouse_x - (float) x - s_drag_dx) / px_per_ms;
            if (new_start < 0)
                new_start = 0;
            if (new_start + durations_ms[s_sel] > total_duration_ms)
                new_start = total_duration_ms - durations_ms[s_sel];
            if (new_start != starts_ms[s_sel])
            {
                starts_ms[s_sel] = new_start;
                changed = 1;
            }
        }
        else if (s_dragging == 2) /* resize left */
        {
            float new_start = ((float) in->mouse_x - (float) x) / px_per_ms;
            if (new_start < 0)
                new_start = 0;
            if (new_start > s_drag_dx - min_dur)
                new_start = s_drag_dx - min_dur; /* s_drag_dx holds original right edge */
            float new_dur = s_drag_dx - new_start;
            if (new_dur < min_dur)
                new_dur = min_dur;
            if (new_start + new_dur > total_duration_ms)
            {
                new_dur = total_duration_ms - new_start;
            }
            if (new_start != starts_ms[s_sel] || new_dur != durations_ms[s_sel])
            {
                starts_ms[s_sel] = new_start;
                durations_ms[s_sel] = new_dur;
                changed = 1;
            }
        }
        else if (s_dragging == 3) /* resize right */
        {
            float right = ((float) in->mouse_x - (float) x) / px_per_ms;
            float new_dur = right - starts_ms[s_sel];
            if (new_dur < min_dur)
                new_dur = min_dur;
            if (starts_ms[s_sel] + new_dur > total_duration_ms)
                new_dur = total_duration_ms - starts_ms[s_sel];
            if (new_dur != durations_ms[s_sel])
            {
                durations_ms[s_sel] = new_dur;
                changed = 1;
            }
        }
    }
    if (s_dragging && !in->mouse_down)
        s_dragging = 0;

#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        for (int i = 0; i < *count; ++i)
        {
            float sx = starts_ms[i] * px_per_ms + x;
            float ex = (starts_ms[i] + durations_ms[i]) * px_per_ms + x;
            if (ex < sx + 1)
                ex = sx + 1;
            SDL_Rect br = {(int) sx, bar_y + 2, (int) (ex - sx), bar_h - 4};
            OverlayColor fill = th->accent_1;
            if (types[i] & 1)
                fill = th->accent_2;
            if (i == s_sel)
            { /* lighten selection */
                fill = shade_color(fill, +25);
            }
            SDL_SetRenderDrawColor(g_app.renderer, fill.r, fill.g, fill.b, fill.a);
            SDL_RenderFillRect(g_app.renderer, &br);
            SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                                   th->panel_border.b, th->panel_border.a);
            SDL_RenderDrawRect(g_app.renderer, &br);
        }
    }
#endif
    g_ui.cur_y += bar_h + 6; /* advance */
    /* Controls row */
    if (s_sel >= 0 && s_sel < *count)
    {
        char info[128];
        snprintf(info, sizeof info, "Block %d: start=%.0fms dur=%.0fms", s_sel, starts_ms[s_sel],
                 durations_ms[s_sel]);
        overlay_label(info);
        if (overlay_button("Remove Block"))
        {
            for (int i = s_sel; i < *count - 1; ++i)
            {
                starts_ms[i] = starts_ms[i + 1];
                durations_ms[i] = durations_ms[i + 1];
                types[i] = types[i + 1];
            }
            --(*count);
            if (s_sel >= *count)
                s_sel = *count - 1;
            changed = 1;
        }
    }
    else
    {
        overlay_label("No block selected");
    }
    if (*count < max_blocks)
    {
        if (overlay_button("Add Block"))
        {
            starts_ms[*count] =
                (*count > 0) ? (starts_ms[*count - 1] + durations_ms[*count - 1]) : 0.f;
            if (starts_ms[*count] > total_duration_ms - 1.f)
                starts_ms[*count] = total_duration_ms - 1.f;
            durations_ms[*count] = 200.f;
            if (starts_ms[*count] + durations_ms[*count] > total_duration_ms)
                durations_ms[*count] = total_duration_ms - starts_ms[*count];
            types[*count] = *count & 1;
            ++(*count);
            changed = 1;
        }
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
    (void) id;
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
