#ifndef ROGUE_GRAPHICS_RENDERER_H
#define ROGUE_GRAPHICS_RENDERER_H

typedef struct RogueRenderer {
	int dummy; /* Placeholder until expanded */
} RogueRenderer;

typedef struct RogueColor { unsigned char r,g,b,a; } RogueColor;

int rogue_renderer_init(RogueRenderer *r);
void rogue_renderer_shutdown(RogueRenderer *r);
void rogue_renderer_set_draw_color(RogueRenderer *r, RogueColor c);
void rogue_renderer_clear(RogueRenderer *r);
void rogue_renderer_present(RogueRenderer *r);

#endif
