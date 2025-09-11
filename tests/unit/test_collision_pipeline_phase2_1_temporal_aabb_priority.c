/* test_collision_pipeline_phase2_1_temporal_aabb_priority.c
 * Validates temporal AABB expansion prevents tunneling and that priority ordering
 * sorts on-screen nearer candidates first.
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
static bool stage_hbroad(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_hierarchical_broad(ctx, m);
}

int main(void)
{
    RogueCollisionPipeline pipeline;
    rogue_collision_pipeline_init(&pipeline, ROGUE_COLLISION_BALANCED, 2.0f, false);
    if (!rogue_collision_pipeline_add_stage(&pipeline, "spatial", stage_spatial, 0.f, 0))
        return 1;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "aabb", stage_aabb, 0.f, 0))
        return 2;
    if (!rogue_collision_pipeline_add_stage(&pipeline, "hbroad", stage_hbroad, 0.f, 0))
        return 3;

    RogueCollisionCandidate c[4];
    memset(c, 0, sizeof(c));
    /* Candidate 0: fast mover starting left of view, moving right into view within horizon */
    c[0].id = 0;
    c[0].x = -50.f;
    c[0].y = 50.f;
    c[0].half_w = c[0].half_h = 2.f;
    c[0].vx = 10.f;
    c[0].vy = 0.f;
    /* Candidate 1: stationary inside view (near center) */
    c[1].id = 1;
    c[1].x = 10.f;
    c[1].y = 10.f;
    c[1].half_w = c[1].half_h = 1.f;
    /* Candidate 2: far outside and static */
    c[2].id = 2;
    c[2].x = 1000.f;
    c[2].y = 1000.f;
    c[2].half_w = c[2].half_h = 2.f;
    /* Candidate 3: inside view but further from center than id 1 */
    c[3].id = 3;
    c[3].x = 90.f;
    c[3].y = 90.f;
    c[3].half_w = c[3].half_h = 1.f;

    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = c;
    ctx.candidate_count = 4;
    ctx.view_x = 0.f;
    ctx.view_y = 0.f;
    ctx.view_w = 100.f;
    ctx.view_h = 100.f;

    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 4;

    /* Temporal AABB expansion in hbroad should keep candidate 0 as it sweeps into view */
    int found_fast = 0;
    for (uint32_t i = 0; i < ctx.candidate_count; ++i)
        if (ctx.candidates[i].id == 0)
            found_fast = 1;
    if (!found_fast)
    {
        fprintf(stderr, "temporal AABB failed to include fast mover\n");
        return 5;
    }

    /* Priority ordering in aabb stage should place id 1 (near center) before id 3 */
    int idx1 = -1, idx3 = -1;
    for (uint32_t i = 0; i < ctx.candidate_count; ++i)
    {
        if (ctx.candidates[i].id == 1)
            idx1 = (int) i;
        if (ctx.candidates[i].id == 3)
            idx3 = (int) i;
    }
    if (idx1 < 0 || idx3 < 0)
    {
        fprintf(stderr, "expected on-screen candidates not present after broad phases\n");
        return 6;
    }
    if (!(idx1 < idx3))
    {
        fprintf(stderr, "priority ordering did not bring nearer candidate earlier (%d vs %d)\n",
                idx1, idx3);
        return 7;
    }
    return 0;
}
