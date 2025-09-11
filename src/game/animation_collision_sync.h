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
        uint32_t animation_id;      /* Reference to animation definition */
        float* keyframe_timestamps; /* Critical collision timing points (ms) */
        uint8_t keyframe_count;     /* Number of keyframes */
        struct RogueHitPixelMaskFrame**
            keyframe_masks;            /* Optional mask frame pointers (one per keyframe) */
        bool smooth_interpolation;     /* If true, interpolate between keyframes */
        float interpolation_quality;   /* 0=linear (only mode implemented) */
        uint32_t frame_skip_threshold; /* (Deferred) perf-based skipping trigger */
        float playback_speed;          /* Speed scaling (1.0 default; <=0 treated as 1) */
        /* Scratch blended (morphed) frame (owned) reused across calls when dimensions stable. */
        struct RogueHitPixelMaskFrame* blended_scratch;
        int blended_w;
        int blended_h;
        /* If true, blended_scratch was acquired from the internal pool and should be returned
            on release instead of freed. */
        bool blended_from_pool;
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

    /* Event batching (enter/exit) scaffold for later deepening (Milestone 3.2+). */
    typedef enum RogueCollisionTimelineEventType
    {
        ROGUE_COLLISION_WINDOW_ENTER = 1,
        ROGUE_COLLISION_WINDOW_EXIT = 2
    } RogueCollisionTimelineEventType;

    typedef struct RogueCollisionTimelineEvent
    {
        RogueCollisionTimelineEventType type;
        uint8_t window_index; /* Index into timeline windows[] */
        float event_time_ms;  /* Absolute time passed to evaluator when event triggered */
    } RogueCollisionTimelineEvent;

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

    /* Speed-scaled timeline evaluation: applies sync->playback_speed to time domain before
     * evaluating windows. Provided as a wrapper to avoid breaking existing API semantics.
     * playback_speed<=0 treated as 1.0. */
    uint8_t rogue_animation_collision_evaluate_timeline_scaled(
        const RogueAnimationCollisionSync* sync, const RogueCollisionTimeline* tl, float time_ms,
        uint8_t* out_indices, uint8_t max_indices);

    /* Cached evaluation state enabling performance-aware frame skipping without missing window
     * boundaries. If the elapsed time since last evaluation is below threshold and no window
     * boundary (start or end) exists inside (last_time, curr_time], the previous active set is
     * reused. */
    typedef struct RogueAnimationCollisionEvalState
    {
        float last_time_ms;
        uint8_t last_active_indices[16];
        uint8_t last_active_count;
        uint8_t initialized;
    } RogueAnimationCollisionEvalState;

    /* Evaluate timeline with frame skipping: uses sync->frame_skip_threshold (ms). If threshold==0
     * behaves like normal evaluation. Safe for looping timelines. Returns active count. */
    uint8_t rogue_animation_collision_evaluate_timeline_cached(
        const RogueAnimationCollisionSync* sync, const RogueCollisionTimeline* tl, float time_ms,
        RogueAnimationCollisionEvalState* state, uint8_t* out_indices, uint8_t max_indices);

    /* Emit enter/exit events for windows between prev_time_ms (exclusive) and curr_time_ms
     * (inclusive). Handles looping timelines by linearizing the interval when loop_animation==true.
     * Assumes (curr_time_ms - prev_time_ms) < 2 * total_cycle_time_ms (frame delta semantics).
     * Returns number of events emitted (truncated to max_events). Order: chronological; ties
     * resolve with ENTER before EXIT at the same timestamp. */
    uint8_t rogue_animation_collision_timeline_events(const RogueCollisionTimeline* tl,
                                                      float prev_time_ms, float curr_time_ms,
                                                      RogueCollisionTimelineEvent* out_events,
                                                      uint8_t max_events);

    /* Speed-scaled variant of timeline event emission. Applies sync->playback_speed to the time
     * domain before delegating to rogue_animation_collision_timeline_events().
     * playback_speed<=0 treated as 1.0. */
    uint8_t rogue_animation_collision_timeline_events_scaled(
        const RogueAnimationCollisionSync* sync, const RogueCollisionTimeline* tl,
        float prev_time_ms, float curr_time_ms, RogueCollisionTimelineEvent* out_events,
        uint8_t max_events);

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

    /* Integration helper (Milestone 3.2 -> future mask morphing slice): derive interpolation
     * directly from a skill collision layer's fractional frame index. This does NOT yet perform
     * mask morphing – it simply maps the fractional frame to keyframe bracketing indices. */
    struct RogueSkillCollisionLayer; /* from skill_collision_manager.h */
    int rogue_animation_collision_interpolate_from_skill_layer(
        const RogueAnimationCollisionSync* sync, const struct RogueSkillCollisionLayer* layer,
        const struct RogueHitPixelMaskFrame** out_a, const struct RogueHitPixelMaskFrame** out_b,
        float* out_t);

    /* Baseline mask morphing (Milestone 3.2 deepening: union-or blend). Returns a pointer to a
     * mask representing the collision shape at time_ms. Behavior:
     *  - If interpolation disabled or <2 keyframes -> returns base keyframe frame.
     *  - For t in (0,1) between two keyframes with matching dimensions: produces (and caches in
     *    sync->blended_scratch) a union OR of A and B (conservative expansion).
     *  - If dimensions mismatch falls back to A to avoid expensive resample (future slice).
     *  - Endpoints (t<=0.15 or t>=0.85) return A or B directly to skip blend work.
     * Caller does not own returned pointer. */
    const struct RogueHitPixelMaskFrame*
    rogue_animation_collision_morph_mask(RogueAnimationCollisionSync* sync, float time_ms);

    /* Release any internal scratch buffers (call in teardown). */
    void rogue_animation_collision_sync_release(RogueAnimationCollisionSync* sync);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_GAME_ANIMATION_COLLISION_SYNC_H */
