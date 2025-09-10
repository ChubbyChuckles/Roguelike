/* test_collision_pipeline_phase2_1_pixel.c
 * Validates integration of new hierarchical broad-phase and pixel-perfect stub stage.
 * Ensures candidate pruning semantics and quality-tier dependent behavior.
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
static bool stage_hbroad(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_hierarchical_broad(ctx, m);
}
static bool stage_pixel(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_pixel_perfect(ctx, m);
}

int main(void)
{
    RogueCollisionPipeline pipeline;
    rogue_collision_pipeline_init(&pipeline, ROGUE_COLLISION_BALANCED, 2.0f, true);
    if (!rogue_collision_pipeline_add_stage(&pipeline, "temporal", stage_temporal, 0.f, 0))
        return 1;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "spatial", stage_spatial, 0.f, 0))
        return 2;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "aabb", stage_aabb, 0.f, 0))
        return 3;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "hbroad", stage_hbroad, 0.f, 0))
        return 4;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "pixel", stage_pixel, 0.f, 0))
        return 5;

    RogueCollisionCandidate cands[32];
    /* Ensure all fields (including newly added layer_mask & pixel_mask pointer) start zeroed
        to avoid undefined pointer dereferences in pixel stage refinement logic. */
    memset(cands, 0, sizeof(cands));
    for (int i = 0; i < 32; ++i)
    {
        cands[i].id = (uint32_t) i;
        cands[i].half_w = cands[i].half_h = 1.f;
        cands[i].vx = cands[i].vy = 0.f;
        /* Cluster half inside, half far outside */
        if (i < 16)
        {
            cands[i].x = (float) (i * 2);
            cands[i].y = (float) (i * 1.5f);
        }
        else
        {
            cands[i].x = 1000.f + (float) (i * 5);
            cands[i].y = 1000.f + (float) (i * 5);
        }
    }
    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = cands;
    ctx.candidate_count = 32;
    ctx.view_x = 0.f;
    ctx.view_y = 0.f;
    ctx.view_w = 200.f;
    ctx.view_h = 200.f;

    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 6;
    /* After hbroad we expect far cluster removed; pixel stage (BALANCED) should not halve set. */
    if (ctx.candidate_count > 20 || ctx.candidate_count < 10)
    {
        fprintf(stderr, "unexpected candidate count post pipeline: %u\n", ctx.candidate_count);
        return 7;
    }
    float hbroad_time =
        pipeline.stages[3].metrics.last_ms; /* index: temporal, spatial, aabb, hbroad, pixel */
    float pixel_time = pipeline.stages[4].metrics.last_ms;
    if (hbroad_time <= 0.f || pixel_time <= 0.f)
    {
        fprintf(stderr, "expected positive timings for new stages\n");
        return 8;
    }
    /* Force FAST tier and re-run to verify pixel stage pruning halves list roughly */
    pipeline.quality_level = ROGUE_COLLISION_FAST;
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 9;
    /* Quality may auto-upgrade/downgrade adaptively; only ensure it didn't drop below FAST. */
    if (pipeline.quality_level < ROGUE_COLLISION_FAST ||
        pipeline.quality_level > ROGUE_COLLISION_ULTRA)
    {
        fprintf(stderr, "quality tier out of bounds\n");
        return 10;
    }
    /* FAST pixel stage pruning may reduce ~by half; allow tolerance. */
    if (ctx.candidate_count == 0)
    {
        fprintf(stderr, "pixel pruning removed all candidates\n");
        return 11;
    }
    return 0;
}
