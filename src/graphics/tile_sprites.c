#include "graphics/tile_sprites.h"
#include "util/log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct TileVariant {
    char path[256];
    int col,row;
    RogueTexture texture; /* each variant owns its texture for simplicity */
    RogueSprite sprite;
    int loaded;
} TileVariant;

typedef struct TileBucket {
    TileVariant* variants;
    int count;
    int cap;
} TileBucket;

static struct {
    int initialized;
    int tile_size;
    TileBucket buckets[ROGUE_TILE_MAX];
    int finalized;
} g_ts;

static void bucket_add_variant(TileBucket* b, const char* path, int col, int row){
    if(b->count == b->cap){
        int ncap = b->cap? b->cap*2 : 2;
        TileVariant* nv = (TileVariant*)realloc(b->variants, ncap * sizeof(TileVariant));
        if(!nv) return; /* OOM silently */
        b->variants = nv; b->cap = ncap;
    }
    TileVariant* v = &b->variants[b->count++];
    memset(v,0,sizeof *v);
#if defined(_MSC_VER)
    strncpy_s(v->path, sizeof v->path, path, _TRUNCATE);
#else
    strncpy(v->path, path, sizeof v->path -1); v->path[sizeof v->path -1] = '\0';
#endif
    v->col = col; v->row = row;
}

bool rogue_tile_sprites_init(int tile_size){
    memset(&g_ts,0,sizeof g_ts);
    if(tile_size<=0) tile_size = 64;
    g_ts.tile_size = tile_size;
    g_ts.initialized = 1;
    return true;
}

static RogueTileType name_to_type(const char* name){
    if(!name) return ROGUE_TILE_EMPTY;
    struct { const char* n; RogueTileType t; } map[] = {
        {"EMPTY", ROGUE_TILE_EMPTY}, {"WATER", ROGUE_TILE_WATER}, {"GRASS", ROGUE_TILE_GRASS},
        {"FOREST", ROGUE_TILE_FOREST}, {"MOUNTAIN", ROGUE_TILE_MOUNTAIN}, {"CAVE_WALL", ROGUE_TILE_CAVE_WALL},
        {"CAVE_FLOOR", ROGUE_TILE_CAVE_FLOOR}, {"RIVER", ROGUE_TILE_RIVER}
    };
    for(size_t i=0;i<sizeof(map)/sizeof(map[0]);i++) if(strcmp(map[i].n,name)==0) return map[i].t;
    return ROGUE_TILE_EMPTY;
}

void rogue_tile_sprite_define(RogueTileType type, const char* path, int col, int row){
    if(!g_ts.initialized || type<0 || type>=ROGUE_TILE_MAX || !path) return;
    bucket_add_variant(&g_ts.buckets[type], path, col, row);
}

static void normalize_path(char* p){
    while(*p){ if(*p=='\\') *p='/'; p++; }
}

bool rogue_tile_sprites_load_config(const char* path){
    if(!g_ts.initialized) return false;
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if(!f){ ROGUE_LOG_WARN("tile config open failed: %s", path); return false; }
    char line[1024];
    int lineno=0; int added=0;
    while(fgets(line, sizeof line, f)){
        lineno++;
        char* segment = line;
        while(1){
            while(*segment==' '||*segment=='\t') segment++;
            if(*segment=='#' || *segment=='\n' || *segment=='\0') break;
            if(strncmp(segment,"TILE",4)!=0) break;
            segment +=4;
            while(*segment==' '||*segment=='\t') segment++;
            if(*segment==',') segment++;
            while(*segment==' '||*segment=='\t') segment++;
            char name_tok[64]; int name_len=0;
            while(segment[name_len] && segment[name_len]!=',' && segment[name_len]!='\n' && name_len<63) name_len++;
            if(segment[name_len] != ','){ break; }
            memcpy(name_tok, segment, name_len); name_tok[name_len]='\0';
            segment += name_len + 1; /* skip comma */
            while(*segment==' '||*segment=='\t') segment++;
            char sheet_path[256]; int path_len=0;
            while(segment[path_len] && segment[path_len]!=',' && segment[path_len]!='\n' && path_len<255) path_len++;
            if(segment[path_len] != ','){ break; }
            memcpy(sheet_path, segment, path_len); sheet_path[path_len]='\0';
            segment += path_len + 1;
            while(*segment==' '||*segment=='\t') segment++;
            int col = atoi(segment);
            while(*segment && *segment!=',' && *segment!='\n') segment++;
            if(*segment!=',') break; segment++;
            while(*segment==' '||*segment=='\t') segment++;
            int row = atoi(segment);
            while(*segment && *segment!=' ' && *segment!='\t' && *segment!='\n') segment++;
            /* Normalize path */
            normalize_path(sheet_path);
            if(name_len>0 && path_len>0){
                RogueTileType t = name_to_type(name_tok);
                rogue_tile_sprite_define(t, sheet_path, col, row);
                added++;
            }
            /* Continue scanning same line for another TILE token */
            char* next = strstr(segment, "TILE");
            if(!next) break; else segment = next;
        }
    }
    fclose(f);
    ROGUE_LOG_INFO("tile config loaded %d variants", added);
    return added>0;
}

bool rogue_tile_sprites_finalize(void){
    if(g_ts.finalized) return true;
    int ok=1;
    for(int t=0;t<ROGUE_TILE_MAX;t++){
        TileBucket* b = &g_ts.buckets[t];
        for(int i=0;i<b->count;i++){
            TileVariant* v = &b->variants[i];
            if(!v->loaded){
                if(!rogue_texture_load(&v->texture, v->path)){
                    ROGUE_LOG_WARN("tile texture load fail: %s (tile=%d variant=%d)", v->path, t, i); ok=0; continue; }
                v->sprite.tex = &v->texture;
                v->sprite.sx = v->col * g_ts.tile_size;
                v->sprite.sy = v->row * g_ts.tile_size;
                v->sprite.sw = g_ts.tile_size;
                v->sprite.sh = g_ts.tile_size;
                v->loaded = 1;
            }
        }
    }
    if(!ok){
        ROGUE_LOG_WARN("Some tile textures failed to load. Verify SDL_image availability or add fallback loader.");
    } else {
        ROGUE_LOG_INFO("All tile textures loaded successfully");
    }
    g_ts.finalized = 1;
    return ok!=0;
}

static unsigned hash_xy(unsigned x, unsigned y, unsigned mod){
    unsigned h = x*73856093u ^ y*19349663u; /* simple spatial hash */
    return (mod>0)? (h % mod) : 0u;
}

const RogueSprite* rogue_tile_sprite_get_xy(RogueTileType type, int x, int y){
    if(type<0 || type>=ROGUE_TILE_MAX) return NULL;
    TileBucket* b = &g_ts.buckets[type];
    if(b->count==0) return NULL;
    unsigned idx = hash_xy((unsigned)x,(unsigned)y,(unsigned)b->count);
    TileVariant* v = &b->variants[idx];
    if(!v->loaded) return NULL;
    return &v->sprite;
}

const RogueSprite* rogue_tile_sprite_get(RogueTileType type){
    if(type<0 || type>=ROGUE_TILE_MAX) return NULL;
    TileBucket* b = &g_ts.buckets[type];
    if(b->count==0) return NULL;
    TileVariant* v = &b->variants[0];
    if(!v->loaded) return NULL;
    return &v->sprite;
}

void rogue_tile_sprites_shutdown(void){
    for(int t=0;t<ROGUE_TILE_MAX;t++){
        TileBucket* b=&g_ts.buckets[t];
        for(int i=0;i<b->count;i++){
            if(b->variants[i].loaded){
                rogue_texture_destroy(&b->variants[i].texture);
                b->variants[i].loaded=0;
            }
        }
        free(b->variants); b->variants=NULL; b->count=b->cap=0;
    }
    memset(&g_ts,0,sizeof g_ts);
}
