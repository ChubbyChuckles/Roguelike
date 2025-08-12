#include "core/app.h"
#include "core/game_loop.h"
#include "util/log.h"

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

typedef struct RogueAppState {
    RogueAppConfig cfg;
#ifdef ROGUE_HAVE_SDL
    SDL_Window *window;
    SDL_Renderer *renderer;
#endif
    int frame_count;
} RogueAppState;

static RogueAppState g_app;

bool rogue_app_init(const RogueAppConfig *cfg) {
    g_app.cfg = *cfg;
#ifdef ROGUE_HAVE_SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        ROGUE_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    g_app.window = SDL_CreateWindow(cfg->window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    cfg->window_width, cfg->window_height, 0);
    if (!g_app.window) {
        ROGUE_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    g_app.renderer = SDL_CreateRenderer(g_app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_app.renderer) {
        ROGUE_LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    /* Temporary exposure of SDL_Renderer for renderer abstraction */
    extern SDL_Renderer *g_internal_sdl_renderer_ref;
    g_internal_sdl_renderer_ref = g_app.renderer;
#else
    (void)cfg;
#endif
    RogueGameLoopConfig loop_cfg = { .target_fps = cfg->target_fps };
    rogue_game_loop_init(&loop_cfg);
    return true;
}

static void process_events(void) {
#ifdef ROGUE_HAVE_SDL
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            rogue_game_loop_request_exit();
        }
    }
#endif
}

void rogue_app_step(void) {
    if (!g_game_loop.running) return;
    process_events();
#ifdef ROGUE_HAVE_SDL
        SDL_SetRenderDrawColor(g_app.renderer, 12, 18, 32, 255);
        SDL_RenderClear(g_app.renderer);
        SDL_RenderPresent(g_app.renderer);
#endif
        rogue_game_loop_iterate();
        g_app.frame_count++;
}

void rogue_app_run(void) {
    while (g_game_loop.running) {
        rogue_app_step();
    }
}

void rogue_app_shutdown(void) {
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer) SDL_DestroyRenderer(g_app.renderer);
    if (g_app.window) SDL_DestroyWindow(g_app.window);
    SDL_Quit();
#endif
}

int rogue_app_frame_count(void) { return g_app.frame_count; }
