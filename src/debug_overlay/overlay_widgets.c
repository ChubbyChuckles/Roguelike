#include "overlay_widgets.h"
#include "overlay_core.h"
#include "overlay_input.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

#include "../core/app/app_state.h"
#include "../graphics/font.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <string.h>

typedef struct UiCtx
{
    int cur_x, cur_y, width;
    int line_h;
    int row_max_h; /* tallest widget height in the current row (for columns auto-wrap) */
    int panel_active;
    OverlayStyle style;
    /* Columns */
    int columns;
    int col_widths[4];
    int col_x0[4];
    int col_index;
    int row_start_y;
    /* Focus & widgets */
    int focus_index;   /* -1 = none */
    int total_widgets; /* count of interactive widgets this frame */
    int first_focus;   /* remembered first focusable index for Shift+Tab wrap */
    int last_focus;    /* last focusable index this frame */
    /* InputText caret position for focused field (single shared for simplicity) */
    int caret_pos;
} UiCtx;

static UiCtx g_ui = {.focus_index = -1};

static void ui_next_line(void)
{
    /* Within a columns block, vertical advance is controlled by columns_end */
    if (g_ui.columns <= 1)
    {
        int step = g_ui.row_max_h > 0 ? g_ui.row_max_h : g_ui.line_h;
        g_ui.cur_y += step;
        g_ui.row_max_h = g_ui.line_h; /* reset to default for next line */
    }
}

void overlay_style_set(OverlayStyle s) { g_ui.style = s; }

int overlay_begin_panel(const char* title, int x, int y, int w)
{
    if (!overlay_is_enabled())
        return 0;
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
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        SDL_Rect panel = {x, y, w, 200};
        SDL_SetRenderDrawColor(g_app.renderer, 10, 10, 10, 160);
        SDL_RenderFillRect(g_app.renderer, &panel);
        SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 200);
        SDL_RenderDrawRect(g_app.renderer, &panel);
    }
#endif
    if (title)
        rogue_font_draw_text(x + 6, y + 6, title, 1, (RogueColor){255, 255, 210, 255});
    return 1;
}

void overlay_end_panel(void)
{
    /* Handle Tab to advance focus after building list */
    const OverlayInputState* in = overlay_input_get();
    if (g_ui.total_widgets > 0)
    {
        if (g_ui.focus_index < 0 && in->key_tab_pressed)
        {
            g_ui.focus_index = 0;
            g_ui.caret_pos = 0;
            /* ensure overlay owns input while navigating */
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

static int mouse_over(int x, int y, int w, int h)
{
    const OverlayInputState* in = overlay_input_get();
    return (in->mouse_x >= x && in->mouse_x < x + w && in->mouse_y >= y && in->mouse_y < y + h);
}

void overlay_label(const char* text)
{
    if (!g_ui.panel_active)
        return;
    rogue_font_draw_text(g_ui.cur_x, g_ui.cur_y + 4, text ? text : "", 1,
                         (RogueColor){220, 220, 255, 255});
    /* label visual height ~18-20px; record for row sizing */
    if (g_ui.row_max_h < 20)
        g_ui.row_max_h = 20;
    /* Auto-advance within columns: move to next column; wrap to new row after last */
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
        int hot = mouse_over(x, y, w, h);
        SDL_SetRenderDrawColor(g_app.renderer, hot ? 60 : 40, 60, 90, 200);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, 200, 200, 220, 220);
        SDL_RenderDrawRect(g_app.renderer, &r);
    }
#endif
    rogue_font_draw_text(x + 6, y + 3, label ? label : "", 1, (RogueColor){255, 255, 255, 255});
    const OverlayInputState* in = overlay_input_get();
    if (mouse_over(x, y, w, h) && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    int clicked = (mouse_over(x, y, w, h) && in->mouse_clicked) ||
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
    if (mouse_over(x, y, sz, sz) && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    if (((mouse_over(x, y, sz, sz) && in->mouse_clicked) ||
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
    /* Simple discrete slider: click sets value proportionally. */
    const OverlayInputState* in = overlay_input_get();
    int changed = 0;
    if (mouse_over(x, y + 2, w, h) && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    if ((mouse_over(x, y + 2, w, h) && in->mouse_clicked) ||
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
    if (mouse_over(x, y + 2, w, h) && in->mouse_clicked)
    {
        g_ui.focus_index = id;
        overlay_input_set_capture(1, 1);
    }
    if ((mouse_over(x, y + 2, w, h) && in->mouse_clicked) ||
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
    snprintf(buf, sizeof(buf), "%s: %.3f", label ? label : "", value ? *value : 0.0f);
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
    /* Compute geometry early so we can resolve focus before handling input */
    int x = g_ui.cur_x, y = g_ui.cur_y,
        w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width), h2 = h;
    int will_focus = mouse_over(x, y + 2, w, h2) && in->mouse_clicked;
    if (will_focus)
    {
        /* Acquire focus immediately so text/backspace this frame apply after click */
        overlay_input_set_capture(1, 1);
        g_ui.focus_index = id;
        g_ui.caret_pos = (int) strlen(buf);
    }
    int has_focus = (g_ui.focus_index == id);
    /* Pre-caret navigation that should affect where edits land this frame */
    if (has_focus)
    {
        size_t len = strlen(buf);
        if (in->key_home_pressed)
        {
            g_ui.caret_pos = 0;
        }
        if (in->key_end_pressed)
            g_ui.caret_pos = (int) len;
        if (g_ui.caret_pos < 0)
            g_ui.caret_pos = 0;
        if (g_ui.caret_pos > (int) len)
            g_ui.caret_pos = (int) len;
    }
    /* Backspace */
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
    /* Append incoming text (ASCII slice) */
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
    if (mouse_over(x, y + 2, w, h2) && overlay_input_get()->mouse_clicked)
    {
        /* already handled above; keep for idempotence */
        overlay_input_set_capture(1, 1);
        g_ui.focus_index = id;
        g_ui.caret_pos = (int) strlen(buf);
    }
    /* caret navigation */
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
        g_ui.col_x0[i] = g_ui.col_x0[i - 1] + g_ui.col_widths[i - 1] + 8; /* gutter */
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
        g_ui.row_max_h = g_ui.line_h; /* reset for next row */
    }
    g_ui.cur_x = g_ui.col_x0[g_ui.col_index];
    g_ui.cur_y = g_ui.row_start_y;
}

void overlay_columns_end(void)
{
    if (!g_ui.panel_active)
        return;
    /* move down one line after columns block */
    g_ui.cur_x = g_ui.col_x0[0];
    g_ui.row_start_y += (g_ui.row_max_h > 0 ? g_ui.row_max_h : g_ui.line_h);
    g_ui.cur_y = g_ui.row_start_y;
    g_ui.columns = 1;
    g_ui.col_index = 0;
    g_ui.row_max_h = g_ui.line_h;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
