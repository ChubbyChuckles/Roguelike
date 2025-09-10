/* animation_collision_sync.c - Milestone 3.2 Scaffold Implementation
 *
 * Provides baseline timeline window evaluation and keyframe mask interpolation helpers.
 * Advanced features (multi-window overlap resolution strategies, dynamic speed scaling,
 * spline interpolation, adaptive quality selection, frame skipping heuristics, event
 * batching) are intentionally deferred to later deepening slices. This scaffold focuses
 * on deterministic behavior suitable for unit testing and early integration without
 * introducing performance complexity.
 */

#include "game/animation_collision_sync.h"
#include <stddef.h>

uint8_t rogue_animation_collision_evaluate_timeline(const RogueCollisionTimeline* tl, float time_ms,
                                                    uint8_t* out_indices, uint8_t max_indices)
{
    if (!tl)
        return 0;
    if (time_ms < 0.f)
        time_ms = 0.f;
    if (tl->loop_animation && tl->total_cycle_time_ms > 0.f)
    {
        /* Wrap using simple modulo (avoid fmod precision for huge values by manual subtraction). */
        while (time_ms >= tl->total_cycle_time_ms)
            time_ms -= tl->total_cycle_time_ms;
    }
    uint8_t active_count = 0;
    for (uint8_t i = 0; i < tl->window_count; ++i)
    {
        const RogueCollisionTimelineWindow* w = &tl->windows[i];
        if (w->duration_ms <= 0.f)
            continue; /* ignore degenerate */
        float end = w->timestamp_ms + w->duration_ms;
        if (time_ms < w->timestamp_ms || time_ms > end)
            continue;
        if (out_indices && active_count < max_indices)
            out_indices[active_count] = i;
        active_count++;
    }
    return active_count;
}

int rogue_animation_collision_interpolate_masks(const RogueAnimationCollisionSync* sync,
                                                float time_ms,
                                                const struct RogueHitPixelMaskFrame** out_a,
                                                const struct RogueHitPixelMaskFrame** out_b,
                                                float* out_t)
{
    if (out_a)
        *out_a = NULL;
    if (out_b)
        *out_b = NULL;
    if (out_t)
        *out_t = 0.f;
    if (!sync || sync->keyframe_count == 0 || !sync->keyframe_timestamps)
        return 0;
    /* Clamp negative time */
    if (time_ms < 0.f)
        time_ms = 0.f;
    /* Simple linear search (keyframe count expected small). Later slice may add binary search. */
    uint8_t last_index = sync->keyframe_count - 1;
    /* If time beyond last timestamp -> clamp */
    float last_ts = sync->keyframe_timestamps[last_index];
    if (time_ms >= last_ts || !sync->smooth_interpolation || sync->keyframe_count == 1)
    {
        if (out_a)
        {
            /* Find greatest keyframe <= time */
            uint8_t k = 0;
            for (uint8_t i = 0; i < sync->keyframe_count; ++i)
            {
                if (sync->keyframe_timestamps[i] <= time_ms)
                    k = i;
                else
                    break;
            }
            if (sync->keyframe_masks)
                *out_a = &sync->keyframe_masks[k];
        }
        return 1;
    }
    /* Identify bracketing keyframes (i,i+1) where ts[i] <= time < ts[i+1] */
    uint8_t base = 0;
    for (uint8_t i = 0; i < last_index; ++i)
    {
        float a_ts = sync->keyframe_timestamps[i];
        float b_ts = sync->keyframe_timestamps[i + 1];
        if (time_ms < b_ts)
        {
            base = i;
            break;
        }
    }
    float a_ts = sync->keyframe_timestamps[base];
    float b_ts = sync->keyframe_timestamps[base + 1];
    float denom = (b_ts - a_ts);
    float t = 0.f;
    if (denom > 0.f)
        t = (time_ms - a_ts) / denom;
    if (t < 0.f)
        t = 0.f;
    if (t > 1.f)
        t = 1.f;
    if (out_a && sync->keyframe_masks)
        *out_a = &sync->keyframe_masks[base];
    if (out_b && sync->keyframe_masks)
        *out_b = &sync->keyframe_masks[base + 1];
    if (out_t)
        *out_t = t; /* linear factor */
    return 1;
}

/* Future extensions (deferred):
 *  - rogue_animation_collision_evaluate_timeline_ex: multi-speed scaling & event queue emission
 *  - Mask interpolation quality modes (cubic, Hermite, distance-field based blending)
 *  - Adaptive frame skip: monitor evaluation cost & dynamically widen sampling interval
 */
