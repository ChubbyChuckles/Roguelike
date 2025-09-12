/* test_collision_pipeline_simd_microbench.c
 * Micro-benchmark: measure scalar vs SIMD (SSE2/AVX2) time for AABB prefilter
 * and hierarchical broad-phase under deterministic synthetic load.
 * Always returns 0; prints timings to stdout for humans/CI artifacts.
 */
#include "game/collision_pipeline.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Simple high-res timer wrappers using the same abstraction as production code if exposed,
 * else fall back to a portable Windows timing via QueryPerformanceCounter behind the
 * Rogue timers. We reuse the pipeline's timer helpers by measuring around execute(). */

static bool stage_aabb(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_aabb_prefilter(ctx, m);
}
static bool stage_hbroad(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    return rogue_collision_stage_hierarchical_broad(ctx, m);
}

static void fill_candidates(RogueCollisionCandidate* c, uint32_t* out_n, uint32_t count)
{
    uint32_t n = count;
    *out_n = n;
    memset(c, 0, sizeof(RogueCollisionCandidate) * n);
    for (uint32_t i = 0; i < n; ++i)
    {
        c[i].id = i;
        /* Spread positions in and out of view; deterministic */
        c[i].x = (float) ((i * 17) % 512) - 128.f;
        c[i].y = (float) ((i * 31) % 512) - 128.f;
        c[i].half_w = 1.f + (float) ((i % 5) == 0);
        c[i].half_h = 1.f + (float) ((i % 7) == 0);
        c[i].vx = (i & 1) ? 2.0f : -1.5f;
        c[i].vy = (i % 3) ? -0.75f : 1.25f;
    }
}

static double bench_once(int simd_on, uint32_t loops, uint32_t cand_count)
{
    rogue_collision_simd_set_enabled(simd_on);
    RogueCollisionPipeline p;
    rogue_collision_pipeline_init(&p, ROGUE_COLLISION_BALANCED, 0.f, false);
    rogue_collision_pipeline_add_stage(&p, "aabb", stage_aabb, 0.f, 0);
    rogue_collision_pipeline_add_stage(&p, "hbroad", stage_hbroad, 0.f, 0);

    RogueCollisionCandidate c[4096];
    uint32_t n = 0;
    fill_candidates(c, &n, cand_count);

    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = c;
    ctx.candidate_count = n;
    ctx.view_x = 0;
    ctx.view_y = 0;
    ctx.view_w = 256;
    ctx.view_h = 256;

    /* Warm-up */
    (void) rogue_collision_pipeline_execute(&p, &ctx, NULL);

    /* Reset deterministic inputs for the measured loops */
    double total_ms = 0.0;
    for (uint32_t i = 0; i < loops; ++i)
    {
        fill_candidates(c, &n, cand_count);
        ctx.candidate_count = n;
        clock_t t0 = clock();
        (void) rogue_collision_pipeline_execute(&p, &ctx, NULL);
        clock_t t1 = clock();
        total_ms += (double) (t1 - t0) * 1000.0 / (double) CLOCKS_PER_SEC;
    }
    return total_ms;
}

int main(void)
{
    const uint32_t cand_count = 512; /* synthetic load size */
    const uint32_t loops = 200;      /* iterations */

    double t_scalar_ms = bench_once(0, loops, cand_count);
    double t_simd_ms = bench_once(1, loops, cand_count);

    double per_loop_scalar = t_scalar_ms / (double) loops;
    double per_loop_simd = t_simd_ms / (double) loops;
    double speedup = (per_loop_simd > 0.0) ? (per_loop_scalar / per_loop_simd) : 0.0;

    printf("[SIMD microbench] candidates=%u loops=%u\n", cand_count, loops);
    printf("  scalar: total=%.3f ms  avg=%.4f ms/loop\n", t_scalar_ms, per_loop_scalar);
    printf("  simd  : total=%.3f ms  avg=%.4f ms/loop\n", t_simd_ms, per_loop_simd);
    printf("  speedup (scalar/simd): %.3fx\n", speedup);

    /* Determinism/parity: also verify counts/order match once per mode toggle */
    RogueCollisionCandidate s0[2048], s1[2048];
    uint32_t n0 = 0, n1 = 0;
    {
        /* One shot with SIMD off */
        rogue_collision_simd_set_enabled(0);
        RogueCollisionPipeline p;
        rogue_collision_pipeline_init(&p, ROGUE_COLLISION_BALANCED, 0.f, false);
        rogue_collision_pipeline_add_stage(&p, "aabb", stage_aabb, 0.f, 0);
        rogue_collision_pipeline_add_stage(&p, "hbroad", stage_hbroad, 0.f, 0);
        RogueCollisionCandidate c[4096];
        uint32_t n = 0;
        fill_candidates(c, &n, cand_count);
        RogueCollisionContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.candidates = c;
        ctx.candidate_count = n;
        ctx.view_x = 0;
        ctx.view_y = 0;
        ctx.view_w = 256;
        ctx.view_h = 256;
        (void) rogue_collision_pipeline_execute(&p, &ctx, NULL);
        n0 = ctx.candidate_count;
        for (uint32_t i = 0; i < n0; ++i)
            s0[i] = ctx.candidates[i];
    }
    {
        /* One shot with SIMD on */
        rogue_collision_simd_set_enabled(1);
        RogueCollisionPipeline p;
        rogue_collision_pipeline_init(&p, ROGUE_COLLISION_BALANCED, 0.f, false);
        rogue_collision_pipeline_add_stage(&p, "aabb", stage_aabb, 0.f, 0);
        rogue_collision_pipeline_add_stage(&p, "hbroad", stage_hbroad, 0.f, 0);
        RogueCollisionCandidate c[4096];
        uint32_t n = 0;
        fill_candidates(c, &n, cand_count);
        RogueCollisionContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.candidates = c;
        ctx.candidate_count = n;
        ctx.view_x = 0;
        ctx.view_y = 0;
        ctx.view_w = 256;
        ctx.view_h = 256;
        (void) rogue_collision_pipeline_execute(&p, &ctx, NULL);
        n1 = ctx.candidate_count;
        for (uint32_t i = 0; i < n1; ++i)
            s1[i] = ctx.candidates[i];
    }
    if (n0 != n1)
        printf("[SIMD microbench] WARNING: count mismatch scalar=%u simd=%u\n", n0, n1);
    else
    {
        for (uint32_t i = 0; i < n0; ++i)
        {
            if (s0[i].id != s1[i].id)
            {
                printf("[SIMD microbench] WARNING: order mismatch at %u: scalar id=%u simd id=%u\n",
                       i, s0[i].id, s1[i].id);
                break;
            }
        }
    }
    return 0;
}
