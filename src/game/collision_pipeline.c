/* collision_pipeline.c - Milestone 2.1 initial executable slice
 * Provides a stub execution framework for multi-stage collision processing.
 * The intent is to allow incremental landing of later advanced features
 * (adaptive quality, spatial partition, SIMD) without blocking integration.
 */
#include "game/collision_pipeline.h"
#include <string.h>
#include <time.h>

/* Simple timing helper using clock() (portable, coarse). Future slice: high-res timer. */
static float measure_ms(clock_t start, clock_t end)
{
    if (end < start)
        return 0.f;
    return 1000.0f * (float) (end - start) / (float) CLOCKS_PER_SEC;
}

bool rogue_collision_pipeline_execute(RogueCollisionPipeline* p, RogueCollisionContext* ctx,
                                      float simulated_stage_cost_ms[])
{
    if (!p || !ctx)
        return false;
    ctx->quality_level = p->quality_level;
    p->total_last_ms = 0.f;
    for (uint8_t i = 0; i < p->stage_count; ++i)
    {
        RogueCollisionStage* s = &p->stages[i];
        clock_t c0 = clock();
        /* Optional simulated cost injection for deterministic unit tests. */
        if (simulated_stage_cost_ms && simulated_stage_cost_ms[i] > 0.f)
        {
            /* Busy wait (coarse) to simulate compute; bounded to < 5ms to avoid slowing tests. */
            float target = simulated_stage_cost_ms[i];
            if (target > 5.f)
                target = 5.f;
            clock_t spin_start = clock();
            while (measure_ms(spin_start, clock()) < target)
            {
                /* spin */
            }
        }
        /* Populate metrics pre-call */
        s->metrics.input_candidates = ctx->candidate_count;
        bool cont = s->stage_func ? s->stage_func(ctx, &s->metrics) : true;
        clock_t c1 = clock();
        s->metrics.last_ms = measure_ms(c0, c1);
        /* Exponential moving average (alpha = 0.1) */
        if (s->metrics.calls == 0)
            s->metrics.avg_ms = s->metrics.last_ms;
        else
            s->metrics.avg_ms = s->metrics.avg_ms * 0.9f + s->metrics.last_ms * 0.1f;
        s->metrics.calls++;
        p->total_last_ms += s->metrics.last_ms;
        if (!cont)
            break; /* early exit */
    }
    /* Adaptive quality placeholder: future slice inspects p->total_last_ms vs budget. */
    return true;
}
