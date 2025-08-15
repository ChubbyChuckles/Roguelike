/* Lifecycle & misc controls extracted from app.c */
#include "core/app.h"
#include "core/app_state.h"
#include "core/metrics.h"
#include "core/platform.h"
#include "core/start_screen.h"
#include "core/game_loop.h" /* g_game_loop */
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

int rogue_app_player_health(void){ return g_app.player.health; }

void rogue_app_run(void){ while (g_game_loop.running){ rogue_app_step(); } }

int rogue_app_frame_count(void) { return g_app.frame_count; }
int rogue_app_enemy_count(void) { return g_app.enemy_count; }
void rogue_app_skip_start_screen(void){ g_app.show_start_screen = 0; }

void rogue_app_toggle_fullscreen(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.window) return;
    if (g_app.cfg.window_mode == ROGUE_WINDOW_FULLSCREEN)
        g_app.cfg.window_mode = ROGUE_WINDOW_WINDOWED;
    else
        g_app.cfg.window_mode = ROGUE_WINDOW_FULLSCREEN;
    rogue_platform_apply_window_mode();
#endif
}

void rogue_app_set_vsync(int enabled)
{
#ifdef ROGUE_HAVE_SDL
    (void) enabled; /* not dynamically supported without renderer recreation */
#else
    (void) enabled;
#endif
}

void rogue_app_get_metrics(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms){
    rogue_metrics_get(out_fps, out_frame_ms, out_avg_frame_ms);
}

double rogue_app_delta_time(void){ return rogue_metrics_delta_time(); }
