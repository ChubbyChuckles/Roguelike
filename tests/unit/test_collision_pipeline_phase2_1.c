/* test_collision_pipeline_phase2_1.c
 * Validates initial Milestone 2.1 collision pipeline slice:
 *  - Stage addition & execution ordering
 *  - Metrics population (calls, input/output candidates)
 *  - Simulated cost accumulation & total pipeline time bookkeeping
 *  - Early-exit behavior
 */
#include "game/collision_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool stage_passthrough(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    (void) m; /* output_candidates left same as input for this stub */
    if (!ctx)
        return false;
    m->output_candidates = ctx->candidate_count; /* unchanged */
    return true;                                 /* continue */
}

static bool stage_consume_half(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    if (!ctx)
        return false;
    if (ctx->candidate_count > 0)
        ctx->candidate_count /= 2; /* reduce */
    m->output_candidates = ctx->candidate_count;
    return true; /* continue */
}

static bool stage_early_exit(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    if (!ctx)
        return false;
    m->output_candidates = ctx->candidate_count;
    return false; /* signal early exit */
}

int main(void)
{
    RogueCollisionPipeline pipeline;
    rogue_collision_pipeline_init(&pipeline, ROGUE_COLLISION_BALANCED, 4.0f, false);
    if (!rogue_collision_pipeline_add_stage(&pipeline, "pass", stage_passthrough, 0.f, 0))
        return 1;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "half", stage_consume_half, 0.f, 0))
        return 2;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "exit", stage_early_exit, 0.f, 0))
        return 3;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "unreached", stage_passthrough, 0.f, 0))
        return 4;

    RogueCollisionCandidate candidates[10];
    for (uint32_t i = 0; i < 10; ++i)
        candidates[i].id = i;
    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = candidates;
    ctx.candidate_count = 10;

    float simulated_costs[4] = {0.2f, 0.3f, 0.1f, 0.4f};
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, simulated_costs))
        return 5;

    if (pipeline.stage_count != 4)
    {
        fprintf(stderr, "unexpected stage count\n");
        return 6;
    }
    if (pipeline.stages[0].metrics.calls != 1 || pipeline.stages[1].metrics.calls != 1)
    {
        fprintf(stderr, "metrics calls mismatch\n");
        return 7;
    }
    if (pipeline.stages[2].metrics.calls != 1 || pipeline.stages[3].metrics.calls != 0)
    {
        fprintf(stderr, "early exit not respected\n");
        return 8;
    }
    if (pipeline.stages[1].metrics.output_candidates != 5)
    {
        fprintf(stderr, "half stage failed\n");
        return 9;
    }
    if (ctx.candidate_count != 5)
    {
        fprintf(stderr, "context count mismatch\n");
        return 10;
    }
    if (pipeline.total_last_ms <= 0.f)
    {
        fprintf(stderr, "no timing accumulated\n");
        return 11;
    }
    return 0;
}
