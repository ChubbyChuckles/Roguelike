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
        int mouse_down;       /* 1 while pressed */
        int mouse_clicked;    /* 1 exactly on the frame a press began */
        int mouse_wheel_y;    /* +N scrolled up, -N scrolled down (per frame, resets each frame) */
        int mouse_right_down; /* Right button 1 while pressed */
        int mouse_right_clicked; /* Right button: 1 exactly on the frame a press began */

        int key_tab_pressed;       /* 1 if Tab pressed this frame */
        int key_backspace_pressed; /* for InputText */
        int key_shift_down;        /* 1 if Shift held on this frame */
        int key_enter_pressed;
        int key_space_pressed;
        int key_left_pressed;
        int key_right_pressed;
        int key_up_pressed;
        int key_down_pressed;
        int key_home_pressed;
        int key_end_pressed;
        int key_escape_pressed;
        int key_ctrl_down;
        int key_alt_down;
        int key_p_pressed;
        int key_k_pressed;
        int key_f_pressed;        /* Fit-to-selection (Content Graph) */
        int key_question_pressed; /* '?' toggled via Shift + '/' */
        int key_s_pressed;        /* For Ctrl+S save */
        int key_z_pressed;        /* For Ctrl+Z undo */
        int key_y_pressed;        /* For Ctrl+Y redo */
        int key_1_pressed;        /* Alt+1..9 panel switching */
        int key_2_pressed;
        int key_3_pressed;
        int key_4_pressed;
        int key_5_pressed;
        int key_6_pressed;
        int key_7_pressed;
        int key_8_pressed;
        int key_9_pressed;

        int want_capture_mouse;
        int want_capture_keyboard;

        /* Text input buffered this frame (UTF-8 truncated to ASCII for simplicity). */
        char text_input[64];
    } OverlayInputState; /* exposes both typedef and struct tag name */

    /* Called once per frame before event polling. Resets per-frame bits. */
    void overlay_input_begin_frame(void);

    /* Feed an SDL event (no-op if SDL headers are not present). */
    void overlay_input_handle_event(const void* sdl_event);

    /* Query whether overlay wants to capture inputs (when overlay is enabled). */
    int overlay_input_want_capture_mouse(void);
    int overlay_input_want_capture_keyboard(void);

    /* Expose readonly snapshot for widgets. */
    const struct OverlayInputState* overlay_input_get(void);

    /* Testing helpers (also useful for headless environments). */
    void overlay_input_simulate_mouse(int x, int y, int down, int clicked);
    void overlay_input_simulate_text(const char* utf8);
    void overlay_input_set_capture(int want_mouse, int want_keyboard);

    /* Key simulation helpers for unit tests/headless */
    void overlay_input_simulate_key_tab(int shift);
    void overlay_input_simulate_key_enter(void);
    void overlay_input_simulate_key_space(void);
    void overlay_input_simulate_key_backspace(void);
    void overlay_input_simulate_key_left(void);
    void overlay_input_simulate_key_right(void);
    void overlay_input_simulate_key_home(void);
    void overlay_input_simulate_key_end(void);

#else /* ROGUE_ENABLE_DEBUG_OVERLAY */

static inline void overlay_input_begin_frame(void) {}
static inline void overlay_input_handle_event(const void* sdl_event) { (void) sdl_event; }
static inline int overlay_input_want_capture_mouse(void) { return 0; }
static inline int overlay_input_want_capture_keyboard(void) { return 0; }
/* Return type uses struct tag to allow forward-declare in panels without full header */
static inline const struct OverlayInputState* overlay_input_get(void) { return 0; }
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
