/* test_collision_pixel_lod.c
 * Verifies pixel stage selects mip level and preserves candidates when higher mips retain
 * occupancy while base center is empty. Uses a synthetic mask with a small solid region
 * at lower resolution.
 */
#include "game/collision_pipeline.h"
#include "game/hit_pixel_mask.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void build_mask_with_mips(RogueHitPixelMaskFrame* f)
{
    memset(f, 0, sizeof(*f));
    /* Base 32x32, empty center; but mip1 16x16 has bit set at its center */
    f->width = 32;
    f->height = 32;
    f->pitch_words = (f->width + 31) / 32;
    size_t words = (size_t) f->pitch_words * (size_t) f->height;
    f->bits = (uint32_t*) calloc(words, sizeof(uint32_t)); /* leave base all zero */
    f->mipmap_count = 2;
    f->mipmaps = (RogueHitPixelMaskMipmapLevel*) calloc(1, sizeof(RogueHitPixelMaskMipmapLevel));
    f->mipmaps[0].width = 16;
    f->mipmaps[0].height = 16;
    f->mipmaps[0].pitch_words = 1;
    f->mipmaps[0].bits = (uint32_t*) calloc(
        (size_t) f->mipmaps[0].pitch_words * f->mipmaps[0].height, sizeof(uint32_t));
    /* set center bit at mip1 */
    int cx = f->mipmaps[0].width / 2;
    int cy = f->mipmaps[0].height / 2;
    int idx = cy * f->mipmaps[0].pitch_words + (cx >> 5);
    f->mipmaps[0].bits[idx] |= (1u << (cx & 31));
}

int main(void)
{
    RogueHitPixelMaskFrame frame;
    build_mask_with_mips(&frame);

    RogueCollisionCandidate cand;
    memset(&cand, 0, sizeof(cand));
    cand.id = 1;
    cand.x = 300.0f; /* far from view center so pixel stage should pick mip>=1 */
    cand.y = 300.0f;
    cand.half_w = 8;
    cand.half_h = 8;
    cand.pixel_mask = &frame;

    RogueCollisionCandidate list[1];
    list[0] = cand;

    RogueCollisionPipeline pipeline;
    rogue_collision_pipeline_init(&pipeline, ROGUE_COLLISION_PRECISE, 2.0f, false);
    /* Only include pixel stage to exercise refinement directly */
    if (!rogue_collision_pipeline_add_stage(&pipeline, "pixel", rogue_collision_stage_pixel_perfect,
                                            0.f, 0))
        return 1;

    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = list;
    ctx.candidate_count = 1;
    ctx.view_x = 0;
    ctx.view_y = 0;
    ctx.view_w = 100;
    ctx.view_h = 100;

    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 2;
    /* Expect the candidate kept by sampling mip1 center, despite base center being empty */
    if (ctx.candidate_count != 1)
        return 3;

    /* Now make it near (forcing level 0 center) and expect prune */
    list[0] = cand;
    ctx.candidate_count = 1;
    list[0].x = 10.0f;
    list[0].y = 10.0f; /* near view center */
    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 4;
    if (ctx.candidate_count != 0)
        return 5;

    /* cleanup */
    free(frame.bits);
    free(frame.mipmaps[0].bits);
    free(frame.mipmaps);
    return 0;
}
