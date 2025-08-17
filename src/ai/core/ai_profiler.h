// AI Profiler (Phase 9.1 Per-frame AI time budget monitoring)
#ifndef ROGUE_AI_PROFILER_H
#define ROGUE_AI_PROFILER_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RogueAIProfileSnapshot {
    double frame_total_ms;      /* Sum of all recorded agent tick durations this frame */
    double frame_max_agent_ms;  /* Longest single agent tick */
    int    frame_agent_count;   /* Number of agent ticks recorded */
    int    budget_exceeded;     /* 1 if frame_total_ms > budget */
    double budget_ms;           /* Current configured budget */
} RogueAIProfileSnapshot;

void rogue_ai_profiler_set_budget_ms(double ms);
double rogue_ai_profiler_get_budget_ms(void);
void rogue_ai_profiler_begin_frame(void);
void rogue_ai_profiler_record_agent(double ms);
void rogue_ai_profiler_end_frame(void);
void rogue_ai_profiler_snapshot(RogueAIProfileSnapshot* out);
void rogue_ai_profiler_reset_for_tests(void);

#ifdef __cplusplus
}
#endif
#endif
