#ifndef ROGUE_TILE_SPRITE_CACHE_H
#define ROGUE_TILE_SPRITE_CACHE_H
/* Builds and owns the precomputed tile sprite LUT formerly in app.c */
void rogue_tile_sprite_cache_ensure(void);
void rogue_tile_sprite_cache_free(void);
#endif
