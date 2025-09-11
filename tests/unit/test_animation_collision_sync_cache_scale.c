/* test_animation_collision_sync_cache_scale.c - Ensure cached evaluation respects playback_speed
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
    /* Timeline with a single window [50, 70] */
    RogueCollisionTimeline tl;
    rogue_collision_timeline_init(&tl);
    RogueCollisionTimelineWindow w = {.timestamp_ms = 50.f,
                                      .duration_ms = 20.f,
                                      .collision_mask_index = 0,
                                      .intensity_multiplier = 1.f};
    rogue_collision_timeline_add(&tl, &w);

    /* Sync with playback_speed=2 so scaled boundary at 50 occurs at real time 25 */
    RogueAnimationCollisionSync sync = {0};
    sync.playback_speed = 2.f;
    sync.frame_skip_threshold = 10; /* ms */

    RogueAnimationCollisionEvalState st = {0};
    uint8_t idx[4];

    /* Initial eval at t=20ms real (scaled 40) -> not inside window */
    uint8_t c0 = rogue_animation_collision_evaluate_timeline_cached(&sync, &tl, 20.f, &st, idx, 4);
    if (c0 != 0)
        return fail("expected no active windows at 20ms real (scaled 40)");

    /* Advance by 6ms (<= threshold): 26ms real (scaled 52) crosses start boundary at scaled 50.
       Cached path must detect boundary and recompute to include the window. */
    uint8_t c1 = rogue_animation_collision_evaluate_timeline_cached(&sync, &tl, 26.f, &st, idx, 4);
    if (c1 != 1 || idx[0] != 0)
        return fail(
            "expected cache to invalidate on scaled boundary crossing and include window 0");

    return 0;
}
