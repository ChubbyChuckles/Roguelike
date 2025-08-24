#include "../../src/ai/core/ai_profiler.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    rogue_ai_profiler_reset_for_tests();
    rogue_ai_profiler_set_budget_ms(1.0); /* 1 ms */
    rogue_ai_profiler_begin_frame();
    /* Simulate 10 agents costing total 0.8ms */
    for (int i = 0; i < 10; i++)
    {
        rogue_ai_profiler_record_agent(0.08);
    }
    rogue_ai_profiler_end_frame();
    RogueAIProfileSnapshot snap;
    rogue_ai_profiler_snapshot(&snap);
    assert(snap.frame_agent_count == 10 && snap.budget_exceeded == 0);
    /* Next frame exceed budget */
    rogue_ai_profiler_begin_frame();
    for (int i = 0; i < 15; i++)
    {
        rogue_ai_profiler_record_agent(0.09);
    } /* 1.35ms */
    rogue_ai_profiler_end_frame();
    rogue_ai_profiler_snapshot(&snap);
    assert(snap.frame_agent_count == 15 && snap.budget_exceeded == 1);
    printf("AI_PHASE11_PERF_GUARD_OK frame_total=%.2f exceed=%d\n", snap.frame_total_ms,
           snap.budget_exceeded);
    return 0;
}
