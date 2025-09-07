/* Implementation: asset_classification
   Pure string/path heuristics; no filesystem access. */
#include "asset_classification.h"
#include <ctype.h>
#include <string.h>

static int ends_with_ci(const char* s, const char* suffix)
{
    size_t ls = strlen(s), lf = strlen(suffix);
    if (lf > ls)
        return 0;
    s += ls - lf;
    for (size_t i = 0; i < lf; ++i)
    {
        char a = (char) tolower((unsigned char) s[i]);
        char b = (char) tolower((unsigned char) suffix[i]);
        if (a != b)
            return 0;
    }
    return 1;
}

static int contains_dir_token_ci(const char* path_in, const char* token)
{
    /* Create a small normalized copy (separators -> '/') limited to 512 chars */
    char buf[512];
    size_t n = 0;
    while (path_in[n] && n < sizeof(buf) - 1)
    {
        char c = path_in[n];
        if (c == '\\')
            c = '/';
        buf[n] = (char) tolower((unsigned char) c);
        ++n;
    }
    buf[n] = '\0';
    char token_lc[64];
    size_t lt = strlen(token);
    if (lt >= sizeof(token_lc))
        return 0;
    for (size_t i = 0; i < lt; ++i)
        token_lc[i] = (char) tolower((unsigned char) token[i]);
    token_lc[lt] = '\0';
    const char* s = strstr(buf, token_lc);
    while (s)
    {
        int start_ok = (s == buf) || (*(s - 1) == '/');
        int end_ok = (*(s + lt) == '/' || *(s + lt) == '\0');
        if (start_ok && end_ok)
            return 1;
        s = strstr(s + 1, token_lc);
    }
    return 0;
}

RogueAssetType rogue_asset_classify(const char* path)
{
    if (!path || !*path)
        return ROGUE_ASSET_UNKNOWN;
    /* Fast extension based buckets first */
    if (ends_with_ci(path, ".png") || ends_with_ci(path, ".bmp") || ends_with_ci(path, ".tga"))
    {
        if (contains_dir_token_ci(path, "sprites"))
            return ROGUE_ASSET_GRAPHICS_SPRITE;
        return ROGUE_ASSET_GRAPHICS_TEXTURE;
    }
    if (ends_with_ci(path, ".ttf") || ends_with_ci(path, ".otf"))
        return ROGUE_ASSET_GRAPHICS_FONT;
    if (ends_with_ci(path, ".ogg") || ends_with_ci(path, ".mp3") || ends_with_ci(path, ".wav"))
    {
        if (contains_dir_token_ci(path, "music") || contains_dir_token_ci(path, "ambient"))
            return ROGUE_ASSET_AUDIO_MUSIC;
        if (contains_dir_token_ci(path, "voice") || contains_dir_token_ci(path, "dialogue"))
            return ROGUE_ASSET_AUDIO_VOICE;
        return ROGUE_ASSET_AUDIO_SFX;
    }
    if (ends_with_ci(path, ".json"))
    {
        if (contains_dir_token_ci(path, "schemas"))
            return ROGUE_ASSET_META_SCHEMA;
        if (contains_dir_token_ci(path, "levels") || contains_dir_token_ci(path, "maps"))
            return ROGUE_ASSET_DATA_LEVEL;
        if (contains_dir_token_ci(path, "localization"))
            return ROGUE_ASSET_DATA_LOCALIZATION;
        if (contains_dir_token_ci(path, "manifests"))
            return ROGUE_ASSET_META_MANIFEST;
        return ROGUE_ASSET_DATA_CONFIG;
    }
    if (ends_with_ci(path, ".cfg"))
        return ROGUE_ASSET_DATA_CONFIG;
    if (ends_with_ci(path, ".vert"))
        return ROGUE_ASSET_SHADER_VERTEX;
    if (ends_with_ci(path, ".frag"))
        return ROGUE_ASSET_SHADER_FRAGMENT;
    if (ends_with_ci(path, ".comp") || ends_with_ci(path, ".compute"))
        return ROGUE_ASSET_SHADER_COMPUTE;
    return ROGUE_ASSET_UNKNOWN;
}

const char* rogue_asset_type_str(RogueAssetType t)
{
    switch (t)
    {
    case ROGUE_ASSET_GRAPHICS_SPRITE:
        return "graphics_sprite";
    case ROGUE_ASSET_GRAPHICS_TEXTURE:
        return "graphics_texture";
    case ROGUE_ASSET_GRAPHICS_FONT:
        return "graphics_font";
    case ROGUE_ASSET_AUDIO_MUSIC:
        return "audio_music";
    case ROGUE_ASSET_AUDIO_SFX:
        return "audio_sfx";
    case ROGUE_ASSET_AUDIO_VOICE:
        return "audio_voice";
    case ROGUE_ASSET_DATA_LEVEL:
        return "data_level";
    case ROGUE_ASSET_DATA_CONFIG:
        return "data_config";
    case ROGUE_ASSET_DATA_LOCALIZATION:
        return "data_localization";
    case ROGUE_ASSET_SHADER_VERTEX:
        return "shader_vertex";
    case ROGUE_ASSET_SHADER_FRAGMENT:
        return "shader_fragment";
    case ROGUE_ASSET_SHADER_COMPUTE:
        return "shader_compute";
    case ROGUE_ASSET_META_SCHEMA:
        return "meta_schema";
    case ROGUE_ASSET_META_MANIFEST:
        return "meta_manifest";
    default:
        return "unknown";
    }
}
