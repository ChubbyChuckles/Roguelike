/* panels_asset_browser_api.h - minimal external hotkey/bookmark API for asset browser
 * Phase 6 slice: exposes lightweight toggles so global input layer can trigger
 * panel feature visibility without reaching into static state directly.
 */
#ifndef ROGUE_PANELS_ASSET_BROWSER_API_H
#define ROGUE_PANELS_ASSET_BROWSER_API_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Hotkey-driven toggles (no-ops if panel not yet initialized). */
    void rogue_asset_browser_toggle_stream_queue(void);
    void rogue_asset_browser_toggle_perf_metrics(void);
    void rogue_asset_browser_toggle_atlas_tool(void);
    void rogue_asset_browser_toggle_compression_compare(void);
    void rogue_asset_browser_toggle_memory_profiler(void);
    /* Bookmark helpers */
    void rogue_asset_browser_add_bookmark_selected(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_PANELS_ASSET_BROWSER_API_H */
