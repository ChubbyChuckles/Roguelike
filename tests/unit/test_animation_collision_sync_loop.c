/* test_animation_collision_sync_loop.c - Looping timeline + speed scaling + event batching scaffold
 * test */
#include "game/animation_collision_sync.h"
#include <stdio.h>

static int expect(int cond, const char* msg)
{
    if (!cond)
    {
        fprintf(stderr, "%s\n", msg);
        return 0;
    }
    return 1;
}

int main(void)
{
    RogueCollisionTimeline tl;
    rogue_collision_timeline_init(&tl);
    tl.loop_animation = 1;
    tl.total_cycle_time_ms = 100.f;
    RogueCollisionTimelineWindow w0 = {.timestamp_ms = 10.f,
                                       .duration_ms = 20.f,
                                       .collision_mask_index = 0,
                                       .intensity_multiplier = 1.f}; /* 10..30 */
    RogueCollisionTimelineWindow w1 = {.timestamp_ms = 60.f,
                                       .duration_ms = 30.f,
                                       .collision_mask_index = 1,
                                       .intensity_multiplier = 1.f}; /* 60..90 */
    rogue_collision_timeline_add(&tl, &w0);
    rogue_collision_timeline_add(&tl, &w1);

    /* Active window wrapping: sample at 115ms (wrap ->15) expect w0 */
    uint8_t idx[4];
    if (!expect(rogue_animation_collision_evaluate_timeline(&tl, 115.f, idx, 4) == 1 && idx[0] == 0,
                "wrap sample 115ms -> expect window 0"))
        return 1;

    /* Event batching across wrap: prev 95 -> curr 105 crosses cycle (wrap). We only expect EXIT of
       w1 because new cycle time (5ms) has not yet reached w0 start (10ms). */
    RogueCollisionTimelineEvent events[8];
    uint8_t ec = rogue_animation_collision_timeline_events(&tl, 85.f, 105.f, events, 8);
    int saw_exit_w1 = 0;
    for (uint8_t i = 0; i < ec; ++i)
    {
        if (events[i].type == ROGUE_COLLISION_WINDOW_EXIT && events[i].window_index == 1)
            saw_exit_w1 = 1;
    }
    if (!expect(saw_exit_w1, "expected EXIT w1 across wrap"))
        return 2;

    /* Speed scaling scaffold: create sync with keyframes and playback_speed (not yet used in
     * evaluator directly, but ensure field accessible) */
    RogueAnimationCollisionSync sync = {0};
    float kts[2] = {0.f, 100.f};
    sync.keyframe_timestamps = kts;
    sync.keyframe_count = 2;
    sync.smooth_interpolation = 1;
    sync.playback_speed = 2.f; /* would double rate in future advanced slice */
    const struct RogueHitPixelMaskFrame *a = NULL, *b = NULL;
    float t = 0.f; /* Even without masks should succeed */
    if (!expect(rogue_animation_collision_interpolate_masks(&sync, 25.f, &a, &b, &t) == 1,
                "interpolate basic"))
        return 3;

    return 0; /* success */
}
