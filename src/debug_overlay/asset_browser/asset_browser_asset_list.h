/* asset_browser_asset_list.h - extracted asset list rendering */
#ifndef ROGUE_ASSET_BROWSER_ASSET_LIST_H
#define ROGUE_ASSET_BROWSER_ASSET_LIST_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Draw the filtered asset list (textures, audio, json, shaders) and update
       g_ab_state.selected_row. filter: wildcard pattern (supports * and ?), may be empty string for
       no filter. */
    void rogue_asset_browser_draw_asset_list(const char* filter);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_BROWSER_ASSET_LIST_H */
