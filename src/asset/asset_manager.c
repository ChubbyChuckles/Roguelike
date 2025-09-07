/* asset_manager.c - initial Phase 3 implementation */
#include "asset_manager.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

/* Minimal hash: derive id from basename (without extension) */
static void derive_id(const char* path, char* out_id, size_t cap)
{
    if (!path || !out_id || cap == 0)
        return;
    const char* slash = strrchr(path, '/');
    const char* bslash = strrchr(path, '\\');
    const char* base = path;
    if (slash && bslash)
        base = (slash > bslash) ? slash + 1 : bslash + 1;
    else if (slash)
        base = slash + 1;
    else if (bslash)
        base = bslash + 1;
    const char* dot = strrchr(base, '.');
    size_t len = dot && dot > base ? (size_t) (dot - base) : strlen(base);
    if (len >= cap)
        len = cap - 1;
    memcpy(out_id, base, len);
    out_id[len] = '\0';
}

static RogueAssetManager g_asset_mgr = {0};

RogueAssetManager* rogue_asset_manager_instance(void) { return &g_asset_mgr; }

bool rogue_asset_manager_init(struct SDL_Renderer* renderer)
{
    if (g_asset_mgr.initialized)
        return true;
    memset(&g_asset_mgr, 0, sizeof(g_asset_mgr));
    g_asset_mgr.renderer = renderer; /* may be NULL in headless tests */
    g_asset_mgr.initialized = true;
    return true;
}

void rogue_asset_manager_shutdown(void)
{
    if (!g_asset_mgr.initialized)
        return;
    /* Future: destroy SDL_Texture objects */
    memset(&g_asset_mgr, 0, sizeof(g_asset_mgr));
}

static int find_slot_by_id(const char* id)
{
    for (uint32_t i = 0; i < g_asset_mgr.texture_count; ++i)
    {
        if (strcasecmp(g_asset_mgr.textures[i].id, id) == 0)
            return (int) i;
    }
    return -1;
}

int rogue_asset_manager_find_by_id(const char* id)
{
    if (!g_asset_mgr.initialized || !id)
        return -1;
    return find_slot_by_id(id);
}

const RogueAssetTexture* rogue_asset_manager_get(int index)
{
    if (index < 0 || (uint32_t) index >= g_asset_mgr.texture_count)
        return NULL;
    return &g_asset_mgr.textures[index];
}

int rogue_asset_manager_acquire_texture(const char* path)
{
    if (!g_asset_mgr.initialized || !path)
        return -1;
    char id[96];
    derive_id(path, id, sizeof(id));
    int existing = find_slot_by_id(id);
    if (existing >= 0)
    {
        g_asset_mgr.textures[existing].ref_count++;
        return existing;
    }
    if (g_asset_mgr.texture_count >= ROGUE_ASSET_MAX_TEXTURES)
        return -1; /* full */
    RogueAssetTexture* tex = &g_asset_mgr.textures[g_asset_mgr.texture_count];
    memset(tex, 0, sizeof(*tex));
    {
        size_t len_id = strlen(id);
        if (len_id >= sizeof(tex->id))
            len_id = sizeof(tex->id) - 1;
        memcpy(tex->id, id, len_id);
        tex->id[len_id] = '\0';
        size_t len_path = strlen(path);
        if (len_path >= sizeof(tex->path))
            len_path = sizeof(tex->path) - 1;
        memcpy(tex->path, path, len_path);
        tex->path[len_path] = '\0';
    }
    /* Phase 3 slice: do not attempt actual SDL load (deferred). */
    tex->sdl_texture = NULL;
    tex->width = 0;
    tex->height = 0;
    tex->ref_count = 1;
    tex->load_failed = false; /* would flip if future load fails */
    int index = (int) g_asset_mgr.texture_count;
    g_asset_mgr.texture_count++;
    return index;
}

void rogue_asset_manager_release_texture(int index)
{
    if (index < 0 || (uint32_t) index >= g_asset_mgr.texture_count)
        return;
    RogueAssetTexture* tex = &g_asset_mgr.textures[index];
    if (tex->ref_count > 0)
        tex->ref_count--;
    if (tex->ref_count == 0)
    {
        /* Compact array (order not guaranteed) */
        uint32_t last = g_asset_mgr.texture_count - 1;
        if ((uint32_t) index != last)
            g_asset_mgr.textures[index] = g_asset_mgr.textures[last];
        memset(&g_asset_mgr.textures[last], 0, sizeof(g_asset_mgr.textures[last]));
        g_asset_mgr.texture_count--;
    }
}
