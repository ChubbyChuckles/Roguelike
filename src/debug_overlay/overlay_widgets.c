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
    int panel_active;
    OverlayStyle style;
} UiCtx;

static UiCtx g_ui = {0};

static void ui_next_line(void) { g_ui.cur_y += g_ui.line_h; }

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

void overlay_end_panel(void) { g_ui.panel_active = 0; }

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
    ui_next_line();
}

int overlay_button(const char* label)
{
    if (!g_ui.panel_active)
        return 0;
    int h = 20;
    int x = g_ui.cur_x, y = g_ui.cur_y;
    int w = g_ui.width;
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
    int clicked = mouse_over(x, y, w, h) && overlay_input_get()->mouse_clicked;
    if (clicked)
        overlay_input_set_capture(1, 1);
    ui_next_line();
    return clicked;
}

int overlay_checkbox(const char* label, int* value)
{
    if (!g_ui.panel_active)
        return 0;
    int changed = 0;
    int sz = 16;
    int x = g_ui.cur_x, y = g_ui.cur_y + 2;
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
    if (mouse_over(x, y, sz, sz) && overlay_input_get()->mouse_clicked && value)
    {
        *value = !*value;
        changed = 1;
        overlay_input_set_capture(1, 1);
    }
    ui_next_line();
    return changed;
}

int overlay_slider_int(const char* label, int* value, int minv, int maxv)
{
    if (!g_ui.panel_active || !value)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y, w = g_ui.width, h = 18;
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
        float t = (float) (in->mouse_x - x) / (float) w;
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
    ui_next_line();
    return changed;
}

int overlay_slider_float(const char* label, float* value, float minv, float maxv)
{
    if (!g_ui.panel_active || !value)
        return 0;
    int x = g_ui.cur_x, y = g_ui.cur_y, w = g_ui.width, h = 18;
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
        float t = (float) (in->mouse_x - x) / (float) w;
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
    ui_next_line();
    return changed;
}

int overlay_input_text(const char* label, char* buf, size_t buf_size)
{
    if (!g_ui.panel_active || !buf || buf_size == 0)
        return 0;
    const OverlayInputState* in = overlay_input_get();
    int changed = 0;
    /* Backspace */
    if (in->key_backspace_pressed && strlen(buf) > 0)
    {
        buf[strlen(buf) - 1] = '\0';
        changed = 1;
    }
    /* Append incoming text (ASCII slice) */
    if (in->text_input[0] != '\0')
    {
        size_t cur = strlen(buf);
        size_t add = strlen(in->text_input);
        if (cur + add >= buf_size)
            add = buf_size - 1 - cur;
        if (add > 0)
        {
            memcpy(buf + cur, in->text_input, add);
            buf[cur + add] = '\0';
            changed = 1;
        }
    }
    int x = g_ui.cur_x, y = g_ui.cur_y, w = g_ui.width, h = 18;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        SDL_Rect r = {x, y + 2, w, h};
        SDL_SetRenderDrawColor(g_app.renderer, 20, 20, 20, 200);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
        SDL_RenderDrawRect(g_app.renderer, &r);
    }
#endif
    char line[256];
    snprintf(line, sizeof(line), "%s: %s", label ? label : "", buf);
    rogue_font_draw_text(x + 6, y + 2, line, 1, (RogueColor){255, 255, 255, 255});
    if (mouse_over(x, y + 2, w, h) && overlay_input_get()->mouse_clicked)
    {
        overlay_input_set_capture(1, 1);
    }
    ui_next_line();
    return changed;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
