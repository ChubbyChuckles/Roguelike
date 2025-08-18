/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef ROGUE_WORLD_TILEMAP_H
#define ROGUE_WORLD_TILEMAP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct RogueTileMap
{
    int width;
    int height;
    unsigned char* tiles;
} RogueTileMap;

/* Basic tile type classifications for procedural generation */
typedef enum RogueTileType
{
    ROGUE_TILE_EMPTY = 0,
    ROGUE_TILE_WATER,
    ROGUE_TILE_GRASS,
    ROGUE_TILE_FOREST,
    ROGUE_TILE_MOUNTAIN,
    ROGUE_TILE_CAVE_WALL,
    ROGUE_TILE_CAVE_FLOOR,
    ROGUE_TILE_RIVER,
    ROGUE_TILE_SWAMP,
    ROGUE_TILE_SNOW,
    ROGUE_TILE_RIVER_DELTA,
    ROGUE_TILE_RIVER_WIDE,
    /* Phase 4 additions (local terrain detailing) */
    ROGUE_TILE_LAVA,        /* lava pocket */
    ROGUE_TILE_ORE_VEIN,    /* ore vein marker embedded in rock/cave */
    ROGUE_TILE_BRIDGE_HINT, /* potential bridge/crossing marker */
    /* Phase 6 additions (structures & POIs) */
    ROGUE_TILE_STRUCTURE_WALL,  /* generic structure wall */
    ROGUE_TILE_STRUCTURE_FLOOR, /* generic structure floor */
    ROGUE_TILE_DUNGEON_ENTRANCE,/* entrance portal marker */
    /* Phase 7 additions (dungeon generator) */
    ROGUE_TILE_DUNGEON_WALL,    /* solid dungeon wall */
    ROGUE_TILE_DUNGEON_FLOOR,   /* walkable dungeon floor */
    ROGUE_TILE_DUNGEON_DOOR,    /* regular door */
    ROGUE_TILE_DUNGEON_LOCKED_DOOR, /* locked door requiring key */
    ROGUE_TILE_DUNGEON_SECRET_DOOR, /* secret door to hidden room */
    ROGUE_TILE_DUNGEON_TRAP,    /* trap tile */
    ROGUE_TILE_DUNGEON_KEY,     /* key pickup tile */
    ROGUE_TILE_MAX
} RogueTileType;

bool rogue_tilemap_init(RogueTileMap* map, int w, int h);
void rogue_tilemap_free(RogueTileMap* map);
unsigned char rogue_tilemap_get(const RogueTileMap* map, int x, int y);
void rogue_tilemap_set(RogueTileMap* map, int x, int y, unsigned char v);

#endif
