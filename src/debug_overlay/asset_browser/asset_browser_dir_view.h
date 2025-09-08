/* asset_browser_dir_view.h - extracted directory browser rendering for Asset Browser panel */
#ifndef ROGUE_DEBUG_OVERLAY_ASSET_BROWSER_DIR_VIEW_H
#define ROGUE_DEBUG_OVERLAY_ASSET_BROWSER_DIR_VIEW_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Draw (and handle interaction for) the lightweight directory browser inside the
       Asset Browser panel. Maintains and mutates g_ab_state (asset_browser_state). */
    void rogue_asset_browser_draw_directory_browser(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_DEBUG_OVERLAY_ASSET_BROWSER_DIR_VIEW_H */
