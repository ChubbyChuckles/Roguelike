#include "world/tilemap.h"
#include <stdlib.h>

bool rogue_tilemap_init(RogueTileMap *map, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    map->width = w;
    map->height = h;
    map->tiles = (unsigned char *)calloc((size_t)w * (size_t)h, sizeof(unsigned char));
    return map->tiles != NULL;
}

void rogue_tilemap_free(RogueTileMap *map) {
    if (!map) return;
    free(map->tiles);
    map->tiles = NULL;
    map->width = map->height = 0;
}

static bool in_bounds(const RogueTileMap *map, int x, int y) {
    return x >= 0 && y >= 0 && x < map->width && y < map->height;
}

unsigned char rogue_tilemap_get(const RogueTileMap *map, int x, int y) {
    if (!in_bounds(map, x, y)) return 0;
    return map->tiles[y * map->width + x];
}

void rogue_tilemap_set(RogueTileMap *map, int x, int y, unsigned char v) {
    if (!in_bounds(map, x, y)) return;
    map->tiles[y * map->width + x] = v;
}
