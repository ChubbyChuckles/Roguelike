/* Reformatted slider controls */
#include "../../overlay_input.h"
#include "../../overlay_theme.h"
#include "../../overlay_tooltip.h"
#include "../overlay_widgets_internal.h"
#include "controls_shared.h"
#include <stdio.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../../../core/app/app_state.h"
#include "../../../graphics/font.h"

static void draw_slider_bg(int id, int x, int y, int w, int h)
{
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect bar = {x, y, w, h};
        int hover = overlay_mouse_over(x, y, w, h);
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
            SDL_Rect fr = {x + 1, y + 1, w - 2, h - 2};
            SDL_SetRenderDrawColor(g_app.renderer, th->accent_2.r, th->accent_2.g, th->accent_2.b,
                                   th->accent_2.a);
            SDL_RenderDrawRect(g_app.renderer, &fr);
        }
    }
#else
    (void) id;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
#endif
}

int overlay_slider_int(const char* label, int* value, int minv, int maxv)
{
    if (!g_ui.panel_active || !value)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y;
    int w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width);
    int h = 18;
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
    draw_slider_bg(id, x, y + 2, w, h);
    if (tip)
        overlay_tooltip_track(id, x, y + 2, w, h, tip);
    const OverlayInputState* in = overlay_input_get();
    int changed = 0;
    int over = overlay_mouse_over(x, y + 2, w, h);
    if (over && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    if ((over && in->mouse_clicked) ||
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
    snprintf(buf, sizeof buf, "%s: %d", label ? label : "", *value);
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
        ui_next_line();
    return changed;
}

int overlay_slider_float(const char* label, float* value, float minv, float maxv)
{
    if (!g_ui.panel_active || !value)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y;
    int w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width);
    int h = 18;
    int id = g_ui.total_widgets++;
    const char* tip = g_ui.next_tooltip;
    g_ui.next_tooltip = NULL;
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
    draw_slider_bg(id, x, y + 2, w, h);
    if (tip)
        overlay_tooltip_track(id, x, y + 2, w, h, tip);
    const OverlayInputState* in = overlay_input_get();
    int changed = 0;
    int over = overlay_mouse_over(x, y + 2, w, h);
    if (over && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    if ((over && in->mouse_clicked) ||
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
    snprintf(buf, sizeof buf, "%s: %.3f", label ? label : "", *value);
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
        ui_next_line();
    return changed;
}

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
            mx = mn;
        changed = 1;
    }
    if (overlay_slider_int(lbl_max, &mx, abs_min, abs_max))
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
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
