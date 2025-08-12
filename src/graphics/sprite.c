/* Sprite/texture abstraction */
#include "graphics/sprite.h"
#include <stdio.h>

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#ifdef ROGUE_HAVE_SDL_IMAGE
#include <SDL_image.h>
#endif
extern SDL_Renderer* g_internal_sdl_renderer_ref;
#endif

bool rogue_texture_load(RogueTexture* t, const char* path)
{
#if defined(ROGUE_HAVE_SDL) && defined(ROGUE_HAVE_SDL_IMAGE)
    if(!g_internal_sdl_renderer_ref) return false;
    SDL_Surface* surf = IMG_Load(path);
    if(!surf){ fprintf(stderr, "IMG_Load failed: %s\n", IMG_GetError()); return false; }
    t->handle = SDL_CreateTextureFromSurface(g_internal_sdl_renderer_ref, surf);
    if(!t->handle){ SDL_FreeSurface(surf); return false; }
    t->w = surf->w; t->h = surf->h;
    SDL_FreeSurface(surf);
    return true;
#else
    (void)t; (void)path; return false;
#endif
}

void rogue_texture_destroy(RogueTexture* t)
{
#ifdef ROGUE_HAVE_SDL
    if(t->handle) SDL_DestroyTexture(t->handle);
    t->handle = NULL; t->w = t->h = 0;
#else
    (void)t;
#endif
}

void rogue_sprite_draw(const RogueSprite* spr, int x, int y, int scale)
{
#ifdef ROGUE_HAVE_SDL
    if(!spr || !spr->tex || !spr->tex->handle) return;
    if(scale < 1) scale = 1;
    SDL_Rect src = { spr->sx, spr->sy, spr->sw, spr->sh };
    SDL_Rect dst = { x, y, spr->sw * scale, spr->sh * scale };
    SDL_RenderCopy(g_internal_sdl_renderer_ref, spr->tex->handle, &src, &dst);
#else
    (void) spr; (void) x; (void) y; (void) scale;
#endif
}
