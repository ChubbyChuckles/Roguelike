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
#include "tilemap.h"
#include <stdlib.h>

/**
 * @brief Initializes a tilemap with the specified dimensions.
 *
 * Allocates memory for the tile grid and sets the width and height.
 * All tiles are initialized to 0 (ROGUE_TILE_EMPTY).
 *
 * @param[in,out] map Pointer to the RogueTileMap structure to initialize.
 * @param[in] w Width of the tilemap in tiles. Must be positive.
 * @param[in] h Height of the tilemap in tiles. Must be positive.
 * @return true if initialization succeeds, false if dimensions are invalid or memory allocation
 * fails.
 */
bool rogue_tilemap_init(RogueTileMap* map, int w, int h)
{
    if (w <= 0 || h <= 0)
        return false;
    map->width = w;
    map->height = h;
    map->tiles = (unsigned char*) calloc((size_t) w * (size_t) h, sizeof(unsigned char));
    return map->tiles != NULL;
}

/**
 * @brief Frees the memory allocated for a tilemap.
 *
 * Deallocates the tile grid and resets the map dimensions to zero.
 * Safe to call on an already freed or uninitialized map.
 *
 * @param[in,out] map Pointer to the RogueTileMap structure to free.
 */
void rogue_tilemap_free(RogueTileMap* map)
{
    if (!map)
        return;
    free(map->tiles);
    map->tiles = NULL;
    map->width = map->height = 0;
}

/**
 * @brief Checks if coordinates are within the tilemap bounds.
 *
 * @param[in] map Pointer to the RogueTileMap to check against.
 * @param[in] x X-coordinate to validate.
 * @param[in] y Y-coordinate to validate.
 * @return true if coordinates are within bounds, false otherwise.
 */
static bool in_bounds(const RogueTileMap* map, int x, int y)
{
    return x >= 0 && y >= 0 && x < map->width && y < map->height;
}

/**
 * @brief Retrieves the tile value at the specified coordinates.
 *
 * @param[in] map Pointer to the RogueTileMap to query.
 * @param[in] x X-coordinate of the tile to retrieve.
 * @param[in] y Y-coordinate of the tile to retrieve.
 * @return The tile value at the coordinates, or 0 if coordinates are out of bounds.
 */
unsigned char rogue_tilemap_get(const RogueTileMap* map, int x, int y)
{
    if (!in_bounds(map, x, y))
        return 0;
    return map->tiles[y * map->width + x];
}

/**
 * @brief Sets the tile value at the specified coordinates.
 *
 * If coordinates are out of bounds, the operation is ignored.
 *
 * @param[in,out] map Pointer to the RogueTileMap to modify.
 * @param[in] x X-coordinate of the tile to set.
 * @param[in] y Y-coordinate of the tile to set.
 * @param[in] v New tile value to assign.
 */
void rogue_tilemap_set(RogueTileMap* map, int x, int y, unsigned char v)
{
    if (!in_bounds(map, x, y))
        return;
    map->tiles[y * map->width + x] = v;
}
