/* test_animation_collision_sync_quality.c - Verify interpolation quality modes:
 * - cubic smoothstep when quality >= 0.5
 * - quintic smootherstep when quality >= 0.9
 */
#include "game/animation_collision_sync.h"
#include <stdio.h>

static int fail(const char* m)
{
    fprintf(stderr, "%s\n", m);
    return 1;
}

int main(void)
{
    RogueAnimationCollisionSync sync = {0};
    float kts[3] = {0.f, 100.f, 200.f};
    sync.keyframe_timestamps = kts;
    sync.keyframe_count = 3;
    sync.smooth_interpolation = 1;
    sync.playback_speed = 1.f;

    const struct RogueHitPixelMaskFrame *a = NULL, *b = NULL;
    float t = 0.f;

    /* At 25ms between 0..100, raw t=0.25. With quality >=0.5 → smoothstep(t)=0.15625 */
    sync.interpolation_quality = 0.6f;
    if (!rogue_animation_collision_interpolate_masks(&sync, 25.f, &a, &b, &t))
        return fail("interp (smoothstep) failed");
    if (t < 0.15f || t > 0.165f)
        return fail("smoothstep t mismatch at 0.25 (expected ~0.15625)");

    /* With quality >=0.9 → smootherstep(t)=6t^5-15t^4+10t^3 ≈ 0.1035 at t=0.25 */
    sync.interpolation_quality = 0.95f;
    a = b = NULL;
    t = 0.f;
    if (!rogue_animation_collision_interpolate_masks(&sync, 25.f, &a, &b, &t))
        return fail("interp (smootherstep) failed");
    if (t < 0.10f || t > 0.11f)
        return fail("smootherstep t mismatch at 0.25 (expected ~0.1035)");

    /* Symmetry check at 75ms (t_raw=0.75): smootherstep ≈ 0.8965 */
    a = b = NULL;
    t = 0.f;
    if (!rogue_animation_collision_interpolate_masks(&sync, 75.f, &a, &b, &t))
        return fail("interp (smootherstep @0.75) failed");
    if (t < 0.89f || t > 0.905f)
        return fail("smootherstep t mismatch at 0.75 (expected ~0.8965)");

    return 0;
}
