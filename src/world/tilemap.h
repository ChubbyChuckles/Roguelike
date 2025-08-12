#ifndef ROGUE_WORLD_TILEMAP_H
#define ROGUE_WORLD_TILEMAP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct RogueTileMap {
    int width;
    int height;
    unsigned char *tiles;
} RogueTileMap;

bool rogue_tilemap_init(RogueTileMap *map, int w, int h);
void rogue_tilemap_free(RogueTileMap *map);
unsigned char rogue_tilemap_get(const RogueTileMap *map, int x, int y);
void rogue_tilemap_set(RogueTileMap *map, int x, int y, unsigned char v);

#endif
