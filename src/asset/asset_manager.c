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

/* ---------------- Internal: Streaming Queue ---------------- */
typedef struct RogueStreamJob
{
    int texture_index; /* index into textures array */
    char path[260];    /* original path (for existence / variant probing) */
} RogueStreamJob;

#define ROGUE_STREAM_MAX_JOBS 128
static RogueStreamJob g_stream_jobs[ROGUE_STREAM_MAX_JOBS];
static int g_stream_job_count = 0;

static void stream_queue_clear(void) { g_stream_job_count = 0; }

static int stream_queue_find(int texture_index)
{
    for (int i = 0; i < g_stream_job_count; ++i)
        if (g_stream_jobs[i].texture_index == texture_index)
            return i;
    return -1;
}

static void stream_queue_compact(int remove_index)
{
    if (remove_index < 0 || remove_index >= g_stream_job_count)
        return;
    int last = g_stream_job_count - 1;
    if (remove_index != last)
        g_stream_jobs[remove_index] = g_stream_jobs[last];
    g_stream_job_count--;
}

RogueAssetManager* rogue_asset_manager_instance(void) { return &g_asset_mgr; }

bool rogue_asset_manager_init(struct SDL_Renderer* renderer)
{
    if (g_asset_mgr.initialized)
        return true;
    memset(&g_asset_mgr, 0, sizeof(g_asset_mgr));
    g_asset_mgr.renderer = renderer; /* may be NULL in headless tests */
    g_asset_mgr.initialized = true;
    g_asset_mgr.lazy_loading_enabled = false;
    g_asset_mgr.streaming_enabled = false;
    g_asset_mgr.prefer_compressed_textures = false;
    memset(&g_asset_metrics, 0, sizeof g_asset_metrics);
    stream_queue_clear();
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
    stream_queue_clear();
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

/* Attempt to find a compressed variant when preference enabled.
   Order: .ktx2, .ktx, .dds. Returns path to use (maybe original). */
static const char* maybe_substitute_compressed(const char* original, char* tmp, size_t tmp_cap)
{
    if (!original || !*original || !g_asset_mgr.prefer_compressed_textures)
        return original;
    const char* exts[] = {".ktx2", ".ktx", ".dds"};
    const char* dot = strrchr(original, '.');
    size_t base_len = dot ? (size_t) (dot - original) : strlen(original);
    if (base_len + 5 >= tmp_cap)
        return original; /* insufficient space */
    for (int i = 0; i < 3; ++i)
    {
        size_t ext_len = strlen(exts[i]);
        memcpy(tmp, original, base_len);
        memcpy(tmp + base_len, exts[i], ext_len + 1); /* include null */
        if (rogue_asset_file_exists(tmp))
            return tmp; /* substitute */
    }
    return original;
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
    /* Compressed preference (only if not already substituted by fallback) */
    char compressed_buf[320];
    if (!substituted && g_asset_mgr.prefer_compressed_textures)
    {
        const char* maybe =
            maybe_substitute_compressed(chosen_path, compressed_buf, sizeof compressed_buf);
        if (maybe != chosen_path)
            chosen_path = maybe;
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
    {
        g_asset_metrics.stream_queue_depth = (uint32_t) g_stream_job_count;
        *out = g_asset_metrics;
    }
}

void rogue_asset_manager_reset_metrics(void)
{
    memset(&g_asset_metrics, 0, sizeof g_asset_metrics);
}

/* ---------------- Streaming Loader API ---------------- */

void rogue_asset_manager_set_streaming_enabled(bool enable)
{
    g_asset_mgr.streaming_enabled = enable;
    if (!enable)
        stream_queue_clear();
}

int rogue_asset_manager_streaming_enabled(void) { return g_asset_mgr.streaming_enabled ? 1 : 0; }

int rogue_asset_manager_enqueue_texture_stream(const char* path)
{
    if (!g_asset_mgr.streaming_enabled)
        return rogue_asset_manager_acquire_texture(path);
    int idx = rogue_asset_manager_acquire_texture(path);
    if (idx < 0)
        return -1;
    if (g_stream_job_count >= ROGUE_STREAM_MAX_JOBS)
        return idx; /* queue full; texture may already be loaded (lazy path) */
    const RogueAssetTexture* tex = rogue_asset_manager_get(idx);
    if (!tex)
        return idx;
    if (tex->sdl_texture || tex->load_failed)
        return idx; /* nothing to stream */
    if (stream_queue_find(idx) >= 0)
        return idx; /* already queued */
    RogueStreamJob* job = &g_stream_jobs[g_stream_job_count++];
    job->texture_index = idx;
    size_t plen = strlen(path);
    if (plen >= sizeof(job->path))
        plen = sizeof(job->path) - 1;
    memcpy(job->path, path, plen);
    job->path[plen] = '\0';
    return idx;
}

int rogue_asset_manager_stream_step(int max_to_load)
{
    if (!g_asset_mgr.streaming_enabled || g_stream_job_count == 0)
        return 0;
    int remaining = (max_to_load <= 0) ? g_stream_job_count : max_to_load;
    int loaded = 0;
    /* Simple FIFO: iterate from end to allow compact removal */
    for (int i = g_stream_job_count - 1; i >= 0 && remaining > 0; --i)
    {
        RogueStreamJob* job = &g_stream_jobs[i];
        RogueAssetTexture* tex = (RogueAssetTexture*) rogue_asset_manager_get(job->texture_index);
        if (!tex)
        {
            stream_queue_compact(i);
            continue;
        }
        if (!tex->sdl_texture && !tex->load_failed)
        {
            texture_attempt_load(tex);
            if (tex->sdl_texture)
            {
                g_asset_metrics.stream_loaded_count++;
                loaded++;
            }
        }
        stream_queue_compact(i);
        remaining--;
    }
    return loaded;
}

int rogue_asset_manager_stream_queue_depth(void) { return g_stream_job_count; }

/* ---------------- Platform Optimization Flags ---------------- */
void rogue_asset_manager_set_prefer_compressed_textures(bool enable)
{
    g_asset_mgr.prefer_compressed_textures = enable;
}
int rogue_asset_manager_get_prefer_compressed_textures(void)
{
    return g_asset_mgr.prefer_compressed_textures ? 1 : 0;
}

/* ---------------- Atlas Tooling (Horizontal) ---------------- */
int rogue_asset_manager_build_atlas_horizontal(const int* texture_indices, int count,
                                               RogueAtlasUV* out_uvs, int uv_cap)
{
    if (!texture_indices || count <= 0 || !out_uvs || uv_cap < count)
        return -1;
#if defined(ROGUE_HAVE_SDL) && defined(ROGUE_HAVE_SDL_IMAGE)
    if (!g_asset_mgr.renderer)
        return -1; /* headless */
    /* First ensure all textures are loaded (lazy or streamed) */
    int total_w = 0;
    int max_h = 0;
    for (int i = 0; i < count; ++i)
    {
        if (!rogue_asset_manager_ensure_texture_loaded(texture_indices[i]))
            return -1;
        const RogueAssetTexture* tex = rogue_asset_manager_get(texture_indices[i]);
        if (!tex || !tex->sdl_texture)
            return -1;
        total_w += tex->width;
        if (tex->height > max_h)
            max_h = tex->height;
    }
    SDL_Texture* atlas = SDL_CreateTexture(g_asset_mgr.renderer, SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET, total_w, max_h);
    if (!atlas)
        return -1;
    SDL_Texture* prev_target = SDL_GetRenderTarget(g_asset_mgr.renderer);
    SDL_SetRenderTarget(g_asset_mgr.renderer, atlas);
    int x = 0;
    for (int i = 0; i < count; ++i)
    {
        const RogueAssetTexture* tex = rogue_asset_manager_get(texture_indices[i]);
        SDL_Rect dst = {x, 0, tex->width, tex->height};
        SDL_RenderCopy(g_asset_mgr.renderer, (SDL_Texture*) tex->sdl_texture, NULL, &dst);
        float u0 = (float) x / (float) total_w;
        float u1 = (float) (x + tex->width) / (float) total_w;
        out_uvs[i].u0 = u0;
        out_uvs[i].v0 = 0.0f;
        out_uvs[i].u1 = u1;
        out_uvs[i].v1 = (float) tex->height / (float) max_h;
        x += tex->width;
    }
    SDL_SetRenderTarget(g_asset_mgr.renderer, prev_target);
    /* Register atlas as a new texture record (no path / synthetic) */
    if (g_asset_mgr.texture_count >= ROGUE_ASSET_MAX_TEXTURES)
    {
        SDL_DestroyTexture(atlas);
        return -1;
    }
    RogueAssetTexture* rec = &g_asset_mgr.textures[g_asset_mgr.texture_count];
    memset(rec, 0, sizeof *rec);
    snprintf(rec->id, sizeof rec->id, "atlas_%u", g_asset_mgr.texture_count);
    snprintf(rec->path, sizeof rec->path, "<atlas:%d>", count);
    rec->sdl_texture = atlas;
    rec->width = total_w;
    rec->height = max_h;
    rec->ref_count = 1; /* implicitly acquired */
    rec->load_failed = false;
    rec->last_mtime = 0;
    int atlas_index = (int) g_asset_mgr.texture_count;
    g_asset_mgr.texture_count++;
    g_asset_metrics.atlas_build_count++;
    g_asset_metrics.last_atlas_width = (uint32_t) total_w;
    return atlas_index;
#else
    (void) texture_indices;
    (void) count;
    (void) out_uvs;
    (void) uv_cap;
    return -1;
#endif
}
