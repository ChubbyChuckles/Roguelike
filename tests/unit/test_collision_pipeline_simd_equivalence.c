/* test_collision_pipeline_simd_equivalence.c
 * Asserts that enabling/disabling SIMD yields identical candidate ordering and counts
 * for AABB prefilter and hierarchical broad-phase.
 */
#include "game/collision_pipeline.h"
#include <stdio.h>
#include <string.h>

static bool stage_aabb(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_aabb_prefilter(ctx, m);
}
static bool stage_hbroad(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_hierarchical_broad(ctx, m);
}

static void fill_candidates(RogueCollisionCandidate* c, uint32_t* out_n)
{
    /* Deterministic set including on/off-screen and moving targets */
    uint32_t n = 16;
    *out_n = n;
    memset(c, 0, sizeof(RogueCollisionCandidate) * n);
    for (uint32_t i = 0; i < n; ++i)
    {
        c[i].id = i;
        c[i].x = (float) ((i * 13) % 120) - 10.f; /* some outside */
        c[i].y = (float) ((i * 7) % 120) - 10.f;
        c[i].half_w = 1.f + (float) ((i % 3) == 0);
        c[i].half_h = 1.f + (float) ((i % 5) == 0);
        c[i].vx = (i % 2) ? 3.f : -2.f;
        c[i].vy = (i % 3) ? -1.f : 2.f;
    }
}

static int run_once(int simd_on, RogueCollisionCandidate* out, uint32_t* out_count)
{
    rogue_collision_simd_set_enabled(simd_on);
    RogueCollisionPipeline p;
    rogue_collision_pipeline_init(&p, ROGUE_COLLISION_BALANCED, 0.f, false);
    if (!rogue_collision_pipeline_add_stage(&p, "aabb", stage_aabb, 0.f, 0))
        return 10;
    if (!rogue_collision_pipeline_add_stage(&p, "hbroad", stage_hbroad, 0.f, 0))
        return 11;
    RogueCollisionCandidate c[32];
    uint32_t n = 0;
    fill_candidates(c, &n);
    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = c;
    ctx.candidate_count = n;
    ctx.view_x = 0;
    ctx.view_y = 0;
    ctx.view_w = 100;
    ctx.view_h = 100;
    if (!rogue_collision_pipeline_execute(&p, &ctx, NULL))
        return 12;
    *out_count = ctx.candidate_count;
    for (uint32_t i = 0; i < ctx.candidate_count; ++i)
        out[i] = ctx.candidates[i];
    return 0;
}

int main(void)
{
    RogueCollisionCandidate s0[32], s1[32];
    uint32_t n0 = 0, n1 = 0;
    int rc = run_once(0, s0, &n0);
    if (rc)
        return rc;
    rc = run_once(1, s1, &n1);
    if (rc)
        return rc;
    if (n0 != n1)
    {
        fprintf(stderr, "count mismatch scalar=%u simd=%u\n", n0, n1);
        return 1;
    }
    for (uint32_t i = 0; i < n0; ++i)
    {
        if (s0[i].id != s1[i].id)
        {
            fprintf(stderr, "order mismatch at %u: scalar id=%u simd id=%u\n", i, s0[i].id,
                    s1[i].id);
            return 2;
        }
    }
    return 0;
}
