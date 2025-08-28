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
        else if (ev->key.keysym.sym == SDLK_BACKSPACE)
            g_inp.key_backspace_pressed = 1;
        else if (ev->key.keysym.sym == SDLK_RETURN)
            g_inp.key_enter_pressed = 1;
        else if (ev->key.keysym.sym == SDLK_SPACE)
            g_inp.key_space_pressed = 1;
        else if (ev->key.keysym.sym == SDLK_LEFT)
            g_inp.key_left_pressed = 1;
        else if (ev->key.keysym.sym == SDLK_RIGHT)
            g_inp.key_right_pressed = 1;
        else if (ev->key.keysym.sym == SDLK_HOME)
            g_inp.key_home_pressed = 1;
        else if (ev->key.keysym.sym == SDLK_END)
            g_inp.key_end_pressed = 1;
        else if (ev->key.keysym.sym == SDLK_ESCAPE)
            g_inp.key_escape_pressed = 1;
        g_inp.key_shift_down = ((ev->key.keysym.mod & KMOD_SHIFT) != 0);
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

void overlay_input_simulate_key_tab(int shift)
{
    g_inp.key_tab_pressed = 1;
    g_inp.key_shift_down = !!shift;
}
void overlay_input_simulate_key_enter(void) { g_inp.key_enter_pressed = 1; }
void overlay_input_simulate_key_space(void) { g_inp.key_space_pressed = 1; }
void overlay_input_simulate_key_backspace(void) { g_inp.key_backspace_pressed = 1; }
void overlay_input_simulate_key_left(void) { g_inp.key_left_pressed = 1; }
void overlay_input_simulate_key_right(void) { g_inp.key_right_pressed = 1; }
void overlay_input_simulate_key_home(void) { g_inp.key_home_pressed = 1; }
void overlay_input_simulate_key_end(void) { g_inp.key_end_pressed = 1; }

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
