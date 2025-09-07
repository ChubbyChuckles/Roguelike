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

typedef struct RogueAssetTexture
{
    char id[96];        /* logical id (usually basename without extension) */
    char path[260];     /* original path provided */
    void* sdl_texture;  /* SDL_Texture* when loaded with a renderer, else NULL */
    int32_t width;      /* 0 when unknown */
    int32_t height;     /* 0 when unknown */
    uint32_t ref_count; /* active reference acquisitions */
    bool load_failed;   /* sticky flag for negative caching of missing assets */
} RogueAssetTexture;

typedef struct RogueAssetManager
{
    struct SDL_Renderer* renderer; /* optional */
    RogueAssetTexture textures[ROGUE_ASSET_MAX_TEXTURES];
    uint32_t texture_count;
    bool initialized;
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

#endif /* ROGUE_ASSET_MANAGER_H */
