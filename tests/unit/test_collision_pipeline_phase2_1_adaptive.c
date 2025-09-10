/* test_collision_pipeline_phase2_1_adaptive.c
 * Validates extended Milestone 2.1 slice additions:
 *  - Spatial culling (quadtree) reduces candidate set to view rectangle
 *  - Predictive culling keeps candidates that will enter view next frame
 *  - AABB prefilter distance-based LOD triggers quality delta
 *  - Adaptive quality adjusts pipeline quality based on frame budget
 */
#include "game/collision_pipeline.h"
#include <stdio.h>
#include <string.h>

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
    rogue_collision_pipeline_init(&pipeline, ROGUE_COLLISION_PRECISE, 0.5f, true);
    /* Register new built-in stages */
    if (!rogue_collision_pipeline_add_stage(&pipeline, "spatial", stage_spatial, 0.f, 0))
        return 1;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "aabb", stage_aabb, 0.f, 0))
        return 2;

    RogueCollisionCandidate cands[16];
    /* Place 6 inside view (0..5), 4 outside but moving in (vx toward view), rest far static */
    for (int i = 0; i < 16; ++i)
    {
        cands[i].id = (uint32_t) i;
        cands[i].half_w = cands[i].half_h = 1.f;
        cands[i].vx = cands[i].vy = 0.f;
    }
    /* view at (0,0) size 100x100 */
    for (int i = 0; i < 6; ++i)
    {
        cands[i].x = (float) (10 * i);
        cands[i].y = (float) (5 * i);
    }
    for (int i = 6; i < 10; ++i)
    {
        cands[i].x = 120.f + (float) (i * 5);
        cands[i].y = 10.f;
        cands[i].vx = -10.f / 16.f;
    }
    for (int i = 10; i < 16; ++i)
    {
        cands[i].x = 400.f + (float) i * 10.f;
        cands[i].y = 400.f;
    }

    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = cands;
    ctx.candidate_count = 16;
    ctx.view_x = 0.f;
    ctx.view_y = 0.f;
    ctx.view_w = 100.f;
    ctx.view_h = 100.f;

    float costs[2] = {0.3f, 0.3f}; /* exceed budget 0.5 -> downgrade */
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, costs))
        return 3;
    if (ctx.candidate_count < 6)
    {
        fprintf(stderr, "spatial cull lost visible candidates\n");
        return 4;
    }
    if (ctx.candidate_count > 10)
    {
        fprintf(stderr, "spatial cull kept too many\n");
        return 5;
    }
    if (pipeline.quality_level != ROGUE_COLLISION_BALANCED &&
        pipeline.quality_level != ROGUE_COLLISION_FAST)
    {
        fprintf(stderr, "adaptive quality failed to downgrade\n");
        return 6;
    }

    /* Run again with tiny costs to allow potential upgrade */
    float low_costs[2] = {0.01f, 0.01f};
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, low_costs))
        return 7;
    if (pipeline.quality_level == ROGUE_COLLISION_FAST)
    {
        fprintf(stderr, "quality did not recover upward\n");
        return 8;
    }
    return 0;
}
