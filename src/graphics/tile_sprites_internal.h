/* Internal shared declarations for tile sprite system split */
#ifndef ROGUE_TILE_SPRITES_INTERNAL_H
#define ROGUE_TILE_SPRITES_INTERNAL_H

#include "tile_sprites.h"
#include <stddef.h>

typedef struct TileVariant
{
    char path[256];
    int col, row;
    RogueTexture texture;
    RogueSprite sprite;
    int loaded;
} TileVariant;

typedef struct TileBucket
{
    TileVariant* variants;
    int count;
    int cap;
} TileBucket;

struct RogueTileSpritesGlobal
{
    int initialized;
    int tile_size;
    TileBucket buckets[ROGUE_TILE_MAX];
    int finalized;
};

extern struct RogueTileSpritesGlobal g_tile_sprites;

/* Shared helpers */
void rogue_tile_bucket_add_variant(TileBucket* b, const char* path, int col, int row);
void rogue_tile_normalize_path(char* p);

#endif /* ROGUE_TILE_SPRITES_INTERNAL_H */
