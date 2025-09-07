/* asset_manager.c - Phase 3: texture/audio loading + negative caching + reload poll */
#include "asset_manager.h"
#include "asset_validation.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#endif
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#if defined(ROGUE_HAVE_SDL) && defined(ROGUE_HAVE_SDL_IMAGE)
#include <SDL_image.h>
#endif
#if defined(ROGUE_HAVE_SDL_MIXER)
#include <SDL_mixer.h>
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
/* Phase 6: metrics (simple cumulative counters) */
static RogueAssetMetrics g_asset_metrics = {0};

RogueAssetManager* rogue_asset_manager_instance(void) { return &g_asset_mgr; }

bool rogue_asset_manager_init(struct SDL_Renderer* renderer)
{
    if (g_asset_mgr.initialized)
        return true;
    memset(&g_asset_mgr, 0, sizeof(g_asset_mgr));
    g_asset_mgr.renderer = renderer; /* may be NULL in headless tests */
    g_asset_mgr.initialized = true;
    g_asset_mgr.lazy_loading_enabled = false;
    memset(&g_asset_metrics, 0, sizeof g_asset_metrics);
    return true;
}

void rogue_asset_manager_shutdown(void)
{
    if (!g_asset_mgr.initialized)
        return;
        /* Destroy loaded textures / audio */
#if defined(ROGUE_HAVE_SDL)
    for (uint32_t i = 0; i < g_asset_mgr.texture_count; ++i)
    {
        if (g_asset_mgr.textures[i].sdl_texture)
        {
            SDL_DestroyTexture((SDL_Texture*) g_asset_mgr.textures[i].sdl_texture);
            g_asset_mgr.textures[i].sdl_texture = NULL;
        }
    }
#endif
#if defined(ROGUE_HAVE_SDL_MIXER)
    for (uint32_t i = 0; i < g_asset_mgr.audio_count; ++i)
    {
        if (g_asset_mgr.audio[i].sdl_chunk)
        {
            Mix_FreeChunk((Mix_Chunk*) g_asset_mgr.audio[i].sdl_chunk);
            g_asset_mgr.audio[i].sdl_chunk = NULL;
        }
    }
#endif
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

/* stat helper (returns mtime or 0) */
static uint64_t file_mtime(const char* path)
{
    if (!path || !*path)
        return 0;
#ifdef _WIN32
    struct _stat s;
    if (_stat(path, &s) == 0)
        return (uint64_t) s.st_mtime;
#else
    struct stat s;
    if (stat(path, &s) == 0)
        return (uint64_t) s.st_mtime;
#endif
    return 0;
}

static void texture_attempt_load(RogueAssetTexture* tex)
{
#if defined(ROGUE_HAVE_SDL) && defined(ROGUE_HAVE_SDL_IMAGE)
    if (!g_asset_mgr.renderer || tex->sdl_texture || tex->load_failed)
        return;
    uint64_t start_ticks = 0ULL;
    /* Use SDL_GetPerformanceCounter when available for micro-timing */
#if defined(ROGUE_HAVE_SDL)
    uint64_t freq = SDL_GetPerformanceFrequency();
    start_ticks = SDL_GetPerformanceCounter();
#endif
    SDL_Texture* t = IMG_LoadTexture(g_asset_mgr.renderer, tex->path);
    if (!t)
    {
        tex->load_failed = true; /* negative cache */
        return;
    }
    int w = 0, h = 0;
    if (SDL_QueryTexture(t, NULL, NULL, &w, &h) == 0)
    {
        tex->width = w;
        tex->height = h;
    }
    tex->sdl_texture = t;
    tex->last_mtime = file_mtime(tex->path);
    /* record metrics */
#if defined(ROGUE_HAVE_SDL)
    if (start_ticks)
    {
        uint64_t end_ticks = SDL_GetPerformanceCounter();
        if (freq)
        {
            uint64_t us = (end_ticks - start_ticks) * 1000000ULL / freq;
            g_asset_metrics.texture_load_us += us;
        }
    }
#endif
    g_asset_metrics.texture_load_count++;
#else
    (void) tex;
#endif
}

int rogue_asset_manager_acquire_texture(const char* path)
{
    if (!g_asset_mgr.initialized || !path)
        return -1;
    /* Fallback resolution (Phase 4): if path missing and a fallback is registered, switch */
    const char* chosen_path = path;
    bool substituted = false;
    if (!rogue_asset_file_exists(path))
    {
        const char* fb = rogue_asset_get_fallback_texture();
        if (fb && rogue_asset_file_exists(fb))
        {
            chosen_path = fb;
            substituted = true;
        }
    }
    char id[96];
    /* If we substituted a fallback, still derive id from original missing path so
       that multiple distinct missing logical textures occupy separate records. */
    if (substituted)
        derive_id(path, id, sizeof(id));
    else
        derive_id(chosen_path, id, sizeof(id));
    int existing = find_slot_by_id(id);
    if (existing >= 0)
    {
        g_asset_mgr.textures[existing].ref_count++;
        /* lazy load if not yet attempted */
        if (!g_asset_mgr.textures[existing].sdl_texture &&
            !g_asset_mgr.textures[existing].load_failed)
            texture_attempt_load(&g_asset_mgr.textures[existing]);
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
        size_t len_path = strlen(chosen_path);
        if (len_path >= sizeof(tex->path))
            len_path = sizeof(tex->path) - 1;
        memcpy(tex->path, chosen_path, len_path);
        tex->path[len_path] = '\0';
    }
    /* Phase 3 slice: do not attempt actual SDL load (deferred). */
    tex->sdl_texture = NULL;
    tex->width = 0;
    tex->height = 0;
    tex->ref_count = 1;
    tex->load_failed = false;
    tex->last_mtime = 0;
    if (!g_asset_mgr.lazy_loading_enabled)
        texture_attempt_load(tex); /* immediate attempt unless lazy mode */
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

/* ---------------- Audio (Mix_Chunk) ---------------- */

static int audio_find_slot_by_id(const char* id)
{
    for (uint32_t i = 0; i < g_asset_mgr.audio_count; ++i)
        if (strcasecmp(g_asset_mgr.audio[i].id, id) == 0)
            return (int) i;
    return -1;
}

static void audio_attempt_load(RogueAssetAudio* au)
{
#if defined(ROGUE_HAVE_SDL_MIXER)
    if (au->sdl_chunk || au->load_failed)
        return;
    uint64_t start_ticks = 0ULL;
#if defined(ROGUE_HAVE_SDL)
    uint64_t freq = SDL_GetPerformanceFrequency();
    start_ticks = SDL_GetPerformanceCounter();
#endif
    Mix_Chunk* c = Mix_LoadWAV(au->path);
    if (!c)
    {
        au->load_failed = true;
        return;
    }
    au->sdl_chunk = c;
    au->last_mtime = file_mtime(au->path);
    /* metrics */
#if defined(ROGUE_HAVE_SDL)
    if (start_ticks)
    {
        uint64_t end_ticks = SDL_GetPerformanceCounter();
        if (freq)
        {
            uint64_t us = (end_ticks - start_ticks) * 1000000ULL / freq;
            g_asset_metrics.audio_load_us += us;
        }
    }
#endif
    g_asset_metrics.audio_load_count++;
#else
    (void) au;
#endif
}

int rogue_asset_manager_acquire_audio(const char* path)
{
    if (!g_asset_mgr.initialized || !path)
        return -1;
    char id[96];
    derive_id(path, id, sizeof id);
    int existing = audio_find_slot_by_id(id);
    if (existing >= 0)
    {
        g_asset_mgr.audio[existing].ref_count++;
        if (!g_asset_mgr.audio[existing].sdl_chunk && !g_asset_mgr.audio[existing].load_failed)
            audio_attempt_load(&g_asset_mgr.audio[existing]);
        return existing;
    }
    if (g_asset_mgr.audio_count >= ROGUE_ASSET_MAX_AUDIO)
        return -1;
    RogueAssetAudio* au = &g_asset_mgr.audio[g_asset_mgr.audio_count];
    memset(au, 0, sizeof *au);
    {
        size_t len_id = strlen(id);
        if (len_id >= sizeof au->id)
            len_id = sizeof au->id - 1;
        memcpy(au->id, id, len_id);
        au->id[len_id] = '\0';
        size_t len_path = strlen(path);
        if (len_path >= sizeof au->path)
            len_path = sizeof au->path - 1;
        memcpy(au->path, path, len_path);
        au->path[len_path] = '\0';
    }
    au->ref_count = 1;
    au->load_failed = false;
    au->sdl_chunk = NULL;
    au->last_mtime = 0;
    if (!g_asset_mgr.lazy_loading_enabled)
        audio_attempt_load(au);
    int index = (int) g_asset_mgr.audio_count;
    g_asset_mgr.audio_count++;
    return index;
}

void rogue_asset_manager_release_audio(int index)
{
    if (index < 0 || (uint32_t) index >= g_asset_mgr.audio_count)
        return;
    RogueAssetAudio* au = &g_asset_mgr.audio[index];
    if (au->ref_count > 0)
        au->ref_count--;
    if (au->ref_count == 0)
    {
#if defined(ROGUE_HAVE_SDL_MIXER)
        if (au->sdl_chunk)
            Mix_FreeChunk((Mix_Chunk*) au->sdl_chunk);
#endif
        uint32_t last = g_asset_mgr.audio_count - 1;
        if ((uint32_t) index != last)
            g_asset_mgr.audio[index] = g_asset_mgr.audio[last];
        memset(&g_asset_mgr.audio[last], 0, sizeof g_asset_mgr.audio[last]);
        g_asset_mgr.audio_count--;
    }
}

const RogueAssetAudio* rogue_asset_manager_get_audio(int index)
{
    if (index < 0 || (uint32_t) index >= g_asset_mgr.audio_count)
        return NULL;
    return &g_asset_mgr.audio[index];
}

/* ---------------- Hot Reload Poll ---------------- */

int rogue_asset_manager_poll_reload(void)
{
    if (!g_asset_mgr.initialized)
        return 0;
    int reloaded = 0;
#if defined(ROGUE_HAVE_SDL) && defined(ROGUE_HAVE_SDL_IMAGE)
    for (uint32_t i = 0; i < g_asset_mgr.texture_count; ++i)
    {
        RogueAssetTexture* tex = &g_asset_mgr.textures[i];
        if (tex->load_failed)
            continue; /* nothing to reload until manual invalidate */
        uint64_t mt = file_mtime(tex->path);
        if (mt && tex->last_mtime && mt != tex->last_mtime)
        {
            if (tex->sdl_texture)
            {
                SDL_DestroyTexture((SDL_Texture*) tex->sdl_texture);
                tex->sdl_texture = NULL;
            }
            tex->width = tex->height = 0;
            tex->last_mtime = 0;
            tex->load_failed = false; /* allow retry */
            texture_attempt_load(tex);
            reloaded++;
        }
    }
#endif
#if defined(ROGUE_HAVE_SDL_MIXER)
    for (uint32_t i = 0; i < g_asset_mgr.audio_count; ++i)
    {
        RogueAssetAudio* au = &g_asset_mgr.audio[i];
        if (au->load_failed)
            continue;
        uint64_t mt = file_mtime(au->path);
        if (mt && au->last_mtime && mt != au->last_mtime)
        {
            if (au->sdl_chunk)
            {
                Mix_FreeChunk((Mix_Chunk*) au->sdl_chunk);
                au->sdl_chunk = NULL;
            }
            au->last_mtime = 0;
            au->load_failed = false; /* retry */
            audio_attempt_load(au);
            reloaded++;
        }
    }
#endif
    return reloaded;
}

/* ---------------- Phase 6: Performance Optimization Slice ---------------- */

void rogue_asset_manager_set_lazy_loading(bool enable)
{
    g_asset_mgr.lazy_loading_enabled = enable;
}

bool rogue_asset_manager_ensure_texture_loaded(int index)
{
    if (index < 0 || (uint32_t) index >= g_asset_mgr.texture_count)
        return false;
    RogueAssetTexture* tex = &g_asset_mgr.textures[index];
    if (tex->sdl_texture)
        return true;
    if (tex->load_failed)
        return false; /* negative cached failure */
    texture_attempt_load(tex);
    return tex->sdl_texture != NULL;
}

int rogue_asset_manager_preload_texture(const char* path)
{
    int idx = rogue_asset_manager_acquire_texture(path);
    if (idx >= 0 && g_asset_mgr.lazy_loading_enabled)
        rogue_asset_manager_ensure_texture_loaded(idx);
    return idx;
}

int rogue_asset_manager_preload_textures(const char* const* paths, int count)
{
    if (!paths || count <= 0)
        return 0;
    int loaded = 0;
    for (int i = 0; i < count; ++i)
    {
        int idx = rogue_asset_manager_preload_texture(paths[i]);
        if (idx >= 0)
            loaded++;
    }
    return loaded;
}

void rogue_asset_manager_get_metrics(RogueAssetMetrics* out)
{
    if (out)
        *out = g_asset_metrics;
}

void rogue_asset_manager_reset_metrics(void)
{
    memset(&g_asset_metrics, 0, sizeof g_asset_metrics);
}
