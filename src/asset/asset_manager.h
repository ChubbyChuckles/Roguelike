/* asset_manager.h - Phase 3 (Asset Structure Plan)
   Minimal SDL-aware (headless safe) asset loading & caching scaffold.
   Responsibilities (initial slice):
     - Central init/shutdown
     - Texture record bookkeeping (id/path, ref counts)
     - Stub load when SDL renderer unavailable (allows tests to exercise API headlessly)
     - Simple fixed-cap array cache (deterministic, no malloc churn)
   Deferred (future phases):
     - Actual SDL_Texture creation (PNG via SDL_image, audio via SDL_mixer)
     - Async streaming / hot reload hooks
     - Memory pressure eviction / LRU
*/
#ifndef ROGUE_ASSET_MANAGER_H
#define ROGUE_ASSET_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

struct SDL_Renderer; /* forward decl; header consumers needn't include SDL */

#define ROGUE_ASSET_MAX_TEXTURES 256
#define ROGUE_ASSET_MAX_AUDIO 128

typedef struct RogueAssetTexture
{
    char id[96];         /* logical id (usually basename without extension) */
    char path[260];      /* original path provided */
    void* sdl_texture;   /* SDL_Texture* when loaded with a renderer, else NULL */
    int32_t width;       /* 0 when unknown */
    int32_t height;      /* 0 when unknown */
    uint32_t ref_count;  /* active reference acquisitions */
    bool load_failed;    /* sticky flag for negative caching of missing assets */
    uint64_t last_mtime; /* file modification time snapshot for hot-reload (0=unknown) */
} RogueAssetTexture;

typedef struct RogueAssetAudio
{
    char id[96];
    char path[260];
    void* sdl_chunk; /* Mix_Chunk* when loaded (lazy), else NULL */
    uint32_t ref_count;
    bool load_failed;    /* negative cache */
    uint64_t last_mtime; /* for hot-reload (future) */
} RogueAssetAudio;

typedef struct RogueAssetManager
{
    struct SDL_Renderer* renderer; /* optional */
    RogueAssetTexture textures[ROGUE_ASSET_MAX_TEXTURES];
    uint32_t texture_count;
    RogueAssetAudio audio[ROGUE_ASSET_MAX_AUDIO];
    uint32_t audio_count;
    bool initialized;
    bool lazy_loading_enabled; /* Phase 6: when true, initial acquire defers actual SDL load */
} RogueAssetManager;

/* Global singleton (tests keep usage simple; future: allow multiple contexts) */
RogueAssetManager* rogue_asset_manager_instance(void);

bool rogue_asset_manager_init(struct SDL_Renderer* renderer);
void rogue_asset_manager_shutdown(void);

/* Acquire (loads or bumps ref). Returns index >=0 on success, -1 on error. */
int rogue_asset_manager_acquire_texture(const char* path);
/* Release by index (safe no-op on invalid). */
void rogue_asset_manager_release_texture(int index);

/* Lookup helpers */
int rogue_asset_manager_find_by_id(const char* id);
const RogueAssetTexture* rogue_asset_manager_get(int index);

/* Audio assets (Mix_Chunk) */
int rogue_asset_manager_acquire_audio(const char* path);
void rogue_asset_manager_release_audio(int index);
const RogueAssetAudio* rogue_asset_manager_get_audio(int index);

/* Hot-reload polling (stat-based) – returns number of textures reloaded. Safe headless. */
int rogue_asset_manager_poll_reload(void);

/* ---------------- Phase 6: Performance Optimization Slice ---------------- */

/* Enable/disable lazy loading (defer SDL texture creation until explicitly ensured). */
void rogue_asset_manager_set_lazy_loading(bool enable);
/* Force load (if deferred) of a previously acquired texture by index. Returns true if loaded or
 * already loaded. */
bool rogue_asset_manager_ensure_texture_loaded(int index);

/* Preload helpers: acquire (and load unless lazy mode) one or more textures. Returns index of last
 * acquired or -1 on error. */
int rogue_asset_manager_preload_texture(const char* path);
/* Batch variant: returns number successfully preloaded. */
int rogue_asset_manager_preload_textures(const char* const* paths, int count);

typedef struct RogueAssetMetrics
{
    uint64_t texture_load_us;    /* cumulative microseconds spent inside successful texture loads */
    uint32_t texture_load_count; /* number of successful texture load attempts */
    uint64_t audio_load_us;      /* cumulative microseconds for audio loads */
    uint32_t audio_load_count;   /* number of successful audio loads */
} RogueAssetMetrics;

/* Retrieve current metrics (out may be NULL). */
void rogue_asset_manager_get_metrics(RogueAssetMetrics* out);
/* Reset (zero) all metrics counters. */
void rogue_asset_manager_reset_metrics(void);

#endif /* ROGUE_ASSET_MANAGER_H */
