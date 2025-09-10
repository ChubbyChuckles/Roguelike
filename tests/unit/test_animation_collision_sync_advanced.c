/* test_animation_collision_sync_advanced.c - Advanced evaluation: speed scaling, frame skip cache,
 * smooth interpolation */
#include "game/animation_collision_sync.h"
#include <stdio.h>
#include <string.h>

static int fail(const char* m)
{
    fprintf(stderr, "%s\n", m);
    return 1;
}

int main(void)
{
    RogueCollisionTimeline tl;
    rogue_collision_timeline_init(&tl);
    tl.loop_animation = 1;
    tl.total_cycle_time_ms = 200.f;
    RogueCollisionTimelineWindow w0 = {.timestamp_ms = 20.f,
                                       .duration_ms = 40.f,
                                       .collision_mask_index = 0,
                                       .intensity_multiplier = 1.f}; /*20..60*/
    RogueCollisionTimelineWindow w1 = {.timestamp_ms = 120.f,
                                       .duration_ms = 30.f,
                                       .collision_mask_index = 1,
                                       .intensity_multiplier = 1.f}; /*120..150*/
    rogue_collision_timeline_add(&tl, &w0);
    rogue_collision_timeline_add(&tl, &w1);

    RogueAnimationCollisionSync sync = {0};
    float kts[3] = {0.f, 100.f, 200.f};
    sync.keyframe_timestamps = kts;
    sync.keyframe_count = 3;
    sync.smooth_interpolation = 1;
    sync.playback_speed = 2.f;
    sync.interpolation_quality = 0.6f;
    sync.frame_skip_threshold = 4; /* ms */

    /* Scaled evaluation: at real time 15ms with speed=2 => scaled 30ms (inside w0) */
    uint8_t idx[4];
    if (rogue_animation_collision_evaluate_timeline_scaled(&sync, &tl, 15.f, idx, 4) != 1 ||
        idx[0] != 0)
        return fail("scaled eval expected window 0 at 15ms (30 scaled)");

    /* Cached evaluation: advance tiny delta below threshold without boundary crossing -> reuse */
    RogueAnimationCollisionEvalState st = {0};
    uint8_t out_a[4];
    uint8_t c1 =
        rogue_animation_collision_evaluate_timeline_cached(&sync, &tl, 15.f, &st, out_a, 4);
    if (c1 != 1 || out_a[0] != 0)
        return fail("initial cached eval mismatch");
    uint8_t out_bi[4];
    uint8_t c2 =
        rogue_animation_collision_evaluate_timeline_cached(&sync, &tl, 17.f, &st, out_bi, 4);
    if (c2 != 1 || out_bi[0] != 0)
        return fail("cache reuse expected for 2ms advance");

    /* Cross a boundary: jump to time that (scaled) enters w1 (real time 60ms -> scaled 120) */
    uint8_t out_c[4];
    uint8_t c3 =
        rogue_animation_collision_evaluate_timeline_cached(&sync, &tl, 60.f, &st, out_c, 4);
    int found_w1 = (c3 == 1 && out_c[0] == 1);
    if (!found_w1)
        return fail("expected window 1 after boundary crossing");

    /* Smooth interpolation factor test: t=0.5 between 0 and 100 -> smoothstep(0.5)=0.5 (still),
     * between 0 and 100 at 25ms real -> scaled 50ms -> raw t=0.5 -> smoothstep =>0.5 */
    const struct RogueHitPixelMaskFrame *ma = NULL, *mb = NULL;
    float t = 0.f;
    if (!rogue_animation_collision_interpolate_masks(&sync, 25.f, &ma, &mb, &t))
        return fail("interpolate failed");
    if (t < 0.49f || t > 0.51f)
        return fail("expected smoothed t near 0.5");

    /* Quality toggle: set quality low (linear) expect raw t ~0.25 at time 12.5ms (scaled 25) */
    sync.interpolation_quality = 0.f;
    ma = mb = NULL;
    t = 0.f;
    if (!rogue_animation_collision_interpolate_masks(&sync, 12.5f, &ma, &mb, &t))
        return fail("interp2 fail");
    if (t < 0.24f || t > 0.26f)
        return fail("linear t mismatch (expected ~0.25)");

    return 0;
}
