/* test_collision_pipeline_phase2_1_temporal.c
 * Validates temporal coherence cache stage:
 *  - First frame populates cache (miss)
 *  - Second frame with stable view & unchanged candidate ids hits cache and skips spatial stage
 * work
 *  - Moving view invalidates cache (miss -> no skip)
 */
#include "game/collision_pipeline.h"
#include <stdio.h>
#include <string.h>

static bool stage_temporal(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_temporal_cache(ctx, m);
}
static bool stage_spatial(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_spatial_cull(ctx, m);
}
static bool stage_aabb(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_aabb_prefilter(ctx, m);
}

int main(void)
{
    RogueCollisionPipeline pipeline;
    rogue_collision_pipeline_init(&pipeline, ROGUE_COLLISION_BALANCED, 1.0f, true);
    if (!rogue_collision_pipeline_add_stage(&pipeline, "temporal", stage_temporal, 0.f, 0))
        return 1;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "spatial", stage_spatial, 0.f, 0))
        return 2;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "aabb", stage_aabb, 0.f, 0))
        return 3;

    RogueCollisionCandidate cands[8];
    for (int i = 0; i < 8; ++i)
    {
        cands[i].id = (uint32_t) i;
        cands[i].x = (float) (i * 5);
        cands[i].y = (float) (i * 3);
        cands[i].vx = cands[i].vy = 0.f;
        cands[i].half_w = cands[i].half_h = 1.f;
    }

    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = cands;
    ctx.candidate_count = 8;
    ctx.view_x = 0;
    ctx.view_y = 0;
    ctx.view_w = 100;
    ctx.view_h = 100;

    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 4;                                                  /* populate cache */
    float first_spatial_time = pipeline.stages[1].metrics.last_ms; /* spatial recorded */
    if (first_spatial_time <= 0.f)
    {
        fprintf(stderr, "expected spatial to run first frame\n");
        return 5;
    }

    /* Second frame unchanged -> temporal cache should skip spatial (time ~0) */
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 6;
    float second_spatial_time = pipeline.stages[1].metrics.last_ms;
    if (second_spatial_time > first_spatial_time * 0.5f)
    {
        fprintf(stderr, "spatial not skipped on temporal hit\n");
        return 7;
    }

    /* Move view enough to invalidate cache */
    ctx.view_x += 10.f;
    ctx.view_y += 10.f;
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 8;
    float third_spatial_time = pipeline.stages[1].metrics.last_ms;
    if (third_spatial_time <= second_spatial_time * 1.5f)
    {
        fprintf(stderr, "expected spatial to run after view move\n");
        return 9;
    }

    return 0;
}
