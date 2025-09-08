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
    /* Phase 3 tagging system: up to 8 lightweight categorization tags (lowercase, no spaces) */
    char tags[8][32];
    uint8_t tag_count;
} RogueAssetTexture;

typedef struct RogueAssetAudio
{
    char id[96];
    char path[260];
    void* sdl_chunk; /* Mix_Chunk* when loaded (lazy), else NULL */
    uint32_t ref_count;
    bool load_failed;    /* negative cache */
    uint64_t last_mtime; /* for hot-reload (future) */
    /* Loop points (milliseconds) for playback tools (overlay). 0/0 = disabled */
    uint32_t loop_start_ms;
    uint32_t loop_end_ms;
    /* Phase 3 tagging system (shared with textures) */
    char tags[8][32];
    uint8_t tag_count;
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
    bool streaming_enabled;    /* Phase 6: incremental streaming queue active */
    bool prefer_compressed_textures; /* Phase 6: platform optimization hook (try .ktx/.ktx2/.dds
                                        variants) */
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

/* Set loop points (ms). If end_ms <= start_ms loop is disabled. Returns 1 if applied. */
int rogue_asset_manager_set_audio_loop_points(int index, uint32_t start_ms, uint32_t end_ms);
/* Query loop points; returns 1 if defined. */
int rogue_asset_manager_get_audio_loop_points(int index, uint32_t* out_start_ms,
                                              uint32_t* out_end_ms);

/* ---------------- Phase 3: Tagging & Categorization ---------------- */
/* Tags are stored lowercase, trimmed, no internal spaces (spaces converted to '_' ).
    Returns 1 on success, 0 on failure (invalid index, duplicate, full, or invalid tag). */
int rogue_asset_manager_add_texture_tag(int texture_index, const char* tag);
int rogue_asset_manager_remove_texture_tag(int texture_index, const char* tag);
int rogue_asset_manager_has_texture_tag(int texture_index, const char* tag);
int rogue_asset_manager_list_texture_tags(int texture_index, const char** out_tags, int max);
int rogue_asset_manager_find_textures_by_tag(const char* tag, int* out_indices, int max);

int rogue_asset_manager_add_audio_tag(int audio_index, const char* tag);
int rogue_asset_manager_remove_audio_tag(int audio_index, const char* tag);
int rogue_asset_manager_has_audio_tag(int audio_index, const char* tag);
int rogue_asset_manager_list_audio_tags(int audio_index, const char** out_tags, int max);
int rogue_asset_manager_find_audio_by_tag(const char* tag, int* out_indices, int max);

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
    uint32_t stream_queue_depth; /* current pending texture stream jobs */
    uint32_t stream_loaded_count; /* number of textures loaded via stream step */
    uint32_t atlas_build_count;   /* number of atlases built (Phase 6) */
    uint32_t last_atlas_width;    /* width of most recently built atlas */
} RogueAssetMetrics;

/* Retrieve current metrics (out may be NULL). */
void rogue_asset_manager_get_metrics(RogueAssetMetrics* out);
/* Reset (zero) all metrics counters. */
void rogue_asset_manager_reset_metrics(void);

/* ---------------- Phase 6: Streaming Loader (Incremental) ----------------
    Design (initial slice): rather than full threaded SDL texture creation (which can have
    renderer thread affinity constraints), we implement an incremental streaming queue.
    Enqueue requests record texture indices whose actual load will be attempted later in
    controlled batches (e.g., per-frame) via stream_step. This reduces long startup stalls by
    spreading IO/decoding over multiple frames. Future upgrade path: internal worker thread
    that loads image data into interim surfaces, with main-thread promotion to textures. */

/* Enable/disable streaming mode (clears queue when disabled). */
void rogue_asset_manager_set_streaming_enabled(bool enable);
int rogue_asset_manager_streaming_enabled(void);
/* Enqueue a texture path for streaming. Returns texture index (existing or new) or -1. If
 * streaming disabled this falls back to immediate (lazy or direct) acquisition. */
int rogue_asset_manager_enqueue_texture_stream(const char* path);
/* Process up to max_to_load pending stream jobs (<=0 means process entire queue). Returns number
 * actually loaded this call. */
int rogue_asset_manager_stream_step(int max_to_load);
/* Current queue depth (pending jobs). */
int rogue_asset_manager_stream_queue_depth(void);

/* Snapshot (copy) current pending streaming texture jobs into caller-provided buffer.
   Returns number of entries written (may be < max). The queue is shallow: jobs are
   removed only when processed via stream_step. This read-only snapshot allows debug
   UIs to display pending jobs without exposing internal mutable arrays. */
typedef struct RogueStreamJobInfo
{
    int texture_index;  /* index into texture array */
    char path[260];     /* original path enqueued */
    int already_loaded; /* 1 if texture SDL object now loaded */
    int load_failed;    /* 1 if negative cache flag set */
} RogueStreamJobInfo;
int rogue_asset_manager_stream_queue_snapshot(RogueStreamJobInfo* out_jobs, int max_jobs);

/* ---------------- Phase 6: Platform-Specific Optimization Hooks --------- */
/* Hint the manager to prefer compressed texture variants (.ktx2, .ktx, .dds) when present.
    The acquire path will attempt to substitute an alternate extension if the original path
    does not exist or if an optimized variant is found alongside it. Safe no-op when SDL_image
    lacks support for the chosen format(s); only simple existence probing performed here. */
void rogue_asset_manager_set_prefer_compressed_textures(bool enable);
int rogue_asset_manager_get_prefer_compressed_textures(void);

/* ---------------- Phase 6: Generalized Atlas Tooling -------------------- */
typedef struct RogueAtlasUV
{
    float u0, v0, u1, v1; /* normalized UV rectangle */
} RogueAtlasUV;
/* Build a horizontal atlas from already acquired texture indices. Creates a new texture record
 * (returned index) and fills out_uvs (count must match input count). Returns atlas texture index
 * on success or -1 on failure / headless environment. The source textures are left intact. */
int rogue_asset_manager_build_atlas_horizontal(const int* texture_indices, int count,
                                               RogueAtlasUV* out_uvs, int uv_cap);

/* ---------------- Phase 3 (Remaining): Basic Processing / Export -------------- */
/* Resize an existing texture (must be loaded or loadable). When replace==0 a new texture record
 * is created (variant) and the original left intact; returns new texture index. When replace!=0
 * the existing texture's SDL_Texture is destroyed/replaced in-place (id/path unchanged) and the
 * same index returned. Returns -1 on failure (headless, invalid index, load failure, params). */
int rogue_asset_manager_resize_texture_variant(int texture_index, int new_w, int new_h,
                                               int replace);
/* Export a texture to a BMP on disk (always available via SDL core). Returns 1 on success, 0 on
 * failure (invalid index, headless, load failure, write error). The texture will be force-loaded
 * if necessary. */
int rogue_asset_manager_export_texture_bmp(int texture_index, const char* out_path);

/* Batch resize multiple textures. new_w/new_h apply to all. When replace==0 creates variants for
 * each (id suffixed individually); when replace!=0 resizes in-place. out_indices (optional) can
 * capture resulting indices (variant or original). Returns number successfully processed. */
int rogue_asset_manager_batch_resize(const int* texture_indices, int count, int new_w, int new_h,
                                     int replace, int* out_indices, int out_cap);
/* Batch export a set of textures to BMP. out_dir must exist. Filenames are <id>.bmp (or
 * <id>_<i>.bmp on collision). Returns number exported. */
int rogue_asset_manager_batch_export_bmp(const int* texture_indices, int count,
                                         const char* out_dir);
/* PNG export (requires SDL_image providing IMG_SavePNG or fallback stub disabled at build).
 * Returns 1 on success else 0. */
int rogue_asset_manager_export_texture_png(int texture_index, const char* out_path);
int rogue_asset_manager_batch_export_png(const int* texture_indices, int count,
                                         const char* out_dir);

#endif /* ROGUE_ASSET_MANAGER_H */
