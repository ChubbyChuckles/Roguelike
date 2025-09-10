/* test_animation_collision_sync_morph.c - Mask morphing (union blend) baseline test */
#include "game/animation_collision_sync.h"
#include "game/hit_pixel_mask.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* m)
{
    fprintf(stderr, "%s\n", m);
    return 1;
}

/* Allocate a tiny mask frame (w,h) and set a single bit at (sx,sy). */
static struct RogueHitPixelMaskFrame* make_mask(int w, int h, int sx, int sy)
{
    struct RogueHitPixelMaskFrame* f = (struct RogueHitPixelMaskFrame*) calloc(1, sizeof(*f));
    if (!f)
        return NULL;
    int pitch_words = (w + 31) / 32;
    f->width = w;
    f->height = h;
    f->pitch_words = pitch_words;
    f->bits = (uint32_t*) calloc((size_t) pitch_words * (size_t) h, sizeof(uint32_t));
    if (!f->bits)
    {
        free(f);
        return NULL;
    }
    if (sx >= 0 && sy >= 0)
        rogue_hit_mask_set(f, sx, sy);
    return f;
}

int main(void)
{
    /* Build two keyframe masks with distinct single bits */
    struct RogueHitPixelMaskFrame* k0 = make_mask(8, 8, 1, 1);
    struct RogueHitPixelMaskFrame* k1 = make_mask(8, 8, 6, 6);
    if (!k0 || !k1)
        return fail("alloc keyframes");
    const struct RogueHitPixelMaskFrame* keyframes[2] = {k0, k1};
    float ts[2] = {0.f, 100.f};
    RogueAnimationCollisionSync sync = {0};
    sync.keyframe_timestamps = ts;
    sync.keyframe_count = 2;
    sync.keyframe_masks = (struct RogueHitPixelMaskFrame**) keyframes;
    sync.smooth_interpolation = 1;
    sync.interpolation_quality = 0.f;
    sync.playback_speed = 1.f;

    /* Midpoint (t=0.5) => union should contain both bits */
    const struct RogueHitPixelMaskFrame* mid = rogue_animation_collision_morph_mask(&sync, 50.f);
    if (!mid)
        return fail("mid morph null");
    if (!rogue_hit_mask_test(mid, 1, 1) || !rogue_hit_mask_test(mid, 6, 6))
        return fail("union missing bits at midpoint");

    /* Near start (t small) should return A directly (optimization) */
    const struct RogueHitPixelMaskFrame* near_start =
        rogue_animation_collision_morph_mask(&sync, 5.f);
    if (near_start != k0)
        return fail("near start should return k0");

    /* Near end should return B directly */
    const struct RogueHitPixelMaskFrame* near_end =
        rogue_animation_collision_morph_mask(&sync, 95.f);
    if (near_end != k1)
        return fail("near end should return k1");

    /* Disable interpolation -> always base (first frame for times before last) */
    sync.smooth_interpolation = 0;
    const struct RogueHitPixelMaskFrame* no_interp =
        rogue_animation_collision_morph_mask(&sync, 50.f);
    if (no_interp != k0)
        return fail("disabled interpolation should return base frame");

    /* Cleanup */
    rogue_animation_collision_sync_release(&sync);
    free(k0->bits);
    free(k0);
    free(k1->bits);
    free(k1);
    return 0;
}
