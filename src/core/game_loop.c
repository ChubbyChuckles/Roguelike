#include "core/game_loop.h"
#include <time.h>

RogueGameLoopState g_game_loop;

static double now_seconds(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

void rogue_game_loop_init(const RogueGameLoopConfig *cfg) {
    g_game_loop.cfg = *cfg;
    g_game_loop.running = true;
    g_game_loop.target_frame_seconds = (cfg->target_fps > 0) ? 1.0 / (double)cfg->target_fps : 0.0;
}

void rogue_game_loop_iterate(void) {
    static double last = 0.0;
    if (last == 0.0) last = now_seconds();
    double start = now_seconds();
    double dt = start - last;
    last = start;
    (void)dt;
    if (g_game_loop.target_frame_seconds > 0.0) {
        for (;;) {
            double elapsed = now_seconds() - start;
            if (elapsed >= g_game_loop.target_frame_seconds) break;
        }
    }
}

void rogue_game_loop_request_exit(void) { g_game_loop.running = false; }
