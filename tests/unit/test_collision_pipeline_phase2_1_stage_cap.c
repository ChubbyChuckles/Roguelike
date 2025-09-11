/* test_collision_pipeline_phase2_1_stage_cap.c
 * Validates per-stage max_candidates cap enforcement in pipeline execution.
 */
#include "game/collision_pipeline.h"
#include <stdio.h>
#include <string.h>

static bool stage_passthrough(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    if (!ctx)
        return false;
    m->output_candidates = ctx->candidate_count;
    return true;
}

int main(void)
{
    RogueCollisionPipeline pipeline;
    rogue_collision_pipeline_init(&pipeline, ROGUE_COLLISION_BALANCED, 2.0f, false);
    /* Add two stages: first with cap=5, second without cap */
    if (!rogue_collision_pipeline_add_stage(&pipeline, "cap5", stage_passthrough, 0.f, 5))
        return 1;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "nocap", stage_passthrough, 0.f, 0))
        return 2;

    RogueCollisionCandidate cands[16];
    for (int i = 0; i < 16; ++i)
        cands[i].id = (uint32_t) i;

    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = cands;
    ctx.candidate_count = 12;

    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 3;

    if (pipeline.stages[0].metrics.output_candidates != 5)
    {
        fprintf(stderr, "stage cap not reflected in metrics: %u\n",
                pipeline.stages[0].metrics.output_candidates);
        return 4;
    }
    if (ctx.candidate_count != 5)
    {
        fprintf(stderr, "context candidate_count not capped: %u\n", ctx.candidate_count);
        return 5;
    }
    /* Second stage should see the capped count as input */
    if (pipeline.stages[1].metrics.input_candidates != 5)
    {
        fprintf(stderr, "second stage input not equal to capped count: %u\n",
                pipeline.stages[1].metrics.input_candidates);
        return 6;
    }
    return 0;
}
