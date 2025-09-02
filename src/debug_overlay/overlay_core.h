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
/* Visibility & enumeration */
int overlay_get_panel_count(void);
/* Returns 0 on success; outputs id/name pointers and visible flag when non-NULL */
int overlay_get_panel_info(int index, const char** out_id, const char** out_name, int* out_visible);
/* Set or query visibility by index (0 on success). */
int overlay_set_panel_visible_by_index(int index, int visible);
int overlay_get_panel_visible_by_index(int index);
/* Helper by id (returns 0 on success, -1 if not found). */
int overlay_set_panel_visible(const char* id, int visible);
int overlay_get_panel_visible(const char* id);

/* Per-frame */
void overlay_new_frame(float dt, int screen_w, int screen_h);
void overlay_render(void);

/* Optional navigation helpers for panels that support selection APIs. */
void overlay_nav_open_items_and_select(int item_index);
void overlay_nav_open_skills_and_select(int skill_index);

/* Toggle */
void overlay_set_enabled(int enabled);
int overlay_is_enabled(void);
/* Accessors */
float overlay_last_dt(void);
/* Registers a baseline set of panels (system, etc.). Safe to call multiple times. */
void rogue_overlay_register_default_panels(void);

/* Convenience: movable panels with persisted position/visibility
 * Begin a panel using saved (x,y,w) if available, else defaults provided.
 * Adds drag support on the title bar; updates persisted layout on move/toggle.
 */
int overlay_begin_panel_auto(const char* id, const char* title, int default_x, int default_y,
                             int default_w);
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
static inline int overlay_get_panel_count(void) { return 0; }
static inline int overlay_get_panel_info(int index, const char** out_id, const char** out_name,
                                         int* out_visible)
{
    (void) index;
    (void) out_id;
    (void) out_name;
    (void) out_visible;
    return -1;
}
static inline int overlay_set_panel_visible_by_index(int index, int visible)
{
    (void) index;
    (void) visible;
    return -1;
}
static inline int overlay_get_panel_visible_by_index(int index)
{
    (void) index;
    return 0;
}
static inline int overlay_set_panel_visible(const char* id, int visible)
{
    (void) id;
    (void) visible;
    return -1;
}
static inline int overlay_get_panel_visible(const char* id)
{
    (void) id;
    return 0;
}
static inline int overlay_begin_panel_auto(const char* id, const char* title, int default_x,
                                           int default_y, int default_w)
{
    (void) id;
    return overlay_begin_panel(title, default_x, default_y, default_w);
}
#endif

#endif /* ROGUE_DEBUG_OVERLAY_CORE_H */
