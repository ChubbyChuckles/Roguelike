/* Reformatted text input control */
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
    int x = g_ui.cur_x, y = g_ui.cur_y;
    int w = (g_ui.columns > 1 ? g_ui.col_widths[g_ui.col_index] : g_ui.width);

    int will_focus = overlay_mouse_over(x, y + 2, w, h) && in->mouse_clicked;
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
        SDL_Rect r = {x, y + 2, w, h};
        SDL_SetRenderDrawColor(g_app.renderer, th->input_bg.r, th->input_bg.g, th->input_bg.b,
                               th->input_bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->input_border.r, th->input_border.g,
                               th->input_border.b, th->input_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
        if (has_focus)
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
    char line[256];
    snprintf(line, sizeof line, "%s: %s", label ? label : "", buf);
    {
        const OverlayTheme* th = overlay_theme_get();
        rogue_font_draw_text(
            x + 6, y + 2, line, 1,
            (RogueColor){th->input_text.r, th->input_text.g, th->input_text.b, th->input_text.a});
    }
    if (overlay_mouse_over(x, y + 2, w, h) && overlay_input_get()->mouse_clicked)
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
        ui_next_line();
    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
