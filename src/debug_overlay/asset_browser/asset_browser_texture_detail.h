/* asset_browser_texture_detail.h - extracted texture + sprite/animation detail UI */
#ifndef ROGUE_ASSET_BROWSER_TEXTURE_DETAIL_H
#define ROGUE_ASSET_BROWSER_TEXTURE_DETAIL_H

#ifdef __cplusplus
extern "C"
{
#endif

    struct RogueAssetTexture;
    struct RogueAssetManager;

    /* Draw detailed texture view (selected texture information, comparison, tags, preview,
       sprite rect editor, animation frame editor, batch/resize/export tools). */
    void rogue_asset_browser_draw_texture_detail(const struct RogueAssetTexture* tex,
                                                 const struct RogueAssetManager* m,
                                                 const char* active_filter);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_BROWSER_TEXTURE_DETAIL_H */
