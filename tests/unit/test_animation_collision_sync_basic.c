/* test_animation_collision_sync_basic.c - Milestone 3.2 scaffold test
 * Validates timeline window activation and keyframe interpolation scaffolding.
 */
#include "game/animation_collision_sync.h"
#include <stdio.h>
#include <stdlib.h>

static int float_near(float a, float b, float eps) { return (a - b < eps && b - a < eps); }

int main(void)
{
    /* Build a timeline with three windows */
    RogueCollisionTimeline tl;
    rogue_collision_timeline_init(&tl);
    RogueCollisionTimelineWindow w0 = {.timestamp_ms = 0.f,
                                       .duration_ms = 40.f,
                                       .collision_mask_index = 0,
                                       .intensity_multiplier = 1.f};
    RogueCollisionTimelineWindow w1 = {.timestamp_ms = 50.f,
                                       .duration_ms = 30.f,
                                       .collision_mask_index = 1,
                                       .intensity_multiplier = 1.2f};
    RogueCollisionTimelineWindow w2 = {.timestamp_ms = 90.f,
                                       .duration_ms = 20.f,
                                       .collision_mask_index = 2,
                                       .intensity_multiplier = 0.8f};
    rogue_collision_timeline_add(&tl, &w0);
    rogue_collision_timeline_add(&tl, &w1);
    rogue_collision_timeline_add(&tl, &w2);

    /* Evaluate at various times */
    uint8_t idx[4];
    if (rogue_animation_collision_evaluate_timeline(&tl, 10.f, idx, 4) != 1 || idx[0] != 0)
    {
        fprintf(stderr, "expected only window 0 active at 10ms\n");
        return 1;
    }
    if (rogue_animation_collision_evaluate_timeline(&tl, 55.f, idx, 4) != 1 || idx[0] != 1)
    {
        fprintf(stderr, "expected only window 1 active at 55ms\n");
        return 2;
    }
    /* Overlap check: create partial overlap (adjust w2 to start before w1 ends) */
    tl.windows[2].timestamp_ms = 70.f;
    tl.windows[2].duration_ms = 40.f; /* 70..110 */
    if (rogue_animation_collision_evaluate_timeline(&tl, 95.f, idx, 4) != 1 || idx[0] != 2)
    {
        fprintf(stderr, "expected only window 2 active at 95ms\n");
        return 3;
    }

    /* Keyframe interpolation test */
    RogueAnimationCollisionSync sync = {0};
    float keyframes[3] = {0.f, 50.f, 100.f};
    sync.keyframe_timestamps = keyframes;
    sync.keyframe_count = 3;
    sync.smooth_interpolation = true;
    sync.interpolation_quality = 0.f;
    /* No masks allocated (NULL) – interpolation still yields factor */
    const struct RogueHitPixelMaskFrame* a = NULL;
    const struct RogueHitPixelMaskFrame* b = NULL;
    float t = -1.f;
    if (!rogue_animation_collision_interpolate_masks(&sync, 25.f, &a, &b, &t) ||
        !float_near(t, 0.5f, 0.0001f))
    {
        fprintf(stderr, "expected t=0.5 at 25ms got %f\n", t);
        return 4;
    }
    /* Clamp beyond last keyframe */
    a = b = NULL;
    t = -1.f;
    if (!rogue_animation_collision_interpolate_masks(&sync, 150.f, &a, &b, &t) || t != 0.f ||
        b != NULL)
    {
        fprintf(stderr, "expected clamp at end (t=0,b=NULL) got t=%f\n", t);
        return 5;
    }
    /* Disable interpolation */
    sync.smooth_interpolation = false;
    if (!rogue_animation_collision_interpolate_masks(&sync, 25.f, &a, &b, &t) || t != 0.f ||
        b != NULL)
    {
        fprintf(stderr, "expected no interpolation when disabled\n");
        return 6;
    }

    return 0; /* success */
}
