#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ROGUE_ENABLE_DEBUG_OVERLAY
#define ROGUE_ENABLE_DEBUG_OVERLAY 0
#endif

    /* Minimal bitmap icon set for the Debug Overlay. Headless-safe (SDL-guarded). */

    typedef enum OverlayIcon
    {
        OVERLAY_ICON_SAVE = 0,
        OVERLAY_ICON_UNDO = 1,
        OVERLAY_ICON_REDO = 2,
        OVERLAY_ICON_PLAY = 3,
        OVERLAY_ICON_SEARCH = 4,
        OVERLAY_ICON_COUNT
    } OverlayIcon;

#if ROGUE_ENABLE_DEBUG_OVERLAY

    /* Draws a small monochrome icon at (x,y). scale>=1 scales pixel size. Color from theme. */
    void overlay_icon_draw(OverlayIcon icon, int x, int y, int scale);

#else

static inline void overlay_icon_draw(OverlayIcon icon, int x, int y, int scale)
{
    (void) icon;
    (void) x;
    (void) y;
    (void) scale;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */

#ifdef __cplusplus
}
#endif
