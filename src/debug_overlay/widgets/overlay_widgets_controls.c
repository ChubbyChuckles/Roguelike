#include "overlay_widgets_internal.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

#include "../../core/app/app_state.h"
#include "../../graphics/font.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <stdio.h>
#include <string.h>

void overlay_label(const char* text)
{
    if (!g_ui.panel_active)
        return;
    rogue_font_draw_text(g_ui.cur_x, g_ui.cur_y + 4, text ? text : "", 1,
                         (RogueColor){220, 220, 255, 255});
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
    if (g_ui.row_max_h < h)
        g_ui.row_max_h = h;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        SDL_Rect r = {x, y, w, h};
        int hot = overlay_mouse_over(x, y, w, h);
        SDL_SetRenderDrawColor(g_app.renderer, hot ? 60 : 40, 60, 90, 200);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, 200, 200, 220, 220);
        SDL_RenderDrawRect(g_app.renderer, &r);
    }
#endif
    rogue_font_draw_text(x + 6, y + 3, label ? label : "", 1, (RogueColor){255, 255, 255, 255});
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
    if (g_ui.row_max_h < sz + 4)
        g_ui.row_max_h = sz + 4;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        SDL_Rect box = {x, y, sz, sz};
        SDL_SetRenderDrawColor(g_app.renderer, 30, 30, 30, 200);
        SDL_RenderFillRect(g_app.renderer, &box);
        SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
        SDL_RenderDrawRect(g_app.renderer, &box);
        if (value && *value)
        {
            SDL_Rect inner = {x + 3, y + 3, sz - 6, sz - 6};
            SDL_RenderFillRect(g_app.renderer, &inner);
        }
    }
#endif
    rogue_font_draw_text(x + sz + 6, g_ui.cur_y + 2, label ? label : "", 1,
                         (RogueColor){220, 255, 220, 255});
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
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        SDL_Rect bar = {x, y + 2, w, h};
        SDL_SetRenderDrawColor(g_app.renderer, 30, 30, 30, 200);
        SDL_RenderFillRect(g_app.renderer, &bar);
        SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
        SDL_RenderDrawRect(g_app.renderer, &bar);
    }
#endif
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
    rogue_font_draw_text(x + 6, y + 2, buf, 1, (RogueColor){255, 255, 255, 255});
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
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        SDL_Rect bar = {x, y + 2, w, h};
        SDL_SetRenderDrawColor(g_app.renderer, 30, 30, 30, 200);
        SDL_RenderFillRect(g_app.renderer, &bar);
        SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
        SDL_RenderDrawRect(g_app.renderer, &bar);
    }
#endif
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
    rogue_font_draw_text(x + 6, y + 2, buf, 1, (RogueColor){255, 255, 255, 255});
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
        SDL_Rect r = {x, y + 2, w, h2};
        SDL_SetRenderDrawColor(g_app.renderer, 20, 20, 20, 200);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
        SDL_RenderDrawRect(g_app.renderer, &r);
    }
#endif
    char line[256];
    snprintf(line, sizeof(line), "%s: %s", label ? label : "", buf);
    rogue_font_draw_text(x + 6, y + 2, line, 1, (RogueColor){255, 255, 255, 255});
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
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
    const OverlayInputState* in = overlay_input_get();
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        SDL_Rect r = {x, y + 2, w, h};
        SDL_SetRenderDrawColor(g_app.renderer, 25, 25, 25, 200);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
        SDL_RenderDrawRect(g_app.renderer, &r);
    }
#endif
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
    rogue_font_draw_text(x + 6, y + 2, line, 1, (RogueColor){255, 255, 255, 255});
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

int overlay_tree_node(const char* label, int* open)
{
    if (!g_ui.panel_active || !open)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y,
        w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width), h = 18;
    int id = g_ui.total_widgets++;
    if (g_ui.row_max_h < h + 2)
        g_ui.row_max_h = h + 2;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        SDL_Rect r = {x, y + 2, w, h};
        SDL_SetRenderDrawColor(g_app.renderer, 18, 18, 18, 200);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
        SDL_RenderDrawRect(g_app.renderer, &r);
    }
#endif
    const char* arrow = (*open ? "▾" : "▸");
    char line[256];
    snprintf(line, sizeof line, "%s %s", arrow, label ? label : "");
    rogue_font_draw_text(x + 6, y + 2, line, 1, (RogueColor){220, 255, 220, 255});
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
            SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
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

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
