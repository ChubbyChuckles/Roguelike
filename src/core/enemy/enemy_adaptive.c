/* enemy_adaptive.c - Phase 4 adaptive difficulty (bounded scalar based on recent KPIs)
 * KPIs tracked:
 *  - avg_ttk: exponential moving average of time-to-kill (seconds)
 *  - dmg_intake_rate: EMA of player damage taken per second
 *  - potion_rate: EMA of potion uses per minute
 *  - death_rate: EMA of player deaths per hour (scaled)
 * Adjustment logic (simple first slice):
 *  - Baseline target TTK (same-level normal): 6.0s reference
 *  - If avg_ttk < 0.6 * target AND dmg_intake_rate low AND potion_rate low => increase scalar
 * toward +12%
 *  - If avg_ttk > 1.6 * target OR dmg_intake_rate high OR frequent potions/deaths => decrease
 * scalar toward -12%
 *  - Otherwise decay scalar toward 1.0
 * Smoothing: EMA alpha derived from dt or event-driven for numerical stability.
 */
#include "core/enemy/enemy_adaptive.h"
#include <math.h>

/* Static global initialized explicitly so that even if reset() is not called
 * (older tests / legacy init paths) we start from a neutral (scalar=1) state. */
static struct AdaptiveState
{
    float avg_ttk;
    int has_ttk;
    float dmg_intake_rate; /* per second */
    float potion_rate;     /* per minute */
    float death_rate;      /* per hour */
    float scalar;          /* current applied scalar */
    int enabled;
    float time_since_last_kill;
    float recent_kill_pressure; /* decays over short window to gate adjustments */
    int kill_event;             /* set when a kill submitted since last tick */
} g_adapt = {0, 0, 0, 0, 0, 1.0f, 1, 1000.0f, 0, 0};

static float ema(float prev, float sample, float alpha, int has_prev)
{
    return has_prev ? (prev + alpha * (sample - prev)) : sample;
}

void rogue_enemy_adaptive_reset(void)
{
    g_adapt.avg_ttk = 0;
    g_adapt.has_ttk = 0;
    g_adapt.dmg_intake_rate = 0;
    g_adapt.potion_rate = 0;
    g_adapt.death_rate = 0;
    g_adapt.scalar = 1.0f;
    g_adapt.enabled = 1;
    g_adapt.time_since_last_kill = 1000.0f;
    g_adapt.recent_kill_pressure = 0;
    g_adapt.kill_event = 0;
}
void rogue_enemy_adaptive_set_enabled(int enabled)
{
    g_adapt.enabled = enabled ? 1 : 0;
    if (!g_adapt.enabled)
        g_adapt.scalar = 1.0f;
}
int rogue_enemy_adaptive_enabled(void) { return g_adapt.enabled; }

void rogue_enemy_adaptive_submit_kill(float ttk_seconds)
{
    if (ttk_seconds <= 0)
        return;
    g_adapt.avg_ttk = ema(g_adapt.avg_ttk, ttk_seconds, 0.20f, g_adapt.has_ttk);
    g_adapt.has_ttk = 1;
    g_adapt.time_since_last_kill = 0.0f;
    g_adapt.recent_kill_pressure += 1.0f;
    g_adapt.kill_event = 1;
}
void rogue_enemy_adaptive_submit_player_damage(float dmg, float interval_seconds)
{
    if (dmg < 0 || interval_seconds <= 0)
        return;
    float rate = dmg / interval_seconds;
    g_adapt.dmg_intake_rate = ema(g_adapt.dmg_intake_rate, rate, 0.10f, 1);
}
void rogue_enemy_adaptive_submit_potion_used(void)
{ /* treat as 1 event; convert to per-minute rate via tick smoothing */ g_adapt.potion_rate += 1.0f;
}
void rogue_enemy_adaptive_submit_player_death(void) { g_adapt.death_rate += 1.0f; }

void rogue_enemy_adaptive_tick(float dt_seconds)
{
    if (dt_seconds <= 0)
        return;
    if (!g_adapt.enabled)
    {
        g_adapt.scalar = 1.0f;
        return;
    }
    g_adapt.time_since_last_kill += dt_seconds;
    /* Decay event counters into rates */
    float pot_alpha = dt_seconds / 60.0f;
    if (pot_alpha > 1)
        pot_alpha = 1;
    g_adapt.potion_rate = ema(g_adapt.potion_rate, 0.0f, pot_alpha,
                              1); /* event injections add discrete +1 before decay */
    float death_alpha = dt_seconds / 3600.0f;
    if (death_alpha > 1)
        death_alpha = 1;
    g_adapt.death_rate = ema(g_adapt.death_rate, 0.0f, death_alpha, 1);
    /* Short-window pressure decay (5s window) */
    if (g_adapt.recent_kill_pressure > 0)
    {
        float decay = dt_seconds / 5.0f;
        if (decay > 1)
            decay = 1;
        g_adapt.recent_kill_pressure -= decay;
        if (g_adapt.recent_kill_pressure < 0)
            g_adapt.recent_kill_pressure = 0;
    }
    /* Derive target conditions */
    const float target_ttk = 6.0f;
    int increase_pressure = 0, decrease_pressure = 0;
    /* Only treat recent kills as active performance window; after that we allow rapid relaxation.
     */
    int active_window = (g_adapt.time_since_last_kill < 5.0f);
    int kill_event = g_adapt.kill_event;
    g_adapt.kill_event = 0; /* consume */
    if (active_window && kill_event)
    {
        if (g_adapt.has_ttk)
        {
            if (g_adapt.avg_ttk < target_ttk * 0.60f && g_adapt.dmg_intake_rate < 3.0f &&
                g_adapt.potion_rate < 0.2f)
                increase_pressure = 1;
            if (g_adapt.avg_ttk > target_ttk * 1.60f || g_adapt.dmg_intake_rate > 12.0f ||
                g_adapt.potion_rate > 1.2f || g_adapt.death_rate > 0.15f)
                decrease_pressure = 1;
        }
    }
    else
    {
        /* Inactive window: gently revert avg_ttk toward target to prevent stale fast-kill pressure
         * lingering. */
        if (g_adapt.has_ttk)
        {
            float relax_alpha = fminf(g_adapt.time_since_last_kill / 30.0f, 1.0f) *
                                0.15f; /* faster as we stay idle */
            g_adapt.avg_ttk = g_adapt.avg_ttk + (target_ttk - g_adapt.avg_ttk) * relax_alpha;
        }
    }
    float target_scalar = 1.0f;
    if (increase_pressure)
        target_scalar = ROGUE_ENEMY_ADAPTIVE_MAX_SCALAR;
    else if (decrease_pressure)
        target_scalar = ROGUE_ENEMY_ADAPTIVE_MIN_SCALAR;
    /* Move scalar toward target with small step */
    float step = 0.05f;
    g_adapt.scalar += (target_scalar - g_adapt.scalar) * step;
    /* debug logging removed */
    if (!increase_pressure && !decrease_pressure)
    {
        /* Neutral: accelerate convergence toward 1 (stronger) */
        g_adapt.scalar += (1.0f - g_adapt.scalar) * 0.30f;
        if (fabsf(g_adapt.scalar - 1.0f) < 0.002f)
            g_adapt.scalar = 1.0f; /* snap close */
    }
    if (g_adapt.scalar < ROGUE_ENEMY_ADAPTIVE_MIN_SCALAR)
        g_adapt.scalar = ROGUE_ENEMY_ADAPTIVE_MIN_SCALAR;
    if (g_adapt.scalar > ROGUE_ENEMY_ADAPTIVE_MAX_SCALAR)
        g_adapt.scalar = ROGUE_ENEMY_ADAPTIVE_MAX_SCALAR;
}

float rogue_enemy_adaptive_scalar(void) { return g_adapt.enabled ? g_adapt.scalar : 1.0f; }
