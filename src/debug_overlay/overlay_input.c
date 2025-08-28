#include "overlay_input.h"
#include "overlay_core.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <stddef.h>
#include <string.h>

static OverlayInputState g_inp;

static void overlay_str_append_clamped(char* dst, size_t cap, const char* src)
{
    if (!dst || !src || cap == 0)
        return;
    size_t cur = strlen(dst);
    if (cur >= cap - 1)
        return; /* already full */
    size_t avail = cap - 1 - cur;
    size_t i = 0;
    while (src[i] && i < avail)
    {
        dst[cur + i] = src[i];
        ++i;
    }
    dst[cur + i] = '\0';
}

void overlay_input_begin_frame(void)
{
    /* reset per-frame bits, keep capture wishes */
    int want_mouse = g_inp.want_capture_mouse;
    int want_kb = g_inp.want_capture_keyboard;
    memset(&g_inp, 0, sizeof(g_inp));
    g_inp.want_capture_mouse = want_mouse;
    g_inp.want_capture_keyboard = want_kb;
}

void overlay_input_handle_event(const void* evptr)
{
    (void) evptr;
#ifdef ROGUE_HAVE_SDL
    const SDL_Event* ev = (const SDL_Event*) evptr;
    if (!ev)
        return;
    if (!overlay_is_enabled())
        return;
    switch (ev->type)
    {
    case SDL_MOUSEMOTION:
        g_inp.mouse_x = ev->motion.x;
        g_inp.mouse_y = ev->motion.y;
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (ev->button.button == SDL_BUTTON_LEFT)
        {
            g_inp.mouse_down = 1;
            g_inp.mouse_clicked = 1;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (ev->button.button == SDL_BUTTON_LEFT)
        {
            g_inp.mouse_down = 0;
        }
        break;
    case SDL_KEYDOWN:
        if (ev->key.keysym.sym == SDLK_TAB)
            g_inp.key_tab_pressed = 1;
        if (ev->key.keysym.sym == SDLK_BACKSPACE)
            g_inp.key_backspace_pressed = 1;
        break;
    case SDL_TEXTINPUT:
        /* copy small chunk of text */
        overlay_str_append_clamped(g_inp.text_input, sizeof(g_inp.text_input), ev->text.text);
        break;
    default:
        break;
    }
#endif
}

int overlay_input_want_capture_mouse(void)
{
    return overlay_is_enabled() ? g_inp.want_capture_mouse : 0;
}
int overlay_input_want_capture_keyboard(void)
{
    return overlay_is_enabled() ? g_inp.want_capture_keyboard : 0;
}

const OverlayInputState* overlay_input_get(void) { return &g_inp; }

void overlay_input_simulate_mouse(int x, int y, int down, int clicked)
{
    g_inp.mouse_x = x;
    g_inp.mouse_y = y;
    g_inp.mouse_down = !!down;
    g_inp.mouse_clicked = !!clicked;
}

void overlay_input_simulate_text(const char* utf8)
{
    if (!utf8)
        return;
    overlay_str_append_clamped(g_inp.text_input, sizeof(g_inp.text_input), utf8);
}

void overlay_input_set_capture(int want_mouse, int want_keyboard)
{
    g_inp.want_capture_mouse = !!want_mouse;
    g_inp.want_capture_keyboard = !!want_keyboard;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
