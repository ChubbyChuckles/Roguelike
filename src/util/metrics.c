/**
 * @file metrics.c
 * @brief Performance and session metrics tracking for the roguelike game.
 * @details This module provides functions to track frame timing, FPS, session duration,
 * and item drop/pickup statistics. Originally extracted from app.c for better organization.
 */

/* Metrics implementation extracted from app.c */
#include "metrics.h"
#include "../core/app/app_state.h"
#include "../core/loot/loot_rarity_adv.h"
#include "../game/game_loop.h"
#include <time.h>

/**
 * @brief Gets the current time in seconds since program start.
 * @return Current time in seconds as a double.
 * @details Uses clock() function for timing measurements, normalized to seconds.
 */
static double now_seconds(void) { return (double) clock() / (double) CLOCKS_PER_SEC; }

/**
 * @brief Resets all metrics to initial state.
 * @details Clears frame counters, timing accumulators, and session statistics.
 * Sets session start time to current time.
 */
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

/**
 * @brief Marks the beginning of a frame for timing.
 * @return Current time in seconds when frame began.
 * @details Should be called at the start of each frame to capture timing data.
 */
double rogue_metrics_frame_begin(void) { return now_seconds(); }

/**
 * @brief Marks the end of a frame and calculates timing metrics.
 * @param frame_start_seconds The time when the frame began (from rogue_metrics_frame_begin).
 * @details Calculates frame time, FPS, and updates rolling averages.
 * Uses deterministic dt for uncapped framerate to ensure consistent behavior in tests.
 */
void rogue_metrics_frame_end(double frame_start_seconds)
{
    g_app.frame_count++;
    double frame_end = now_seconds();
    g_app.frame_ms = (frame_end - frame_start_seconds) * 1000.0;
    /* Deterministic dt: when uncapped (target_fps==0), use a fixed small step to
       avoid wall-clock variance in unit tests and headless runs. */
    if (g_game_loop.target_frame_seconds > 0.0)
    {
        g_app.dt = g_game_loop.target_frame_seconds;
    }
    else
    {
        g_app.dt = 0.0083; /* ~120 FPS fixed step for uncapped mode */
    }
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

/**
 * @brief Retrieves current performance metrics.
 * @param out_fps Pointer to store current FPS (can be NULL).
 * @param out_frame_ms Pointer to store current frame time in milliseconds (can be NULL).
 * @param out_avg_frame_ms Pointer to store average frame time in milliseconds (can be NULL).
 * @details Provides access to the most recent frame timing data and rolling averages.
 */
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

/**
 * @brief Gets the elapsed time since the session started.
 * @return Session duration in seconds.
 * @details Calculates time since rogue_metrics_reset was last called.
 */
double rogue_metrics_session_elapsed(void) { return now_seconds() - g_app.session_start_seconds; }

/**
 * @brief Records an item drop event for session statistics.
 * @param rarity The rarity level of the dropped item (0-4).
 * @details Increments total drops counter and rarity-specific counters.
 */
void rogue_metrics_record_drop(int rarity)
{
    g_app.session_items_dropped++;
    if (rarity >= 0 && rarity < 5)
        g_app.session_rarity_drops[rarity]++;
}

/**
 * @brief Records an item pickup event for session statistics.
 * @param rarity The rarity level of the picked up item (unused parameter).
 * @details Increments total pickups counter. Rarity parameter currently unused.
 */
void rogue_metrics_record_pickup(int rarity)
{
    g_app.session_items_picked++;
    (void) rarity;
}

/**
 * @brief Calculates session rates for items and rarities.
 * @param out_items_per_hour Pointer to store items dropped per hour (can be NULL).
 * @param out_rarity_per_hour Array to store rarity drop rates per hour (can be NULL).
 * @details Computes hourly rates based on session elapsed time and recorded events.
 */
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

/**
 * @brief Gets the current delta time (time step) for the frame.
 * @return Current delta time in seconds.
 * @details Returns the time step used for game logic updates.
 */
double rogue_metrics_delta_time(void) { return g_app.dt; }
