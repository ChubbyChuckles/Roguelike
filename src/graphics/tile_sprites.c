#include "graphics/tile_sprites.h"
#include "util/log.h"
#include <string.h>
#include <stdlib.h>

#define MAX_TILE_TEXTURES 32

typedef struct TileDef {
    char path[256];
    int col,row;
    RogueSprite sprite;
    int defined;
    int loaded;
} TileDef;

static struct {
    int initialized;
    int tile_size;
    TileDef defs[ROGUE_TILE_MAX];
    /* We allow multiple tiles referencing same path; maintain distinct textures to keep logic simple for now */
} g_ts;

bool rogue_tile_sprites_init(int tile_size){
    if(tile_size<=0) tile_size = 64;
    memset(&g_ts,0,sizeof g_ts);
    g_ts.tile_size = tile_size;
    g_ts.initialized = 1;
    return true;
}

void rogue_tile_sprite_define(RogueTileType type, const char* path, int col, int row){
    if(!g_ts.initialized || type < 0 || type >= ROGUE_TILE_MAX) return;
    TileDef* d = &g_ts.defs[type];
    d->defined = 1;
    d->col = col; d->row = row;
    if(path){
#if defined(_MSC_VER)
    strncpy_s(d->path, sizeof d->path, path, _TRUNCATE);
#else
    strncpy(d->path, path, sizeof d->path -1);
    d->path[sizeof d->path -1] = '\0';
#endif
    }
}

bool rogue_tile_sprites_finalize(void){
    int ok = 1;
    for(int i=0;i<ROGUE_TILE_MAX;i++){
        TileDef* d = &g_ts.defs[i];
        if(!d->defined) continue;
        if(!d->loaded){
            if(!rogue_texture_load((RogueTexture*)&d->sprite.tex, d->path)){
                ROGUE_LOG_WARN("Failed to load tile texture %s", d->path);
                ok = 0; continue;
            }
            d->sprite.tex = (RogueTexture*)d->sprite.tex; /* already assigned */
            d->sprite.sx = d->col * g_ts.tile_size;
            d->sprite.sy = d->row * g_ts.tile_size;
            d->sprite.sw = g_ts.tile_size;
            d->sprite.sh = g_ts.tile_size;
            d->loaded = 1;
        }
    }
    return ok!=0;
}

const RogueSprite* rogue_tile_sprite_get(RogueTileType type){
    if(type < 0 || type >= ROGUE_TILE_MAX) return NULL;
    TileDef* d = &g_ts.defs[type];
    if(!d->defined || !d->loaded) return NULL;
    return &d->sprite;
}

void rogue_tile_sprites_shutdown(void){
    for(int i=0;i<ROGUE_TILE_MAX;i++){
        TileDef* d = &g_ts.defs[i];
        if(d->loaded && d->sprite.tex){
            rogue_texture_destroy(d->sprite.tex);
            d->loaded = 0;
        }
    }
    memset(&g_ts,0,sizeof g_ts);
}
