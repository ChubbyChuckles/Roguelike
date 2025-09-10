/* animation_collision_sync.h - Milestone 3.2 Scaffold: Advanced Animation Synchronization
 *
 * This initial slice introduces lightweight data structures and evaluation helpers for
 * keyframe-based collision timing and timeline window activation. It purposefully
 * omits advanced interpolation quality modes, performance-aware frame skipping, and
 * spline-based mask morphing which remain deferred to later deepening slices.
 *
 * Provided in this slice:
 *  - RogueAnimationCollisionSync: holds animation id, keyframe timestamps, optional
 *    keyframe mask pointers and simple linear interpolation toggle.
 *  - RogueCollisionTimeline: up to 16 collision windows (timestamp,duration, mask index,
 *    intensity multiplier) with optional looping support.
 *  - Timeline evaluation helper returning active window indices at an arbitrary time
 *    (supports looping by modulo on total_cycle_time_ms when enabled).
 *  - Basic mask interpolation helper that identifies the bracketing keyframes and
 *    returns an interpolation factor (0..1). Advanced cubic/spline quality modes and
 *    adaptive quality selection are deferred.
 */
#ifndef ROGUE_GAME_ANIMATION_COLLISION_SYNC_H
#define ROGUE_GAME_ANIMATION_COLLISION_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Forward declare pixel mask frame (from hit_pixel_mask.h) */
    struct RogueHitPixelMaskFrame; /* actual definition in hit_pixel_mask.h */

    typedef struct RogueAnimationCollisionSync
    {
        uint32_t animation_id;                         /* Reference to animation definition */
        float* keyframe_timestamps;                    /* Critical collision timing points (ms) */
        uint8_t keyframe_count;                        /* Number of keyframes */
        struct RogueHitPixelMaskFrame* keyframe_masks; /* Optional mask frames (one per keyframe) */
        bool smooth_interpolation;                     /* If true, interpolate between keyframes */
        float interpolation_quality;                   /* 0=linear (only mode implemented) */
        uint32_t frame_skip_threshold;                 /* (Deferred) perf-based skipping trigger */
    } RogueAnimationCollisionSync;

    typedef struct RogueCollisionTimelineWindow
    {
        float timestamp_ms;           /* Window start (ms) */
        float duration_ms;            /* Window length (ms) */
        uint8_t collision_mask_index; /* Index into keyframe_masks / derived mask set */
        float intensity_multiplier;   /* Damage/effect scalar */
    } RogueCollisionTimelineWindow;

    typedef struct RogueCollisionTimeline
    {
        RogueCollisionTimelineWindow windows[16];
        uint8_t window_count;
        bool loop_animation;       /* If true treat time modulo total_cycle_time_ms */
        float total_cycle_time_ms; /* Required if loop_animation true */
    } RogueCollisionTimeline;

    /* Initialize a timeline struct to empty. */
    static inline void rogue_collision_timeline_init(RogueCollisionTimeline* tl)
    {
        if (!tl)
            return;
        tl->window_count = 0;
        tl->loop_animation = false;
        tl->total_cycle_time_ms = 0.f;
    }

    /* Append a window, returns 0 on success, -1 on overflow. */
    static inline int rogue_collision_timeline_add(RogueCollisionTimeline* tl,
                                                   const RogueCollisionTimelineWindow* w)
    {
        if (!tl || !w)
            return -1;
        if (tl->window_count >= 16)
            return -1;
        tl->windows[tl->window_count++] = *w;
        return 0;
    }

    /* Evaluate active collision windows for a given time (ms). If out_indices provided, fills up to
     * max_indices with active window indices in ascending order. Returns number of active windows.
     * Time handling: clamps negative to 0. If loop_animation and total_cycle_time_ms>0, wraps time
     * by modulo. Windows with duration_ms<=0 are ignored. */
    uint8_t rogue_animation_collision_evaluate_timeline(const RogueCollisionTimeline* tl,
                                                        float time_ms, uint8_t* out_indices,
                                                        uint8_t max_indices);

    /* Interpolate (scaffold) between keyframe masks.
     * Returns 1 on success, 0 on invalid input. Provides:
     *  - out_a: pointer to base keyframe mask (never NULL on success if count>0)
     *  - out_b: pointer to next keyframe mask (NULL when clamped at end or interpolation disabled)
     *  - out_t: 0..1 interpolation factor (0 when no interpolation)
     * If smooth_interpolation==false or keyframe_count<=1 -> out_b=NULL, out_t=0.
     */
    int rogue_animation_collision_interpolate_masks(const RogueAnimationCollisionSync* sync,
                                                    float time_ms,
                                                    const struct RogueHitPixelMaskFrame** out_a,
                                                    const struct RogueHitPixelMaskFrame** out_b,
                                                    float* out_t);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_GAME_ANIMATION_COLLISION_SYNC_H */
