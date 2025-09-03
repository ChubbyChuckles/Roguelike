#pragma once

#include "overlay_input.h"
#include "overlay_theme.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

#ifdef __cplusplus
extern "C"
{
#endif

    /* Call at the start of each overlay frame to reset per-frame state */
    void overlay_tooltip_new_frame(void);
    /* Track a hover region for the given widget id; if hovered long enough, show tooltip */
    void overlay_tooltip_track(int id, int x, int y, int w, int h, const char* text);
    /* Render active tooltip, if any */
    void overlay_tooltip_render(void);

#ifdef __cplusplus
}
#endif

#else

static inline void overlay_tooltip_new_frame(void) {}
static inline void overlay_tooltip_track(int id, int x, int y, int w, int h, const char* text)
{
    (void) id;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    (void) text;
}
static inline void overlay_tooltip_render(void) {}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
