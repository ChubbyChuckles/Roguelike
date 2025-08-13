/* Metrics implementation extracted from app.c */
#include <time.h>
#include "core/app_state.h"
#include "core/game_loop.h"
#include "core/metrics.h"

static double now_seconds(void) { return (double) clock() / (double) CLOCKS_PER_SEC; }

void rogue_metrics_reset(void){
    g_app.frame_count = 0; g_app.dt = 0.0; g_app.fps = 0.0; g_app.frame_ms = 0.0;
    g_app.avg_frame_ms_accum = 0.0; g_app.avg_frame_samples = 0;
}

double rogue_metrics_frame_begin(void){
    return now_seconds();
}

void rogue_metrics_frame_end(double frame_start_seconds){
    g_app.frame_count++;
    double frame_end = now_seconds();
    g_app.frame_ms = (frame_end - frame_start_seconds) * 1000.0;
    g_app.dt = (g_game_loop.target_frame_seconds > 0.0) ? g_game_loop.target_frame_seconds : (frame_end - frame_start_seconds);
    if(g_app.dt < 0.0083) g_app.dt = 0.0083; /* simulate ~120 FPS minimum */
    if (g_app.dt < 0.0001) g_app.dt = 0.0001; /* clamp absurdly low */
    g_app.fps = (g_app.dt > 0.0) ? 1.0 / g_app.dt : 0.0;
    g_app.avg_frame_ms_accum += g_app.frame_ms;
    g_app.avg_frame_samples++;
    if (g_app.avg_frame_samples >= 120){
        g_app.avg_frame_ms_accum = g_app.avg_frame_ms_accum / g_app.avg_frame_samples;
        g_app.avg_frame_samples = 0; /* reuse accum as average holder */
    }
}

void rogue_metrics_get(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms){
    if(out_fps) *out_fps = g_app.fps;
    if(out_frame_ms) *out_frame_ms = g_app.frame_ms;
    if(out_avg_frame_ms){
        double avg = (g_app.avg_frame_samples == 0 && g_app.avg_frame_ms_accum > 0.0)
                      ? g_app.avg_frame_ms_accum
                      : (g_app.avg_frame_ms_accum / (g_app.avg_frame_samples ? g_app.avg_frame_samples : 1));
        *out_avg_frame_ms = avg;
    }
}

double rogue_metrics_delta_time(void){ return g_app.dt; }
