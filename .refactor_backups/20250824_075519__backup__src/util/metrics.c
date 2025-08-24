/* Metrics implementation extracted from app.c */
#include "metrics.h"
#include "app/app_state.h"
#include "game_loop.h"
#include "loot/loot_rarity_adv.h"
#include <time.h>

static double now_seconds(void) { return (double) clock() / (double) CLOCKS_PER_SEC; }

void rogue_metrics_reset(void)
{
    g_app.frame_count = 0;
    g_app.dt = 0.0;
    g_app.fps = 0.0;
    g_app.frame_ms = 0.0;
    g_app.avg_frame_ms_accum = 0.0;
    g_app.avg_frame_samples = 0;
    g_app.session_start_seconds = now_seconds();
    g_app.session_items_dropped = 0;
    g_app.session_items_picked = 0;
    for (int i = 0; i < 5; i++)
    {
        g_app.session_rarity_drops[i] = 0;
    }
}

double rogue_metrics_frame_begin(void) { return now_seconds(); }

void rogue_metrics_frame_end(double frame_start_seconds)
{
    g_app.frame_count++;
    double frame_end = now_seconds();
    g_app.frame_ms = (frame_end - frame_start_seconds) * 1000.0;
    g_app.dt = (g_game_loop.target_frame_seconds > 0.0) ? g_game_loop.target_frame_seconds
                                                        : (frame_end - frame_start_seconds);
    if (g_app.dt < 0.0083)
        g_app.dt = 0.0083; /* simulate ~120 FPS minimum */
    if (g_app.dt < 0.0001)
        g_app.dt = 0.0001; /* clamp absurdly low */
    g_app.fps = (g_app.dt > 0.0) ? 1.0 / g_app.dt : 0.0;
    g_app.avg_frame_ms_accum += g_app.frame_ms;
    g_app.avg_frame_samples++;
    if (g_app.avg_frame_samples >= 120)
    {
        g_app.avg_frame_ms_accum = g_app.avg_frame_ms_accum / g_app.avg_frame_samples;
        g_app.avg_frame_samples = 0; /* reuse accum as average holder */
    }
}

void rogue_metrics_get(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms)
{
    if (out_fps)
        *out_fps = g_app.fps;
    if (out_frame_ms)
        *out_frame_ms = g_app.frame_ms;
    if (out_avg_frame_ms)
    {
        double avg = (g_app.avg_frame_samples == 0 && g_app.avg_frame_ms_accum > 0.0)
                         ? g_app.avg_frame_ms_accum
                         : (g_app.avg_frame_ms_accum /
                            (g_app.avg_frame_samples ? g_app.avg_frame_samples : 1));
        *out_avg_frame_ms = avg;
    }
}

/* Session metrics API (9.5) */
double rogue_metrics_session_elapsed(void) { return now_seconds() - g_app.session_start_seconds; }
void rogue_metrics_record_drop(int rarity)
{
    g_app.session_items_dropped++;
    if (rarity >= 0 && rarity < 5)
        g_app.session_rarity_drops[rarity]++;
}
void rogue_metrics_record_pickup(int rarity)
{
    g_app.session_items_picked++;
    (void) rarity;
}
void rogue_metrics_rates(double* out_items_per_hour, double out_rarity_per_hour[5])
{
    double elapsed = rogue_metrics_session_elapsed();
    if (elapsed <= 0.0)
        elapsed = 1.0;
    double hours = elapsed / 3600.0;
    if (out_items_per_hour)
        *out_items_per_hour = g_app.session_items_dropped / hours;
    if (out_rarity_per_hour)
    {
        for (int i = 0; i < 5; i++)
        {
            out_rarity_per_hour[i] = g_app.session_rarity_drops[i] / hours;
        }
    }
}

double rogue_metrics_delta_time(void) { return g_app.dt; }
