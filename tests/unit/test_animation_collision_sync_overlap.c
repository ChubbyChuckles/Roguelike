/* test_animation_collision_sync_overlap.c - Overlap resolution strategies */
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
    /* Two overlapping windows around t=50..80 with different intensities and start times */
    RogueCollisionTimelineWindow w0 = {.timestamp_ms = 40.f,
                                       .duration_ms = 40.f,
                                       .collision_mask_index = 0,
                                       .intensity_multiplier = 1.0f}; /* 40..80 */
    RogueCollisionTimelineWindow w1 = {.timestamp_ms = 50.f,
                                       .duration_ms = 40.f,
                                       .collision_mask_index = 1,
                                       .intensity_multiplier = 2.0f}; /* 50..90 */
    rogue_collision_timeline_add(&tl, &w0);
    rogue_collision_timeline_add(&tl, &w1);

    /* At t=60 both active: strategy 0 should pick higher intensity (w1 -> index 1) */
    int idx = rogue_animation_collision_resolve_overlap(
        &tl, 60.f, ROGUE_OVERLAP_HIGHEST_INTENSITY_LATEST_START_LOWEST_INDEX);
    if (idx != 1)
        return fail("expected index 1 (higher intensity) at t=60");

    /* Tie on intensity: make new window w2 with same intensity but later start */
    RogueCollisionTimelineWindow w2 = {.timestamp_ms = 55.f,
                                       .duration_ms = 20.f,
                                       .collision_mask_index = 2,
                                       .intensity_multiplier = 2.0f}; /* 55..75 */
    rogue_collision_timeline_add(&tl, &w2);
    idx = rogue_animation_collision_resolve_overlap(
        &tl, 60.f, ROGUE_OVERLAP_HIGHEST_INTENSITY_LATEST_START_LOWEST_INDEX);
    if (idx != 2)
        return fail("expected index 2 (tie on intensity, latest start) at t=60");

    /* Earliest-start strategy should pick w1 (start 50) over w2 (55) at t=60 */
    idx = rogue_animation_collision_resolve_overlap(&tl, 60.f,
                                                    ROGUE_OVERLAP_EARLIEST_START_THEN_LOWEST_INDEX);
    if (idx != 1)
        return fail("expected index 1 (earliest start) at t=60");

    /* Scaled variant: with speed=2, real 30ms -> scaled 60ms, expect same */
    RogueAnimationCollisionSync sync = {0};
    sync.playback_speed = 2.f;
    idx = rogue_animation_collision_resolve_overlap_scaled(
        &sync, &tl, 30.f, ROGUE_OVERLAP_HIGHEST_INTENSITY_LATEST_START_LOWEST_INDEX);
    if (idx != 2)
        return fail("expected index 2 (scaled-time) at real 30ms");

    return 0;
}
