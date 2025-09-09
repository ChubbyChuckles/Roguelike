/**
 * @file overlay_icon.h
 * @brief Bitmap icon system for the debug overlay.
 *
 * Provides a minimal bitmap icon set for use within the debug overlay system.
 * Icons are rendered as monochrome bitmaps and support scaling. The system
 * is headless-safe and SDL-guarded, with no-op implementations when the debug
 * overlay is disabled.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ROGUE_ENABLE_DEBUG_OVERLAY
#define ROGUE_ENABLE_DEBUG_OVERLAY 0
#endif

    /**
     * @brief Available icon types for the debug overlay.
     *
     * Enumeration of built-in monochrome bitmap icons that can be rendered
     * within the debug overlay system. Each icon is designed as a small
     * pixel-perfect bitmap suitable for UI elements.
     */
    typedef enum OverlayIcon
    {
        OVERLAY_ICON_SAVE = 0,   ///< Save/disk icon
        OVERLAY_ICON_UNDO = 1,   ///< Undo/curved arrow left icon
        OVERLAY_ICON_REDO = 2,   ///< Redo/curved arrow right icon
        OVERLAY_ICON_PLAY = 3,   ///< Play/triangle icon
        OVERLAY_ICON_SEARCH = 4, ///< Search/magnifying glass icon
        OVERLAY_ICON_COUNT       ///< Total number of available icons
    } OverlayIcon;

#if ROGUE_ENABLE_DEBUG_OVERLAY

    /**
     * @brief Draw a monochrome bitmap icon.
     *
     * Renders the specified icon at the given coordinates with scaling support.
     * The icon color is determined by the current debug overlay theme.
     * Only functional when ROGUE_ENABLE_DEBUG_OVERLAY is enabled.
     *
     * @param icon The icon type to draw
     * @param x X coordinate for the top-left corner of the icon
     * @param y Y coordinate for the top-left corner of the icon
     * @param scale Scaling factor (minimum 1, scales pixel size)
     */
    void overlay_icon_draw(OverlayIcon icon, int x, int y, int scale);

#else

    /**
     * @brief No-op implementation when debug overlay is disabled.
     *
     * Inline stub function that does nothing when debug overlay support
     * is not compiled in. Prevents link errors while maintaining API
     * compatibility.
     *
     * @param icon Unused parameter
     * @param x Unused parameter
     * @param y Unused parameter
     * @param scale Unused parameter
     */
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
