/**
 * @file ai_scheduler.c
 * @brief Simple AI scheduler used to spread behavior tree ticks across frames.
 *
 * The scheduler provides a configurable bucketed update scheme with a
 * level-of-detail radius test so distant enemies are only run through cheap
 * maintenance work. Tests may override bucket count and radius via the
 * provided helpers. This module is single-threaded and designed for
 * deterministic, frame-based updates.
 */
#include "ai/core/ai_scheduler.h"
#include "core/app_state.h"
#include "entities/enemy.h"
#include <math.h>

/** Current scheduler frame counter (monotonic). */
static unsigned int g_frame = 0;
/** Default bucket count (tests may override). */
static int g_buckets = 4; /* default bucket count (tests may override) */
/** Level-of-detail radius in tiles. */
static float g_lod_radius = 18.0f; /* tiles */
/** Precomputed squared LOD radius for distance comparisons. */
static float g_lod_radius_sq = 18.0f * 18.0f;

/**
 * @brief Set the number of update buckets used to spread work across frames.
 *
 * A value < 1 will be clamped to 1. Higher bucket counts reduce per-frame
 * behavior tree work by only processing a subset of enemies each frame.
 *
 * @param buckets Number of buckets (clamped to >= 1).
 */
void rogue_ai_scheduler_set_buckets(int buckets)
{
    if (buckets < 1)
        buckets = 1;
    g_buckets = buckets;
}

/**
 * @brief Get the currently configured bucket count.
 * @return int Current bucket count (>=1).
 */
int rogue_ai_scheduler_get_buckets(void) { return g_buckets; }

/**
 * @brief Set the AI level-of-detail radius in tiles.
 *
 * Negative radius values are clamped to zero. The squared radius is cached
 * for efficient distance tests.
 *
 * @param radius Radius in tiles (non-negative).
 */
void rogue_ai_lod_set_radius(float radius)
{
    if (radius < 0.0f)
        radius = 0.0f;
    g_lod_radius = radius;
    g_lod_radius_sq = radius * radius;
}

/**
 * @brief Get the configured LOD radius in tiles.
 * @return float LOD radius in tiles.
 */
float rogue_ai_lod_get_radius(void) { return g_lod_radius; }

/**
 * @brief Return the current scheduler frame counter.
 * @return unsigned int Current frame index (monotonic).
 */
unsigned int rogue_ai_scheduler_frame(void) { return g_frame; }

/**
 * @brief Reset scheduler state for unit tests.
 *
 * Resets frame counter, bucket count, and LOD radius to defaults to provide
 * deterministic test isolation.
 */
void rogue_ai_scheduler_reset_for_tests(void)
{
    g_frame = 0;
    g_buckets = 4;
    rogue_ai_lod_set_radius(18.0f);
}

/* Cheap maintenance tick: could handle timers/decay w/o full BT cost (placeholder). */
/**
 * @brief Lightweight maintenance update for distant or deferred enemies.
 *
 * Currently a placeholder that intentionally does nothing. Existing inline
 * comment notes possible future work: threat decay, status timers, etc.
 *
 * @param e Enemy to update.
 * @param dt Delta seconds for the maintenance tick.
 */
static void maintenance_tick(RogueEnemy* e, float dt)
{
    (void) dt; /* future: threat decay, status timers */
    (void) e;
}

/**
 * @brief Tick the AI scheduler over an array of enemies.
 *
 * The scheduler applies the following sequence per enemy:
 *  - Skip dead or BT-disabled enemies.
 *  - Perform an LOD distance test; if outside the configured radius, run the
 *    cheap maintenance tick instead of the full behavior tree.
 *  - If inside radius, use bucketed selection: only enemies whose index mod
 *    bucket_count equals the current bucket are processed this frame; others
 *    receive the maintenance tick.
 *  - For selected enemies, call rogue_enemy_ai_bt_tick to execute behavior tree logic.
 *
 * The function increments the internal frame counter each invocation. If
 * enemies is NULL or count <= 0 the function simply increments the frame and returns.
 *
 * @param enemies Pointer to a contiguous array of RogueEnemy objects.
 * @param count Number of enemies in the array.
 * @param dt_seconds Delta time in seconds for this tick.
 */
void rogue_ai_scheduler_tick(RogueEnemy* enemies, int count, float dt_seconds)
{
    if (!enemies || count <= 0)
    {
        g_frame++;
        return;
    }
    int bucket = g_frame % g_buckets;
    for (int i = 0; i < count; i++)
    {
        RogueEnemy* e = &enemies[i];
        if (!e->alive)
            continue;
        if (!e->ai_bt_enabled)
            continue; /* only BT-enabled enemies considered */
        /* LOD distance test */
        float dx = e->base.pos.x - g_app.player.base.pos.x;
        float dy = e->base.pos.y - g_app.player.base.pos.y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 > g_lod_radius_sq)
        {
            maintenance_tick(e, dt_seconds);
            continue;
        }
        /* Bucket selection: only process BT for enemies whose index mod bucket count equals current
         * bucket */
        if (g_buckets > 1)
        {
            if ((i % g_buckets) != bucket)
            {
                maintenance_tick(e, dt_seconds);
                continue;
            }
        }
        /* Full behavior tree tick */
        rogue_enemy_ai_bt_tick(e, dt_seconds);
    }
    g_frame++;
}
