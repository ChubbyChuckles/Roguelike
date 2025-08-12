#ifndef ROGUE_GRAPHICS_TILE_SPRITES_H
#define ROGUE_GRAPHICS_TILE_SPRITES_H

#include "world/tilemap.h"
#include "graphics/sprite.h"
#include <stdbool.h>

/* Initialize registry with tile size (default 64). */
bool rogue_tile_sprites_init(int tile_size);
/* Define mapping for a tile type. col,row are zero-based indices in a 64x64 grid on the sheet. */
void rogue_tile_sprite_define(RogueTileType type, const char* path, int col, int row);
/* Load all textures & compute sprite rects (idempotent). Returns false if any definition failed to load. */
bool rogue_tile_sprites_finalize(void);
/* Retrieve sprite for tile type (may return NULL if undefined). */
const RogueSprite* rogue_tile_sprite_get(RogueTileType type);
/* Free loaded textures and reset definitions. */
void rogue_tile_sprites_shutdown(void);

#endif
