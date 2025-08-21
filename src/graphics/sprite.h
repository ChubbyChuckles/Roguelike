#ifndef ROGUE_GRAPHICS_SPRITE_H
#define ROGUE_GRAPHICS_SPRITE_H

#include <stdbool.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

typedef struct RogueTexture
{
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* handle;
#endif
    int w, h;
} RogueTexture;

typedef struct RogueSprite
{
    RogueTexture* tex;
    int sx, sy, sw, sh;
} RogueSprite;

bool rogue_texture_load(RogueTexture* t, const char* path);
void rogue_texture_destroy(RogueTexture* t);
void rogue_sprite_draw(const RogueSprite* spr, int x, int y, int scale);

#endif
