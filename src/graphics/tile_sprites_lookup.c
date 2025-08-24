/* Lookup and hashing utilities for tile sprites */
#include "tile_sprites_internal.h"

static unsigned hash_xy(unsigned x, unsigned y, unsigned mod)
{
    unsigned h = x * 73856093u ^ y * 19349663u;
    return (mod > 0) ? (h % mod) : 0u;
}

const RogueSprite* rogue_tile_sprite_get_xy(RogueTileType type, int x, int y)
{
    if (type < 0 || type >= ROGUE_TILE_MAX)
        return NULL;
    TileBucket* b = &g_tile_sprites.buckets[type];
    if (b->count == 0)
        return NULL;
    unsigned idx = hash_xy((unsigned) x, (unsigned) y, (unsigned) b->count);
    TileVariant* v = &b->variants[idx];
    if (!v->loaded)
        return NULL;
    return &v->sprite;
}

const RogueSprite* rogue_tile_sprite_get(RogueTileType type)
{
    if (type < 0 || type >= ROGUE_TILE_MAX)
        return NULL;
    TileBucket* b = &g_tile_sprites.buckets[type];
    if (b->count == 0)
        return NULL;
    TileVariant* v = &b->variants[0];
    if (!v->loaded)
        return NULL;
    return &v->sprite;
}
