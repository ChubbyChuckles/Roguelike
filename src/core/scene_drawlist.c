#include "scene_drawlist.h"
#include <stdlib.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#ifndef ROGUE_MAX_DRAW_ITEMS
#define ROGUE_MAX_DRAW_ITEMS 8192
#endif

static RogueDrawItem g_items[ROGUE_MAX_DRAW_ITEMS];
static int g_item_count = 0;
#ifdef ROGUE_HAVE_SDL
typedef struct WeaponOverlayItem
{
    SDL_Texture* tex;
    float x, y, w, h;
    float pivot_x, pivot_y;
    float angle;
    int flip;
    unsigned char r, g, b, a;
} WeaponOverlayItem;
static WeaponOverlayItem g_weapon_overlays[32];
static int g_weapon_overlay_count = 0;
#endif

void rogue_scene_drawlist_begin(void)
{
    g_item_count = 0;
#ifdef ROGUE_HAVE_SDL
    g_weapon_overlay_count = 0;
#endif
}

void rogue_scene_drawlist_push_sprite(const RogueSprite* spr, int dx, int dy, int y_base, int flip,
                                      unsigned char r, unsigned char g, unsigned char b,
                                      unsigned char a)
{
    if (!spr || !spr->tex)
        return;
#ifdef ROGUE_HAVE_SDL
    if (!spr->tex->handle)
        return;
#endif
    if (g_item_count >= ROGUE_MAX_DRAW_ITEMS)
        return;
    RogueDrawItem* it = &g_items[g_item_count++];
    memset(it, 0, sizeof *it);
    it->kind = ROGUE_DRAW_SPRITE;
    it->sprite = spr;
    it->dx = dx;
    it->dy = dy;
    it->dw = spr->sw;
    it->dh = spr->sh;
    it->y_sort = y_base;
    it->flip = (unsigned char) flip;
    it->tint_r = r;
    it->tint_g = g;
    it->tint_b = b;
    it->tint_a = a;
}

static int cmp_draw_items(const void* a, const void* b)
{
    const RogueDrawItem* A = (const RogueDrawItem*) a;
    const RogueDrawItem* B = (const RogueDrawItem*) b;
    if (A->y_sort < B->y_sort)
        return -1;
    if (A->y_sort > B->y_sort)
        return 1;
    return 0;
}

void rogue_scene_drawlist_flush(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    if (g_item_count > 0)
    {
        qsort(g_items, g_item_count, sizeof(RogueDrawItem), cmp_draw_items);
        for (int i = 0; i < g_item_count; i++)
        {
            RogueDrawItem* it = &g_items[i];
            if (it->kind == ROGUE_DRAW_SPRITE)
            {
                const RogueSprite* spr = it->sprite;
                SDL_Rect src = {spr->sx, spr->sy, spr->sw, spr->sh};
                SDL_Rect dst = {it->dx, it->dy, it->dw, it->dh};
                if (spr->tex && spr->tex->handle)
                {
                    if (it->tint_r != 255 || it->tint_g != 255 || it->tint_b != 255)
                        SDL_SetTextureColorMod(spr->tex->handle, it->tint_r, it->tint_g,
                                               it->tint_b);
                    if (it->tint_a != 255)
                        SDL_SetTextureAlphaMod(spr->tex->handle, it->tint_a);
                    SDL_RenderCopyEx(g_app.renderer, spr->tex->handle, &src, &dst, 0.0, NULL,
                                     it->flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
                    if (it->tint_r != 255 || it->tint_g != 255 || it->tint_b != 255)
                        SDL_SetTextureColorMod(spr->tex->handle, 255, 255, 255);
                    if (it->tint_a != 255)
                        SDL_SetTextureAlphaMod(spr->tex->handle, 255);
                }
            }
        }
    }
    /* weapon overlays last */
    for (int i = 0; i < g_weapon_overlay_count; i++)
    {
        WeaponOverlayItem* w = &g_weapon_overlays[i];
        if (!w->tex)
            continue;
        SDL_FRect dst = {w->x, w->y, w->w, w->h};
        SDL_FPoint pivot = {w->w * w->pivot_x, w->h * w->pivot_y};
        SDL_SetTextureColorMod(w->tex, w->r, w->g, w->b);
        if (w->a != 255)
            SDL_SetTextureAlphaMod(w->tex, w->a);
        SDL_RenderCopyExF(g_app.renderer, w->tex, NULL, &dst, (double) w->angle, &pivot,
                          w->flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        SDL_SetTextureColorMod(w->tex, 255, 255, 255);
        if (w->a != 255)
            SDL_SetTextureAlphaMod(w->tex, 255);
    }
#else
    (void) g_items;
    (void) g_item_count; /* headless build: no-op */
#endif
}

void rogue_scene_drawlist_push_weapon_overlay(void* sdl_texture, float x, float y, float w, float h,
                                              float pivot_x, float pivot_y, float angle_deg,
                                              int flip, unsigned char r, unsigned char g,
                                              unsigned char b, unsigned char a)
{
#ifdef ROGUE_HAVE_SDL
    if (!sdl_texture)
        return;
    if (g_weapon_overlay_count >= (int) (sizeof(g_weapon_overlays) / sizeof(g_weapon_overlays[0])))
        return;
    WeaponOverlayItem* it = &g_weapon_overlays[g_weapon_overlay_count++];
    it->tex = (SDL_Texture*) sdl_texture;
    it->x = x;
    it->y = y;
    it->w = w;
    it->h = h;
    it->pivot_x = pivot_x;
    it->pivot_y = pivot_y;
    it->angle = angle_deg;
    it->flip = flip;
    it->r = r;
    it->g = g;
    it->b = b;
    it->a = a;
#else
    (void) sdl_texture;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    (void) pivot_x;
    (void) pivot_y;
    (void) angle_deg;
    (void) flip;
    (void) r;
    (void) g;
    (void) b;
    (void) a;
#endif
}
