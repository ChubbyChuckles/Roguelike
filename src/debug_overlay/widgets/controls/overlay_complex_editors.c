/* Reformatted complex editors (curve, gradient, timeline) */
#include "../../overlay_input.h"
#include "../../overlay_theme.h"
#include "../../overlay_tooltip.h"
#include "../overlay_widgets_internal.h"
#include "controls_shared.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../../../core/app/app_state.h"
#include "../../../graphics/font.h"

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
    int area_w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width) - 4;
    if (area_w < 50)
        area_w = 50;
    int area_h = 120;
    int x = g_ui.cur_x + 2;
    int y = g_ui.cur_y + 18;
    if (g_ui.row_max_h < area_h + 20)
        g_ui.row_max_h = area_h + 20;
    int id = g_ui.total_widgets++;
    {
        char lbl[128];
        snprintf(lbl, sizeof lbl, "%s%s", label ? label : "Curve", " (click to add / drag)");
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(g_ui.cur_x + 2, g_ui.cur_y + 2, lbl, 1,
                             (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
    }
    static int s_selected = -1;
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
    g_ui.cur_y += area_h + 20;
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
    if (g_ui.columns > 1)
    {
        overlay_next_column();
        if (g_ui.col_index == 0)
            ui_next_line();
    }
    else
        ui_next_line();
    (void) id;
    return changed;
}

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
    if (g_ui.row_max_h < bar_h + 4 + 40)
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
        for (int px = 0; px < w; ++px)
        {
            float t = (float) px / (float) (w - 1);
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
            unsigned char bb = (unsigned char) (ca[2] + (int) (local * (cb[2] - ca[2])));
            unsigned char aalpha = (unsigned char) (ca[3] + (int) (local * (cb[3] - ca[3])));
            SDL_SetRenderDrawColor(g_app.renderer, r, g, bb, aalpha);
            SDL_RenderDrawLine(g_app.renderer, x + px, y, x + px, y + bar_h);
        }
        SDL_Rect br = {x, y, w, bar_h};
        SDL_SetRenderDrawColor(g_app.renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(g_app.renderer, &br);
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
                stops[*count] = t;
                memcpy(&colors[*count * 4], &colors[nearest * 4], 4);
                ++(*count);
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
            if (s_sel > 0 && s_sel < *count - 1)
            {
                float t = (float) (in->mouse_x - x) / (float) w;
                if (t < 0)
                    t = 0;
                if (t > 1)
                    t = 1;
                stops[s_sel] = t;
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
        ui_next_line();
    (void) id;
    return changed;
}

int overlay_timeline_editor(const char* label, float* starts_ms, float* durations_ms, int* types,
                            int* count, int max_blocks, float total_duration_ms)
{
    if (!g_ui.panel_active || !starts_ms || !durations_ms || !types || !count || max_blocks <= 0 ||
        total_duration_ms <= 0.f)
        return 0;
    const OverlayInputState* in = overlay_input_get();
    int changed = 0;
    int h = 54;
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
    for (int i = 0; i < *count; ++i)
        if (durations_ms[i] < 1.f)
            durations_ms[i] = 1.f;
    float px_per_ms = (float) w / total_duration_ms;
    int bar_y = y;
    int bar_h = 40;
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
    static int s_sel = -1;
    static int s_dragging = 0;
    static float s_drag_dx = 0.f;
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
        if (s_dragging == 1)
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
        else if (s_dragging == 2)
        {
            float new_start = ((float) in->mouse_x - (float) x) / px_per_ms;
            if (new_start < 0)
                new_start = 0;
            if (new_start > s_drag_dx - min_dur)
                new_start = s_drag_dx - min_dur;
            float new_dur = s_drag_dx - new_start;
            if (new_dur < min_dur)
                new_dur = min_dur;
            if (new_start + new_dur > total_duration_ms)
                new_dur = total_duration_ms - new_start;
            if (new_start != starts_ms[s_sel] || new_dur != durations_ms[s_sel])
            {
                starts_ms[s_sel] = new_start;
                durations_ms[s_sel] = new_dur;
                changed = 1;
            }
        }
        else if (s_dragging == 3)
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
                fill = shade_color(fill, +25);
            SDL_SetRenderDrawColor(g_app.renderer, fill.r, fill.g, fill.b, fill.a);
            SDL_RenderFillRect(g_app.renderer, &br);
            SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                                   th->panel_border.b, th->panel_border.a);
            SDL_RenderDrawRect(g_app.renderer, &br);
        }
    }
#endif
    g_ui.cur_y += bar_h + 6;
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
        ui_next_line();
    (void) id;
    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
