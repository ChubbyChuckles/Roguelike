#include "ai/core/ai_profiler.h"

static double g_budget_ms = 1.0; /* default 1ms */
static double g_accum_ms = 0.0;
static double g_max_agent_ms = 0.0;
static int g_agent_count = 0;
static int g_budget_exceeded = 0;

void rogue_ai_profiler_set_budget_ms(double ms)
{
    if (ms <= 0.0)
        ms = 0.0001;
    g_budget_ms = ms;
}
double rogue_ai_profiler_get_budget_ms(void) { return g_budget_ms; }
void rogue_ai_profiler_begin_frame(void)
{
    g_accum_ms = 0.0;
    g_max_agent_ms = 0.0;
    g_agent_count = 0;
    g_budget_exceeded = 0;
}
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
void rogue_ai_profiler_end_frame(void) {}
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
void rogue_ai_profiler_reset_for_tests(void)
{
    rogue_ai_profiler_set_budget_ms(1.0);
    rogue_ai_profiler_begin_frame();
}
