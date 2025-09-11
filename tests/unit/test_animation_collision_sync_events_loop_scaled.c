/* test_animation_collision_sync_events_loop_scaled.c
 * Validate event ordering across loop wrap and parity with scaled variant. */
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
    RogueCollisionTimeline tl; /* two windows: one ends near cycle end, one starts near 0 */
    rogue_collision_timeline_init(&tl);
    tl.loop_animation = 1;
    tl.total_cycle_time_ms = 100.f;
    RogueCollisionTimelineWindow w0 = {.timestamp_ms = 80.f,
                                       .duration_ms = 15.f,
                                       .collision_mask_index = 0,
                                       .intensity_multiplier = 1.f}; /* 80..95 */
    RogueCollisionTimelineWindow w1 = {.timestamp_ms = 5.f,
                                       .duration_ms = 20.f,
                                       .collision_mask_index = 1,
                                       .intensity_multiplier = 1.f}; /* 5..25 */
    rogue_collision_timeline_add(&tl, &w0);
    rogue_collision_timeline_add(&tl, &w1);

    /* Cross wrap interval: prev=90, curr=10 -> expect EXIT w0 at 95 then ENTER w1 at 5 (wrap-aware
     * order) */
    RogueCollisionTimelineEvent ev[4];
    uint8_t n = rogue_animation_collision_timeline_events(&tl, 90.f, 10.f, ev, 4);
    if (!expect(n >= 2, "expected at least two events across wrap"))
        return 1;
    if (!expect(ev[0].type == ROGUE_COLLISION_WINDOW_EXIT && ev[0].window_index == 0 &&
                    ev[0].event_time_ms == 95.f,
                "first event should be EXIT w0 at 95"))
        return 2;
    if (!expect(ev[1].type == ROGUE_COLLISION_WINDOW_ENTER && ev[1].window_index == 1 &&
                    ev[1].event_time_ms == 5.f,
                "second event should be ENTER w1 at 5"))
        return 3;

    /* Scaled parity: playback_speed=2, prev=45->curr=5 maps to 90->10 in timeline space */
    RogueAnimationCollisionSync sync = {0};
    sync.playback_speed = 2.f;
    RogueCollisionTimelineEvent evs[4];
    uint8_t ns = rogue_animation_collision_timeline_events_scaled(&sync, &tl, 45.f, 5.f, evs, 4);
    if (!expect(ns == n, "scaled event count should match"))
        return 4;
    for (uint8_t i = 0; i < n; ++i)
    {
        if (!expect(evs[i].type == ev[i].type && evs[i].window_index == ev[i].window_index &&
                        evs[i].event_time_ms == ev[i].event_time_ms,
                    "scaled events should match kinds, indices, and times"))
            return 5;
    }
    return 0;
}
