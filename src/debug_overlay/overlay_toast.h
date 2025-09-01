#ifndef ROGUE_DEBUG_OVERLAY_TOAST_H
#define ROGUE_DEBUG_OVERLAY_TOAST_H

#ifdef __cplusplus
extern "C"
{
#endif

    enum OverlayToastKind
    {
        OVERLAY_TOAST_INFO = 0,
        OVERLAY_TOAST_WARN = 1,
        OVERLAY_TOAST_ERROR = 2,
    };

    /* Push a simple toast to appear in top-right; auto-dismiss after ms. */
    void overlay_toast_push(enum OverlayToastKind kind, const char* msg, int duration_ms);
    /* Render all active toasts; called by overlay_render. */
    void overlay_toast_render(void);
    /* Clear all toasts (tests). */
    void overlay_toast_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_DEBUG_OVERLAY_TOAST_H */
