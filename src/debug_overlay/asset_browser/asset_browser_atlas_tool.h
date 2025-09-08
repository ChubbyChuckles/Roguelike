/* Atlas Tool UI (extracted from panels_asset_browser.c) */
#ifndef ROGUE_DEBUG_OVERLAY_ASSET_BROWSER_ATLAS_TOOL_H
#define ROGUE_DEBUG_OVERLAY_ASSET_BROWSER_ATLAS_TOOL_H
#ifdef __cplusplus
extern "C"
{
#endif
    struct RogueAssetManager; /* fwd */
    /* Draw Atlas Tool controls; mutates global asset browser state. */
    void rogue_asset_browser_draw_atlas_tool(struct RogueAssetManager* m);
#ifdef __cplusplus
}
#endif
#endif
