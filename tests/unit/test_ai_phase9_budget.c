#define SDL_MAIN_HANDLED 1
#include <assert.h>
#include <stdio.h>
#include "ai/core/ai_profiler.h"

int main(void){
    rogue_ai_profiler_reset_for_tests();
    rogue_ai_profiler_set_budget_ms(0.50);
    rogue_ai_profiler_begin_frame();
    for(int i=0;i<5;i++) rogue_ai_profiler_record_agent(0.08); /* 0.40ms */
    RogueAIProfileSnapshot snap; rogue_ai_profiler_snapshot(&snap);
    assert(snap.frame_agent_count==5);
    assert(snap.budget_exceeded==0);
    rogue_ai_profiler_record_agent(0.20); /* exceed */
    rogue_ai_profiler_snapshot(&snap);
    assert(snap.frame_agent_count==6);
    assert(snap.budget_exceeded==1);
    printf("AI_BUDGET_OK total=%.3f max=%.3f count=%d exceeded=%d budget=%.2f\n", snap.frame_total_ms, snap.frame_max_agent_ms, snap.frame_agent_count, snap.budget_exceeded, snap.budget_ms);
    return 0;
}
