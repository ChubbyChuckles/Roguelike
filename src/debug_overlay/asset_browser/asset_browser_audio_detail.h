/* asset_browser_audio_detail.h - extracted audio detail + loop editor UI */
#ifndef ROGUE_ASSET_BROWSER_AUDIO_DETAIL_H
#define ROGUE_ASSET_BROWSER_AUDIO_DETAIL_H

#ifdef __cplusplus
extern "C"
{
#endif

    struct RogueAssetAudio;
    struct RogueAssetManager;

    void rogue_asset_browser_draw_audio_detail(const struct RogueAssetAudio* audio,
                                               const struct RogueAssetManager* m);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_BROWSER_AUDIO_DETAIL_H */
