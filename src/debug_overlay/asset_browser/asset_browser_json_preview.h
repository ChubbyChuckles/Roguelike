/* JSON preview renderer (lightweight syntax coloration for first N lines). */
#ifndef ROGUE_ASSET_BROWSER_JSON_PREVIEW_H
#define ROGUE_ASSET_BROWSER_JSON_PREVIEW_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Draw a lightweight syntax highlighted preview of the supplied JSON buffer. */
    void rogue_asset_browser_json_draw_preview(const char* buffer);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_BROWSER_JSON_PREVIEW_H */
