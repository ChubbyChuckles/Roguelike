/* Sprite/texture abstraction */
#include "graphics/sprite.h"
#include <stdio.h>
#include "util/log.h"

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#ifdef ROGUE_HAVE_SDL_IMAGE
#include <SDL_image.h>
#endif
extern SDL_Renderer* g_internal_sdl_renderer_ref;
#ifdef _WIN32
bool rogue_png_load_rgba(const char* path, unsigned char** out_pixels, int* w, int* h);
#endif
#endif

bool rogue_texture_load(RogueTexture* t, const char* path)
{
#if defined(ROGUE_HAVE_SDL) && defined(ROGUE_HAVE_SDL_IMAGE)
    if(!g_internal_sdl_renderer_ref) { ROGUE_LOG_ERROR("rogue_texture_load: renderer not ready"); return false; }
    const char* prefixes[] = { "", "../", "../../", "../../../" };
    SDL_Surface* surf = NULL;
    char attempt[512];
    for(size_t i=0;i<sizeof(prefixes)/sizeof(prefixes[0]);i++){
        snprintf(attempt, sizeof attempt, "%s%s", prefixes[i], path);
        surf = IMG_Load(attempt);
        if(surf){
            if(i>0) ROGUE_LOG_INFO("Loaded texture via fallback path: %s", attempt);
            break;
        }
    }
    if(!surf){
#ifdef _WIN32
        ROGUE_LOG_WARN("IMG_Load failed for %s (last error: %s). Trying WIC fallback.", path, IMG_GetError());
        /* Try WIC fallback manually */
        unsigned char* pixels=NULL; int w=0,h=0; int ok=0;
        for(size_t i=0;i<sizeof(prefixes)/sizeof(prefixes[0]) && !ok;i++){
            snprintf(attempt,sizeof attempt, "%s%s", prefixes[i], path);
            if(rogue_png_load_rgba(attempt,&pixels,&w,&h)){
                SDL_Surface* tmp = SDL_CreateRGBSurfaceWithFormatFrom(pixels,w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);
                if(tmp){ surf = tmp; ok=1; if(i>0) ROGUE_LOG_INFO("Loaded (WIC) via fallback path: %s", attempt); }
                else { free(pixels); pixels=NULL; }
            }
        }
        if(!surf){ return false; }
#else
        ROGUE_LOG_WARN("IMG_Load failed for all path variants of %s (last error: %s)", path, IMG_GetError());
        return false;
#endif
    }
    t->handle = SDL_CreateTextureFromSurface(g_internal_sdl_renderer_ref, surf);
    if(!t->handle){ ROGUE_LOG_WARN("SDL_CreateTextureFromSurface failed for %s", path); SDL_FreeSurface(surf); return false; }
    t->w = surf->w; t->h = surf->h;
    SDL_FreeSurface(surf);
    return true;
#elif defined(ROGUE_HAVE_SDL)
    /* SDL available but SDL_image not compiled in: attempt WIC fallback (Windows) */
    #ifdef _WIN32
    unsigned char* pixels=NULL; int w=0,h=0;
    if(!g_internal_sdl_renderer_ref) { ROGUE_LOG_ERROR("rogue_texture_load (WIC): renderer not ready"); return false; }
    const char* prefixes[] = { "", "../", "../../", "../../../" };
    int tried = 0; int ok = 0; char attempt[512];
    for(size_t i=0;i<sizeof(prefixes)/sizeof(prefixes[0]) && !ok;i++){
        snprintf(attempt, sizeof attempt, "%s%s", prefixes[i], path);
        tried++;
        if(rogue_png_load_rgba(attempt, &pixels, &w, &h)){
            if(i>0) ROGUE_LOG_INFO("Loaded texture via fallback path: %s", attempt);
            ok = 1; break;
        }
    }
    if(!ok){ ROGUE_LOG_WARN("WIC PNG load failed for all path variants of %s", path); return false; }
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 32, w*4, SDL_PIXELFORMAT_RGBA32);
    if(!surf){ ROGUE_LOG_WARN("SDL_CreateRGBSurfaceWithFormatFrom failed: %s", path); free(pixels); return false; }
    t->handle = SDL_CreateTextureFromSurface(g_internal_sdl_renderer_ref, surf);
    if(!t->handle){ ROGUE_LOG_WARN("SDL_CreateTextureFromSurface failed (WIC) %s", path); SDL_FreeSurface(surf); free(pixels); return false; }
    t->w = w; t->h = h;
    SDL_FreeSurface(surf); free(pixels); /* SDL copy done */
    return true;
    #else
    (void)path; (void)t; ROGUE_LOG_WARN("rogue_texture_load: built without SDL_image and no WIC fallback available"); return false;
    #endif
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
