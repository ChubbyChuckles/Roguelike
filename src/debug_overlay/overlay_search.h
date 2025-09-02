#ifndef ROGUE_DEBUG_OVERLAY_SEARCH_H
#define ROGUE_DEBUG_OVERLAY_SEARCH_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ROGUE_ENABLE_DEBUG_OVERLAY
#define ROGUE_ENABLE_DEBUG_OVERLAY 0
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY
    void overlay_search_toggle(int open);
    void overlay_search_render(void);
#else
static inline void overlay_search_toggle(int open) { (void) open; }
static inline void overlay_search_render(void) {}
#endif

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_DEBUG_OVERLAY_SEARCH_H */
