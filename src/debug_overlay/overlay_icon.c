#include "overlay_icon.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

#include "../core/app/app_state.h"
#include "overlay_theme.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* 12x12 monochrome bitmaps for a few common icons (1=on, 0=off). */
#define IW 12
#define IH 12
static const unsigned char ICON_SAVE[IH][IW] = {
    {0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0}, {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
    {1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0}, {1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0}, {1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0},
    {1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0}, {1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0},
    {1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static const unsigned char ICON_UNDO[IH][IW] = {
    {0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0},
    {0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0}, {1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
    {1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0}, {0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0},
    {0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static const unsigned char ICON_REDO[IH][IW] = {
    {0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
    {1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0}, {1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static const unsigned char ICON_PLAY[IH][IW] = {
    {0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0},
    {0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static const unsigned char ICON_SEARCH[IH][IW] = {
    {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0},
    {0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const unsigned char (*ICON_TABLE[OVERLAY_ICON_COUNT])[IH][IW] = {
    &ICON_SAVE, &ICON_UNDO, &ICON_REDO, &ICON_PLAY, &ICON_SEARCH,
};

void overlay_icon_draw(OverlayIcon icon, int x, int y, int scale)
{
    if (icon < 0 || icon >= OVERLAY_ICON_COUNT)
        return;
    if (scale < 1)
        scale = 1;
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        const unsigned char(*bmp)[IH][IW] = ICON_TABLE[icon];
        for (int iy = 0; iy < IH; ++iy)
        {
            for (int ix = 0; ix < IW; ++ix)
            {
                if ((*bmp)[iy][ix])
                {
                    SDL_Rect r = {x + ix * scale, y + iy * scale, scale, scale};
                    SDL_SetRenderDrawColor(g_app.renderer, th->text_accent.r, th->text_accent.g,
                                           th->text_accent.b, th->text_accent.a);
                    SDL_RenderFillRect(g_app.renderer, &r);
                }
            }
        }
    }
#else
    (void) x;
    (void) y;
    (void) scale;
    (void) icon;
#endif
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
