/* test_sprite_atlas_animation_collision_fuzz.c
 * Deterministic pseudo-fuzz test stressing blended capacity growth & reuse.
 * We build animations of varying frame counts (1..16) with alternating
 * frame dimensions to force blended allocation resizing while ensuring
 * non-flaky behavior via fixed PRNG seed.
 */
#include "game/hit_pixel_mask.h"
#include "game/sprite_atlas_collision.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rng_state = 0xC0FFEEu;
static uint32_t rng_next(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}
static int alloc_frame_basic(struct RogueHitPixelMaskFrame* f, int w, int h)
{
    if (!f)
        return 0;
    memset(f, 0, sizeof(*f));
    f->width = w;
    f->height = h;
    f->pitch_words = (w + 31) / 32;
    f->mipmap_count = 1;
    size_t words = (size_t) f->pitch_words * h;
    f->bits = (uint32_t*) calloc(words, sizeof(uint32_t));
    return f->bits != NULL;
}

int main(void)
{
    for (int variant = 0; variant < 50; ++variant)
    {
        int frame_count = (int) (rng_next() % 16) + 1; /* 1..16 (exercise inline & heap) */
        struct RogueHitPixelMaskFrame* frames =
            (struct RogueHitPixelMaskFrame*) calloc((size_t) frame_count, sizeof(*frames));
        if (!frames)
            return 1;
        const struct RogueHitPixelMaskFrame** frame_ptrs =
            (const struct RogueHitPixelMaskFrame**) calloc((size_t) frame_count,
                                                           sizeof(*frame_ptrs));
        uint32_t* durations = (uint32_t*) calloc((size_t) frame_count, sizeof(*durations));
        if (!frame_ptrs || !durations)
            return 2;
        /* Create frames with alternating sizes: small (5x5) and larger (9x7) to force mismatch
           so blended path will early-return (assert fires only in debug if mismatch encountered).
           After first mismatch cycle, unify sizes back to consistent dims to trigger blend union.
         */
        int toggle_mismatch =
            (variant % 3) == 0; /* every third variant introduces mismatch early */
        int uniform_w = (variant & 1) ? 8 : 10;
        int uniform_h = (variant & 1) ? 6 : 12;
        for (int i = 0; i < frame_count; i++)
        {
            int w, h;
            if (toggle_mismatch && i < frame_count / 2)
            {
                w = (i & 1) ? 5 : 9;
                h = (i & 1) ? 5 : 7;
            }
            else
            {
                w = uniform_w;
                h = uniform_h;
            }
            if (!alloc_frame_basic(&frames[i], w, h))
            {
                fprintf(stderr, "alloc fail frame %d\n", i);
                return 3;
            }
            /* Sprinkle bits: set one deterministic coordinate if in bounds */
            int bx = (int) (rng_next() % w);
            int by = (int) (rng_next() % h);
            rogue_hit_mask_set(&frames[i], bx, by);
            frame_ptrs[i] = &frames[i];
            durations[i] = (uint32_t) ((rng_next() % 40) + 10); /* 10..49 */
        }
        RogueAnimationCollisionSet set;
        if (!rogue_animation_collision_build(&set, frame_ptrs, durations, (uint8_t) frame_count,
                                             1.0f))
        {
            fprintf(stderr, "build fail variant %d\n", variant);
            return 4;
        }
        /* Sample a spread of times across two total loops to exercise wrap & blended reuse.
           We only invoke the blended sampler when the two sampled frames have matching dimensions
           (the implementation asserts in debug on mismatches to surface future resample work).
           This per-sample gating is more robust than a coarse time window because randomized
           durations can push mismatched pairs later in the timeline. */
        uint32_t total = set.total_duration_ms;
        if (total == 0)
        {
            fprintf(stderr, "zero total duration\n");
            return 5;
        }
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int step = 0; step < 13; ++step)
            {
                uint32_t t =
                    (uint32_t) ((((uint64_t) step * (uint64_t) total) / 13) + (pass ? total : 0));
                /* First get the base sample (A/B + interpolation). */
                const struct RogueHitPixelMaskFrame *a, *b;
                float interp = 0.f;
                if (!rogue_animation_collision_sample(&set, t, &a, &b, &interp))
                {
                    fprintf(stderr, "sample fail variant %d step %d\n", variant, step);
                    return 6;
                }
                if (interp < -0.01f || interp > 1.01f)
                {
                    fprintf(stderr, "interp out of range %f\n", interp);
                    return 7;
                }
                int dims_match = (a->width == b->width && a->height == b->height);
                if (frame_count > 1 && dims_match)
                {
                    float blended_local = 0.f;
                    const struct RogueHitPixelMaskFrame* blended =
                        rogue_animation_collision_sample_blended(&set, t, &blended_local);
                    if (!blended)
                    {
                        fprintf(stderr, "NULL blended variant %d step %d (dims match)\n", variant,
                                step);
                        return 8;
                    }
                    if (blended_local < -0.01f || blended_local > 1.01f)
                    {
                        fprintf(stderr, "blended_local out of range %f\n", blended_local);
                        return 9;
                    }
                }
            }
        }
        /* Track that blended frame capacity matches last consistent size if any frame_count>1. */
        if (frame_count > 1 && set.blended_frame)
        {
            if (set.blended_w != uniform_w || set.blended_h != uniform_h)
            {
                fprintf(stderr, "capacity mismatch expected %dx%d got %dx%d variant %d\n",
                        uniform_w, uniform_h, set.blended_w, set.blended_h, variant);
                return 10;
            }
        }
        rogue_animation_collision_release_blended(&set);
        rogue_animation_collision_free(&set);
        for (int i = 0; i < frame_count; i++)
            free(frames[i].bits);
        free(frames);
        free(frame_ptrs);
        free(durations);
    }
    return 0;
}
