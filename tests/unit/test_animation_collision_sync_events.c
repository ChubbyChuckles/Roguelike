/* test_animation_collision_sync_events.c - Validate chronological ordering and scaled wrapper
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
    RogueCollisionTimeline tl;
    rogue_collision_timeline_init(&tl);
    tl.loop_animation = 0;
    tl.total_cycle_time_ms = 0.f;
    RogueCollisionTimelineWindow w0 = {.timestamp_ms = 10.f,
                                       .duration_ms = 20.f,
                                       .collision_mask_index = 0,
                                       .intensity_multiplier = 1.f};
    RogueCollisionTimelineWindow w1 = {.timestamp_ms = 30.f,
                                       .duration_ms = 10.f,
                                       .collision_mask_index = 1,
                                       .intensity_multiplier = 1.f};
    /* w0: 10..30, w1: 30..40 so there is a tie at 30 (EXIT w0, ENTER w1); ENTER should come first
     */
    rogue_collision_timeline_add(&tl, &w0);
    rogue_collision_timeline_add(&tl, &w1);

    RogueCollisionTimelineEvent ev[8];
    uint8_t n = rogue_animation_collision_timeline_events(&tl, 5.f, 35.f, ev, 8);
    if (n < 2)
        return fail("expected at least two events");
    /* find the two events at 30ms */
    int enter_first = 0;
    for (uint8_t i = 0; i + 1 < n; ++i)
    {
        if (ev[i].event_time_ms == 30.f && ev[i + 1].event_time_ms == 30.f)
        {
            if (ev[i].type == ROGUE_COLLISION_WINDOW_ENTER &&
                ev[i + 1].type == ROGUE_COLLISION_WINDOW_EXIT)
            {
                enter_first = 1;
                break;
            }
        }
    }
    if (!enter_first)
        return fail("expected ENTER before EXIT at the same timestamp");

    /* Scaled wrapper: same timeline but evaluate [5, 20] with speed=2 -> scaled [10,40] */
    RogueAnimationCollisionSync sync = {0};
    sync.playback_speed = 2.f;
    RogueCollisionTimelineEvent evs[8];
    uint8_t ns = rogue_animation_collision_timeline_events_scaled(&sync, &tl, 5.f, 20.f, evs, 8);
    if (ns != n)
        return fail("scaled event count mismatch");

    return 0;
}
