/**
 * @file tilemap.h
 * @brief 2D tile-based world map system.
 *
 * Provides the core tilemap data structure and API for managing 2D tile-based
 * worlds. Supports a primary tile layer for terrain types and an optional
 * overlay layer for decorative elements. Includes comprehensive tile type
 * enumeration covering terrain, structures, and dungeon elements used by
 * the procedural generation system.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

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
#include <stdint.h>

/**
 * @brief 2D tile map data structure.
 *
 * Represents a 2D grid of tiles with optional overlay decoration layer.
 * The primary tiles array stores terrain and structure types, while the
 * optional overlay_deco array stores lightweight decorative markers.
 * Memory is managed internally and should be freed with rogue_tilemap_free().
 */
typedef struct RogueTileMap
{
    int width;                   ///< Map width in tiles
    int height;                  ///< Map height in tiles
    unsigned char* tiles;        ///< Primary tile data array (width * height elements)
    unsigned char* overlay_deco; ///< Optional overlay decoration layer (may be NULL)
    uint32_t overlay_magic;      ///< Internal guard value for overlay validation
} RogueTileMap;

/**
 * @brief Basic tile type classifications for procedural generation.
 * @ingroup WorldGen
 *
 * Comprehensive enumeration of all tile types used throughout the game,
 * from basic terrain to complex dungeon elements. Values are used as
 * indices into tile graphics and behavior lookup tables.
 */
typedef enum RogueTileType
{
    ROGUE_TILE_EMPTY = 0,     ///< Empty/void tile
    ROGUE_TILE_WATER,         ///< Water tile (typically non-walkable)
    ROGUE_TILE_GRASS,         ///< Grass terrain (walkable)
    ROGUE_TILE_FOREST,        ///< Forest tile (may restrict movement)
    ROGUE_TILE_MOUNTAIN,      ///< Mountain tile (typically non-walkable)
    ROGUE_TILE_CAVE_WALL,     ///< Cave wall (non-walkable)
    ROGUE_TILE_CAVE_FLOOR,    ///< Cave floor (walkable)
    ROGUE_TILE_RIVER,         ///< River tile (flowing water)
    ROGUE_TILE_SWAMP,         ///< Swamp terrain (may slow movement)
    ROGUE_TILE_SNOW,          ///< Snow-covered terrain
    ROGUE_TILE_RIVER_DELTA,   ///< River delta formation
    ROGUE_TILE_RIVER_WIDE,    ///< Wide river section
    
    /* Local terrain detailing */
    ROGUE_TILE_LAVA,          ///< Lava pocket (hazardous terrain)
    ROGUE_TILE_ORE_VEIN,      ///< Ore vein embedded in rock/cave
    ROGUE_TILE_BRIDGE_HINT,   ///< Potential bridge/crossing location marker
    
    /* Structures and points of interest */
    ROGUE_TILE_STRUCTURE_WALL,   ///< Generic structure wall
    ROGUE_TILE_STRUCTURE_FLOOR,  ///< Generic structure floor
    ROGUE_TILE_DUNGEON_ENTRANCE, ///< Entrance portal to dungeon
    
    /* Dungeon-specific tiles */
    ROGUE_TILE_DUNGEON_WALL,        ///< Solid dungeon wall (non-walkable)
    ROGUE_TILE_DUNGEON_FLOOR,       ///< Walkable dungeon floor
    ROGUE_TILE_DUNGEON_DOOR,        ///< Regular door (can be opened)
    ROGUE_TILE_DUNGEON_LOCKED_DOOR, ///< Locked door requiring key
    ROGUE_TILE_DUNGEON_SECRET_DOOR, ///< Secret door to hidden room
    ROGUE_TILE_DUNGEON_TRAP,        ///< Trap tile (triggers when stepped on)
    ROGUE_TILE_DUNGEON_KEY,         ///< Key pickup tile
    
    ROGUE_TILE_MAX               ///< Total number of tile types (used for bounds checking)
} RogueTileType;

/** @name Tilemap API */
/**@{*/
/**
 * @brief Initializes a tilemap with width/height; tiles are zeroed.
 * @param[out] map Tilemap to initialize.
 * @param[in] w Width (>0).
 * @param[in] h Height (>0).
 * @return true on success; false if dims invalid or OOM.
 */
bool rogue_tilemap_init(RogueTileMap* map, int w, int h);
/**
 * @brief Frees tile storage and resets the map.
 * @param[in,out] map Tilemap to free (safe with NULL).
 */
void rogue_tilemap_free(RogueTileMap* map);
/**
 * @brief Gets tile at (x,y) or returns 0 if out of bounds.
 * @param[in] map Tilemap to read.
 * @param[in] x X coordinate.
 * @param[in] y Y coordinate.
 * @return Tile value or 0 when out of bounds.
 */
unsigned char rogue_tilemap_get(const RogueTileMap* map, int x, int y);
/**
 * @brief Sets tile at (x,y); ignored if out of bounds.
 * @param[in,out] map Tilemap to write.
 * @param[in] x X coordinate.
 * @param[in] y Y coordinate.
 * @param[in] v Tile value to set.
 */
void rogue_tilemap_set(RogueTileMap* map, int x, int y, unsigned char v);

/** Overlay (deco) helpers */
/**
 * @brief Gets the overlay deco code at (x,y); returns 0 if none or out of bounds.
 */
unsigned char rogue_tilemap_get_deco(const RogueTileMap* map, int x, int y);
/**
 * @brief Sets the overlay deco code at (x,y); ignored if out of bounds. 0 clears.
 */
void rogue_tilemap_set_deco(RogueTileMap* map, int x, int y, unsigned char code);
/**
 * @brief Returns 1 if the overlay deco code at (x,y) is considered blocking for movement.
 * Codes: 1=pillar (blocking), 2=banner (non-blocking), 3=brazier (non-blocking), >=128 reserved.
 */
int rogue_tilemap_overlay_blocks(const RogueTileMap* map, int x, int y);
/**@}*/

#endif
