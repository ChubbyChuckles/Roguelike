/**
 * @file overlay_toast.h
 * @brief Toast notification system for the debug overlay.
 *
 * Provides a simple toast notification system that displays temporary messages
 * in the top-right corner of the debug overlay. Supports different message types
 * (info, warning, error) with automatic dismissal after a specified duration.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#ifndef ROGUE_DEBUG_OVERLAY_TOAST_H
#define ROGUE_DEBUG_OVERLAY_TOAST_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Toast notification types for the debug overlay.
     *
     * Defines the different types of toast notifications that can be displayed,
     * each with distinct visual styling to indicate message severity.
     */
    enum OverlayToastKind
    {
        OVERLAY_TOAST_INFO = 0,  ///< Informational message (neutral styling)
        OVERLAY_TOAST_WARN = 1,  ///< Warning message (yellow/amber styling)
        OVERLAY_TOAST_ERROR = 2, ///< Error message (red styling)
    };

    /**
     * @brief Push a toast notification to the display queue.
     *
     * Creates and displays a toast notification that appears in the top-right
     * corner of the debug overlay. The toast will automatically dismiss after
     * the specified duration.
     *
     * @param kind The type of toast notification (info, warning, error)
     * @param msg The message text to display (null-terminated string)
     * @param duration_ms Duration in milliseconds before auto-dismissal
     */
    void overlay_toast_push(enum OverlayToastKind kind, const char* msg, int duration_ms);
    
    /**
     * @brief Render all active toast notifications.
     *
     * Renders all currently active toast notifications to the screen.
     * This function is called internally by overlay_render() during the
     * debug overlay rendering cycle.
     */
    void overlay_toast_render(void);
    
    /**
     * @brief Clear all active toast notifications.
     *
     * Immediately removes all toast notifications from the display queue.
     * Primarily used for testing purposes to reset the toast state.
     */
    void overlay_toast_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_DEBUG_OVERLAY_TOAST_H */
