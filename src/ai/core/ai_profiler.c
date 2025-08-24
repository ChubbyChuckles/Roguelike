/**
 * @file ai_profiler.c
 * @brief Lightweight AI profiler / budget tracker used by AI systems and tests.
 *
 * The profiler accumulates per-agent timings into a per-frame total and
 * exposes lightweight diagnostics: total frame time, maximum single-agent
 * time, agent count, and whether the configured budget was exceeded.
 *
 * This module is intentionally simple and not thread-safe. It is intended
 * for use from a single-threaded update loop or with external synchronization.
 */
#include "ai_profiler.h"

/** Default per-frame AI budget in milliseconds. */
static double g_budget_ms = 1.0; /* default 1ms */
/** Accumulated milliseconds used so far this frame. */
static double g_accum_ms = 0.0;
/** Maximum milliseconds consumed by a single agent this frame. */
static double g_max_agent_ms = 0.0;
/** Number of agents recorded this frame. */
static int g_agent_count = 0;
/** Budget-exceeded flag for the current frame (1 if exceeded, else 0). */
static int g_budget_exceeded = 0;

/**
 * @brief Set the per-frame AI budget in milliseconds.
 *
 * ms must be positive; values <= 0 are clamped to a small >0 value to avoid
 * divide-by-zero or zero-budget semantics.
 *
 * @param ms Budget in milliseconds (positive). Non-positive values are clamped.
 */
void rogue_ai_profiler_set_budget_ms(double ms)
{
    if (ms <= 0.0)
        ms = 0.0001;
    g_budget_ms = ms;
}

/**
 * @brief Get the configured per-frame AI budget in milliseconds.
 * @return double Current budget in milliseconds.
 */
double rogue_ai_profiler_get_budget_ms(void) { return g_budget_ms; }

/**
 * @brief Begin a new profiling frame.
 *
 * Resets accumulated counters and flags. Call at the start of the AI update
 * frame before recording per-agent timings.
 */
void rogue_ai_profiler_begin_frame(void)
{
    g_accum_ms = 0.0;
    g_max_agent_ms = 0.0;
    g_agent_count = 0;
    g_budget_exceeded = 0;
}

/**
 * @brief Record a single agent's elapsed time (in milliseconds) into the profiler.
 *
 * ms is clamped to >= 0. The function updates the running total, max per-agent
 * timing, agent count, and the budget-exceeded flag when the accumulated
 * total surpasses the configured budget.
 *
 * @param ms Elapsed milliseconds for the agent (will be clamped to >= 0).
 */
void rogue_ai_profiler_record_agent(double ms)
{
    if (ms < 0.0)
        ms = 0.0;
    g_accum_ms += ms;
    if (ms > g_max_agent_ms)
        g_max_agent_ms = ms;
    g_agent_count++;
    if (g_accum_ms > g_budget_ms)
        g_budget_exceeded = 1;
}

/**
 * @brief End of frame hook (no-op placeholder).
 *
 * Present for API symmetry; future instrumentation or rollover logic may be
 * placed here.
 */
void rogue_ai_profiler_end_frame(void) {}

/**
 * @brief Snapshot current profiler state into the provided structure.
 *
 * Copies totals, max agent time, agent count, budget flag, and configured
 * budget to the out snapshot structure. No validation is performed beyond
 * a NULL check for the output pointer.
 *
 * @param out Pointer to a RogueAIProfileSnapshot structure to receive values.
 */
void rogue_ai_profiler_snapshot(RogueAIProfileSnapshot* out)
{
    if (!out)
        return;
    out->frame_total_ms = g_accum_ms;
    out->frame_max_agent_ms = g_max_agent_ms;
    out->frame_agent_count = g_agent_count;
    out->budget_exceeded = g_budget_exceeded;
    out->budget_ms = g_budget_ms;
}

/**
 * @brief Reset profiler state for unit tests.
 *
 * Restores the default budget (1.0 ms) and clears frame counters. Intended
 * solely for test isolation.
 */
void rogue_ai_profiler_reset_for_tests(void)
{
    rogue_ai_profiler_set_budget_ms(1.0);
    rogue_ai_profiler_begin_frame();
}
