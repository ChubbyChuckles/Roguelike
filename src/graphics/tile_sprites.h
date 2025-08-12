#ifndef ROGUE_GRAPHICS_TILE_SPRITES_H
#define ROGUE_GRAPHICS_TILE_SPRITES_H

#include "world/tilemap.h"
#include "graphics/sprite.h"
#include <stdbool.h>

/* External configuration driven tile sprite registry with variant support. */

/* Initialize registry with tile size (default 64). Call once at startup. */
bool rogue_tile_sprites_init(int tile_size);

/* Programmatically add a variant mapping (may be called multiple times for same type). */
void rogue_tile_sprite_define(RogueTileType type, const char* path, int col, int row);

/* Load definitions from a text config file. Format per non-empty/non-comment line:
	TILE,NAME,path/to/sheet.png,col,row
	NAME matches enum token without prefix (e.g. GRASS, WATER, FOREST, MOUNTAIN, CAVE_WALL, CAVE_FLOOR, RIVER, EMPTY)
	Multiple lines with same NAME create variants.
	Lines starting with '#'(possibly preceded by whitespace) are ignored. */
bool rogue_tile_sprites_load_config(const char* path);

/* Finalize (load textures & slice). Safe to call multiple times; only loads once. */
bool rogue_tile_sprites_finalize(void);

/* Retrieve sprite for tile type using deterministic variant chosen from tile coordinates (x,y). */
const RogueSprite* rogue_tile_sprite_get_xy(RogueTileType type, int x, int y);

/* Retrieve first variant (legacy). */
const RogueSprite* rogue_tile_sprite_get(RogueTileType type);

/* Shutdown & free. */
void rogue_tile_sprites_shutdown(void);

#endif
