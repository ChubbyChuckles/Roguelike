#ifndef ROGUE_GRAPHICS_FONT_H
#define ROGUE_GRAPHICS_FONT_H

#include <stddef.h>
#include "graphics/renderer.h"

/* Very small built-in 5x7 ASCII subset font (generated). */

typedef struct RogueBitmapFont {
    int glyph_w;
    int glyph_h;
} RogueBitmapFont;

extern const RogueBitmapFont g_rogue_builtin_font;

/* Draw text using built-in font at pixel position (x,y), scale = integer scale (>=1). */
void rogue_font_draw_text(int x, int y, const char* text, int scale, RogueColor color);

#endif
