#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ROGUE_ENABLE_DEBUG_OVERLAY
#define ROGUE_ENABLE_DEBUG_OVERLAY 0
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY

#include <stddef.h>

    /* Minimal per-frame input state for the debug overlay. */
    typedef struct OverlayInputState
    {
        int mouse_x;
        int mouse_y;
        int mouse_down;    /* 1 while pressed */
        int mouse_clicked; /* 1 exactly on the frame a press began */

        int key_tab_pressed;       /* 1 if Tab pressed this frame */
        int key_backspace_pressed; /* for InputText */

        int want_capture_mouse;
        int want_capture_keyboard;

        /* Text input buffered this frame (UTF-8 truncated to ASCII for simplicity). */
        char text_input[64];
    } OverlayInputState;

    /* Called once per frame before event polling. Resets per-frame bits. */
    void overlay_input_begin_frame(void);

    /* Feed an SDL event (no-op if SDL headers are not present). */
    void overlay_input_handle_event(const void* sdl_event);

    /* Query whether overlay wants to capture inputs (when overlay is enabled). */
    int overlay_input_want_capture_mouse(void);
    int overlay_input_want_capture_keyboard(void);

    /* Expose readonly snapshot for widgets. */
    const OverlayInputState* overlay_input_get(void);

    /* Testing helpers (also useful for headless environments). */
    void overlay_input_simulate_mouse(int x, int y, int down, int clicked);
    void overlay_input_simulate_text(const char* utf8);
    void overlay_input_set_capture(int want_mouse, int want_keyboard);

#else /* ROGUE_ENABLE_DEBUG_OVERLAY */

static inline void overlay_input_begin_frame(void) {}
static inline void overlay_input_handle_event(const void* sdl_event) { (void) sdl_event; }
static inline int overlay_input_want_capture_mouse(void) { return 0; }
static inline int overlay_input_want_capture_keyboard(void) { return 0; }
static inline const void* overlay_input_get(void) { return 0; }
static inline void overlay_input_simulate_mouse(int x, int y, int down, int clicked)
{
    (void) x;
    (void) y;
    (void) down;
    (void) clicked;
}
static inline void overlay_input_simulate_text(const char* utf8) { (void) utf8; }
static inline void overlay_input_set_capture(int want_mouse, int want_keyboard)
{
    (void) want_mouse;
    (void) want_keyboard;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */

#ifdef __cplusplus
}
#endif
