#ifndef ROGUE_DEBUG_OVERLAY_CORE_H
#define ROGUE_DEBUG_OVERLAY_CORE_H

#include <stddef.h>

typedef struct OverlayPanel
{
    const char* id;         /* stable key */
    const char* name;       /* display name */
    void (*fn)(void* user); /* render callback; immediate-mode style */
    void* user;             /* user data */
} OverlayPanel;

#if ROGUE_ENABLE_DEBUG_OVERLAY
/* Lifecycle (no-op for now; kept for future extensions) */
void overlay_init(void);
void overlay_shutdown(void);

/* Panels */
int overlay_register_panel(const char* id, const char* name, void (*fn)(void*), void* user);

/* Per-frame */
void overlay_new_frame(float dt, int screen_w, int screen_h);
void overlay_render(void);

/* Toggle */
void overlay_set_enabled(int enabled);
int overlay_is_enabled(void);
/* Registers a baseline set of panels (system, etc.). Safe to call multiple times. */
void rogue_overlay_register_default_panels(void);
#else
/* Stubs when overlay is disabled at compile time */
static inline void overlay_init(void) {}
static inline void overlay_shutdown(void) {}
static inline int overlay_register_panel(const char* id, const char* name, void (*fn)(void*),
                                         void* user)
{
    (void) id;
    (void) name;
    (void) fn;
    (void) user;
    return -1;
}
static inline void overlay_new_frame(float dt, int screen_w, int screen_h)
{
    (void) dt;
    (void) screen_w;
    (void) screen_h;
}
static inline void overlay_render(void) {}
static inline void overlay_set_enabled(int enabled) { (void) enabled; }
static inline int overlay_is_enabled(void) { return 0; }
#endif

#endif /* ROGUE_DEBUG_OVERLAY_CORE_H */
