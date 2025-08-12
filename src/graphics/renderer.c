#include "graphics/renderer.h"

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
SDL_Renderer *g_internal_sdl_renderer_ref = 0; /* shared with app.c */
#endif

int rogue_renderer_init(RogueRenderer *r) {
	r->dummy = 0;
	return 1;
}

void rogue_renderer_shutdown(RogueRenderer *r) { (void)r; }

void rogue_renderer_set_draw_color(RogueRenderer *r, RogueColor c) {
	(void)r;
#ifndef ROGUE_HAVE_SDL
	(void)c; /* suppress unused parameter when SDL disabled */
#endif
#ifdef ROGUE_HAVE_SDL
	if (g_internal_sdl_renderer_ref) {
		SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref, c.r, c.g, c.b, c.a);
	}
#endif
}

void rogue_renderer_clear(RogueRenderer *r) {
	(void)r;
#ifdef ROGUE_HAVE_SDL
	if (g_internal_sdl_renderer_ref) SDL_RenderClear(g_internal_sdl_renderer_ref);
#endif
}

void rogue_renderer_present(RogueRenderer *r) {
	(void)r;
#ifdef ROGUE_HAVE_SDL
	if (g_internal_sdl_renderer_ref) SDL_RenderPresent(g_internal_sdl_renderer_ref);
#endif
}
