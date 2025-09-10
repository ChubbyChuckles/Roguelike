/* test_collision_pipeline_phase2_1_pixel_sampling.c
 * Validates new pixel-perfect stage mask sampling & pruning semantics:
 *  - FAST tier: legacy half-prune heuristic (no mask-based pruning)
 *  - BALANCED tier: single center sample keeps candidate only if center bit set (empty mask pruned)
 *  - PRECISE / ULTRA tiers: 3x3 center sampling grid (still keeps candidate with center bit)
 *  - Candidates with NULL pixel_mask are always retained (cannot refine)
 *  - Degenerate / empty masks (no sampled solid bit) are pruned at BALANCED+
 */
#include "game/collision_pipeline.h"
#include "game/hit_pixel_mask.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void init_mask(RogueHitPixelMaskFrame* f, int set_center_bit)
{
    memset(f, 0, sizeof(*f));
    f->width = 8;
    f->height = 8;
    f->origin_x = 0;
    f->origin_y = 0;
    f->pitch_words = 1; /* (8+31)/32 */
    size_t words = (size_t) f->pitch_words * f->height;
    f->bits = (uint32_t*) calloc(words, sizeof(uint32_t));
    if (set_center_bit)
        rogue_hit_mask_set(f, f->width / 2, f->height / 2);
}

static int verify_ids(const RogueCollisionContext* ctx, const uint32_t* expected,
                      int expected_count)
{
    if ((int) ctx->candidate_count != expected_count)
    {
        fprintf(stderr, "candidate_count mismatch: got %u expected %d\n", ctx->candidate_count,
                expected_count);
        return 0;
    }
    for (int i = 0; i < expected_count; ++i)
    {
        if (ctx->candidates[i].id != expected[i])
        {
            fprintf(stderr, "candidate[%d] id=%u expected=%u\n", i, ctx->candidates[i].id,
                    expected[i]);
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    /* Prepare two masks: solid (center bit set) and empty */
    RogueHitPixelMaskFrame solid_mask, empty_mask;
    init_mask(&solid_mask, 1);
    init_mask(&empty_mask, 0);

    RogueCollisionPipeline pipeline;
    rogue_collision_pipeline_init(&pipeline, ROGUE_COLLISION_BALANCED, 2.0f, false);

    /* Only pixel stage needed for this focused test */
    if (!rogue_collision_pipeline_add_stage(&pipeline, "pixel", rogue_collision_stage_pixel_perfect,
                                            0.f, 0))
        return 1;

    /* Base candidate templates */
    RogueCollisionCandidate base[3];
    memset(base, 0, sizeof(base));
    base[0].id = 1; /* solid */
    base[0].pixel_mask = &solid_mask;
    base[1].id = 2; /* empty */
    base[1].pixel_mask = &empty_mask;
    base[2].id = 3; /* no mask */
    base[2].pixel_mask = NULL;

    RogueCollisionCandidate work[3];
    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = work;

    /* Scenario 1: BALANCED quality (center sample) */
    memcpy(work, base, sizeof(base));
    ctx.candidate_count = 3;
    pipeline.quality_level = ROGUE_COLLISION_BALANCED;
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 2;
    {
        uint32_t expected[] = {1, 3};
        if (!verify_ids(&ctx, expected, 2))
            return 3;
    }

    /* Scenario 2: PRECISE quality (3x3 sampling) */
    memcpy(work, base, sizeof(base));
    ctx.candidate_count = 3;
    pipeline.quality_level = ROGUE_COLLISION_PRECISE;
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 4;
    {
        uint32_t expected[] = {1, 3};
        if (!verify_ids(&ctx, expected, 2))
            return 5;
    }

    /* Scenario 3: ULTRA quality (same sampling path as PRECISE for now) */
    memcpy(work, base, sizeof(base));
    ctx.candidate_count = 3;
    pipeline.quality_level = ROGUE_COLLISION_ULTRA;
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 6;
    {
        uint32_t expected[] = {1, 3};
        if (!verify_ids(&ctx, expected, 2))
            return 7;
    }

    /* Scenario 4: FAST quality (half-prune heuristic, ignores mask sampling). Expect IDs 1 & 3
     * (indices 0 and 2 kept). */
    memcpy(work, base, sizeof(base));
    ctx.candidate_count = 3;
    pipeline.quality_level = ROGUE_COLLISION_FAST;
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 8;
    {
        uint32_t expected[] = {1, 3};
        if (!verify_ids(&ctx, expected, 2))
            return 9;
    }

    /* Cleanup */
    free(solid_mask.bits);
    free(empty_mask.bits);
    return 0;
}
