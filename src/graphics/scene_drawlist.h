#ifndef ROGUE_SCENE_DRAWLIST_H
#define ROGUE_SCENE_DRAWLIST_H
#include "../core/app/app_state.h"
#include "sprite.h"
#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RogueDrawKind
    {
        ROGUE_DRAW_SPRITE = 0
    } RogueDrawKind;

    typedef struct RogueDrawItem
    {
        RogueDrawKind kind;
        const RogueSprite* sprite; /* optional; when NULL color rect? (future) */
        int sx, sy, sw, sh;        /* source override if sprite null */
        int dx, dy, dw, dh;        /* destination rect */
        int y_sort;                /* key */
        unsigned char flip;        /* SDL_RendererFlip bits (0 or 1 for horizontal) */
        unsigned char tint_r, tint_g, tint_b, tint_a; /* modulation */
    } RogueDrawItem;

    void rogue_scene_drawlist_begin(void);
    void rogue_scene_drawlist_push_sprite(const RogueSprite* spr, int dx, int dy, int y_base,
                                          int flip, unsigned char r, unsigned char g,
                                          unsigned char b, unsigned char a);
    void rogue_scene_drawlist_flush(void);

    /* Weapon overlay (single texture with transform) queued after normal sprites so it renders on
     * top */
    void rogue_scene_drawlist_push_weapon_overlay(void* sdl_texture, float x, float y, float w,
                                                  float h, float pivot_x, float pivot_y,
                                                  float angle_deg, int flip, unsigned char r,
                                                  unsigned char g, unsigned char b,
                                                  unsigned char a);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_SCENE_DRAWLIST_H */
