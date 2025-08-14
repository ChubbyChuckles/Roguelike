#ifndef ROGUE_SCENE_DRAWLIST_H
#define ROGUE_SCENE_DRAWLIST_H
#include "graphics/sprite.h"
#include "core/app_state.h"
#ifdef __cplusplus
extern "C" { 
#endif

typedef enum RogueDrawKind { ROGUE_DRAW_SPRITE=0 } RogueDrawKind;

typedef struct RogueDrawItem {
    RogueDrawKind kind;
    const RogueSprite* sprite; /* optional; when NULL color rect? (future) */
    int sx, sy, sw, sh; /* source override if sprite null */
    int dx, dy, dw, dh; /* destination rect */
    int y_sort; /* key */
    unsigned char flip; /* SDL_RendererFlip bits (0 or 1 for horizontal) */
    unsigned char tint_r, tint_g, tint_b, tint_a; /* modulation */
} RogueDrawItem;

void rogue_scene_drawlist_begin(void);
void rogue_scene_drawlist_push_sprite(const RogueSprite* spr,int dx,int dy,int y_base,int flip,unsigned char r,unsigned char g,unsigned char b,unsigned char a);
void rogue_scene_drawlist_flush(void);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_SCENE_DRAWLIST_H */
