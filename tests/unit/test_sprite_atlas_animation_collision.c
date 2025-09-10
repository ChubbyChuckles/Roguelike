/* test_sprite_atlas_animation_collision.c - Milestone 1.3 initial slice test */
#include "game/hit_pixel_mask.h"
#include "game/sprite_atlas_collision.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int alloc_frame_basic(struct RogueHitPixelMaskFrame* f, int w, int h)
{
    if (!f)
        return 0;
    memset(f, 0, sizeof(*f));
    f->width = w;
    f->height = h;
    f->pitch_words = (w + 31) / 32;
    f->mipmap_count = 1;
    size_t words = (size_t) f->pitch_words * (size_t) h;
    f->bits = (uint32_t*) calloc(words, sizeof(uint32_t));
    return f->bits != NULL;
}

int main(void)
{
    /* Atlas 8x8 - set a bit at (2,3) */
    struct RogueHitPixelMaskFrame atlas;
    if (!alloc_frame_basic(&atlas, 8, 8))
    {
        fprintf(stderr, "alloc atlas fail\n");
        return 1;
    }
    rogue_hit_mask_set(&atlas, 2, 3);

    RogueSpriteAtlasRegion region = {0, 0, 4, 4, 0, 0, 1.0f, 1.0f, 0};
    struct RogueHitPixelMaskFrame sub;
    if (!rogue_sprite_atlas_extract_collision_region(&atlas, &region, &sub))
    {
        fprintf(stderr, "region extract fail\n");
        return 1;
    }
    if (!rogue_hit_mask_test(&sub, 2, 3))
    {
        fprintf(stderr, "expected bit in sub region (2,3)\n");
        return 1;
    }

    /* Build simple animation (sub -> f2 -> atlas) */
    struct RogueHitPixelMaskFrame f2;
    if (!alloc_frame_basic(&f2, 4, 4))
    {
        fprintf(stderr, "alloc f2 fail\n");
        return 1;
    }
    rogue_hit_mask_set(&f2, 1, 1);

    const struct RogueHitPixelMaskFrame* frames[3] = {&sub, &f2, &atlas};
    uint32_t durations[3] = {100, 200, 300}; /* total 600 */
    RogueAnimationCollisionSet set;
    if (!rogue_animation_collision_build(&set, frames, durations, 3, 1.0f))
    {
        fprintf(stderr, "build anim set fail\n");
        return 1;
    }
    if (set.total_duration_ms != 600 || set.frame_timings[3] != 600 ||
        set.frame_timings[1] != 100 || set.frame_timings[2] != 300)
    {
        fprintf(stderr, "timing mismatch\n");
        return 1;
    }

    const struct RogueHitPixelMaskFrame *a, *b;
    float t;
    if (!rogue_animation_collision_sample(&set, 50, &a, &b, &t) || a != frames[0] ||
        b != frames[1] || t < 0.45f || t > 0.55f)
    {
        fprintf(stderr, "sample 50ms invalid t=%f\n", t);
        return 1;
    }
    if (!rogue_animation_collision_sample(&set, 250, &a, &b, &t) || a != frames[1] ||
        b != frames[2] || t < 0.70f || t > 0.80f)
    {
        fprintf(stderr, "sample 250ms mismatch t=%f\n", t);
        return 1;
    }
    if (!rogue_animation_collision_sample(&set, 599, &a, &b, &t) || a != frames[2] ||
        b != frames[0] || t < 0.95f)
    {
        fprintf(stderr, "sample wrap mismatch t=%f\n", t);
        return 1;
    }

    /* Blended sampling mid-way inside first frame span (should union A/B) */
    float bt = 0.f;
    const struct RogueHitPixelMaskFrame* blended =
        rogue_animation_collision_sample_blended(&set, 50, &bt);
    if (!blended)
    {
        fprintf(stderr, "blended frame NULL\n");
        return 1;
    }
    if (bt < 0.45f || bt > 0.55f)
    {
        fprintf(stderr, "unexpected blended local t=%f\n", bt);
        return 1;
    }
    /* Since A had bit (2,3) and B has (1,1), blended should include both. */
    if (!rogue_hit_mask_test(blended, 2, 3) || !rogue_hit_mask_test(blended, 1, 1))
    {
        fprintf(stderr, "blended union missing bits\n");
        return 1;
    }

    /* Endpoint bias: near 0 should return direct A (not allocate union). */
    const struct RogueHitPixelMaskFrame* near_start =
        rogue_animation_collision_sample_blended(&set, 5, &bt);
    if (near_start != frames[0])
    {
        fprintf(stderr, "expected direct A on near-start blend\n");
        return 1;
    }

    /* Ensure release works (no crash). */
    rogue_animation_collision_release_blended(&set);

    rogue_animation_collision_free(&set);
    rogue_sprite_atlas_region_free_frame(&sub);
    free(f2.bits);
    free(atlas.bits);
    return 0;
}
