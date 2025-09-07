/* Asset Classification Module
   Provides lightweight heuristic-based classification of asset paths into
   coarse categories used by tooling, validation, and future packaging.
   NOTE: Does NOT touch the filesystem; purely string/path based so it is
   deterministic and cheap for mass scans.
*/
#ifndef ROGUE_ASSET_CLASSIFICATION_H
#define ROGUE_ASSET_CLASSIFICATION_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RogueAssetType
    {
        ROGUE_ASSET_UNKNOWN = 0,
        ROGUE_ASSET_GRAPHICS_SPRITE,   /* sprite sheets / frame atlases */
        ROGUE_ASSET_GRAPHICS_TEXTURE,  /* standalone textures / backgrounds */
        ROGUE_ASSET_GRAPHICS_FONT,     /* font files (ttf/otf) */
        ROGUE_ASSET_AUDIO_MUSIC,       /* long-form music/ambient */
        ROGUE_ASSET_AUDIO_SFX,         /* short sound effects */
        ROGUE_ASSET_AUDIO_VOICE,       /* dialogue / voice over */
        ROGUE_ASSET_DATA_LEVEL,        /* level / map JSON */
        ROGUE_ASSET_DATA_CONFIG,       /* cfg/json gameplay/data config */
        ROGUE_ASSET_DATA_LOCALIZATION, /* localization bundles */
        ROGUE_ASSET_SHADER_VERTEX,
        ROGUE_ASSET_SHADER_FRAGMENT,
        ROGUE_ASSET_SHADER_COMPUTE,
        ROGUE_ASSET_META_SCHEMA, /* json schema definitions */
        /* TODO Phase 2: future categories for sprite/audio metadata JSON once schemas finalized */
        /* ROGUE_ASSET_SPRITE_META, */
        /* ROGUE_ASSET_AUDIO_META,  */
        ROGUE_ASSET_META_MANIFEST /* generated manifests */
    } RogueAssetType;

    /* Classify a path (relative or absolute). Path separators may be mixed. */
    RogueAssetType rogue_asset_classify(const char* path);

    /* String form (stable, lowercase snake). Returns "unknown" for out of range. */
    const char* rogue_asset_type_str(RogueAssetType t);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_CLASSIFICATION_H */
