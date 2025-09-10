/* sprite_atlas_collision.h - Milestone 1.3 (initial slice)
 * Minimal data structures + APIs for sprite atlas region extraction and
 * animation collision sampling. Advanced roadmap features (automatic origin
 * detection, interpolation refinement, keyframe optimization, background
 * building) are intentionally deferred to keep this slice small & testable.
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct RogueHitPixelMaskFrame; /* forward declaration */

    typedef struct RogueSpriteAtlasRegion
    {
        uint16_t x, y, w, h;        /* atlas rectangle */
        int16_t origin_x, origin_y; /* origin within region (can be negative) */
        float scale_x, scale_y;     /* non‑uniform scale (advisory) */
        uint8_t frame_index;        /* animation frame index hint */
    } RogueSpriteAtlasRegion;

/* Small-object optimization threshold (frames stored inline when <= this). */
#ifndef ROGUE_ANIM_INLINE_MAX_FRAMES
#define ROGUE_ANIM_INLINE_MAX_FRAMES 8
#endif

    typedef struct RogueAnimationCollisionSet
    {
        uint8_t frame_count;        /* number of frames */
        uint32_t total_duration_ms; /* total loop duration */
        /* Primary frame pointer arrays (may alias inline arrays). */
        const struct RogueHitPixelMaskFrame** frames;
        uint32_t* frame_timings;    /* cumulative start times (len = frame_count+1) */
        float interpolation_factor; /* 0..1 global smoothing multiplier */
        /* Inline storage (used when frame_count <= ROGUE_ANIM_INLINE_MAX_FRAMES). */
        const struct RogueHitPixelMaskFrame* frames_inline[ROGUE_ANIM_INLINE_MAX_FRAMES];
        uint32_t frame_timings_inline[ROGUE_ANIM_INLINE_MAX_FRAMES + 1];
        int using_inline; /* boolean */
        /* Scratch blended frame (lazy allocated / resized) for interpolation union. */
        struct RogueHitPixelMaskFrame* blended_frame; /* owned */
        int blended_w, blended_h;                     /* current capacity */
    } RogueAnimationCollisionSet;

    /* Extract region from atlas mask into a new standalone frame (allocated). */
    int rogue_sprite_atlas_extract_collision_region(const struct RogueHitPixelMaskFrame* atlas,
                                                    const RogueSpriteAtlasRegion* region,
                                                    struct RogueHitPixelMaskFrame* out_frame);

    /* Free bits of a region-extracted frame (helper). */
    void rogue_sprite_atlas_region_free_frame(struct RogueHitPixelMaskFrame* f);

    /* Build animation collision set (allocates internal arrays). */
    int rogue_animation_collision_build(RogueAnimationCollisionSet* set,
                                        const struct RogueHitPixelMaskFrame** frames,
                                        const uint32_t* frame_durations_ms, uint8_t frame_count,
                                        float interpolation_factor);

    /* Free internal arrays (does not free frames). */
    void rogue_animation_collision_free(RogueAnimationCollisionSet* set);

    /* Sample animation at time (ms); returns frames A,B and interpolation t (0..1 * factor). */
    int rogue_animation_collision_sample(const RogueAnimationCollisionSet* set, uint32_t t_ms,
                                         const struct RogueHitPixelMaskFrame** out_a,
                                         const struct RogueHitPixelMaskFrame** out_b, float* out_t);

    /* Optional: produce (or reuse internal) blended collision mask representing an
     * approximate interpolation between two sampled frames at time t_ms. The current
     * strategy is a simple UNION (bitwise OR) of frame A and frame B when 0<t<1; for
     * t near endpoints (<0.15 or >0.85) it returns a direct pointer to A or B to avoid
     * extra work. Returns pointer to an owned scratch frame (valid until next call or
     * free) or NULL on failure. */
    const struct RogueHitPixelMaskFrame*
    rogue_animation_collision_sample_blended(RogueAnimationCollisionSet* set, uint32_t t_ms,
                                             float* out_local_t);

    /* Release scratch blended resources without destroying the set. */
    void rogue_animation_collision_release_blended(RogueAnimationCollisionSet* set);

#ifdef __cplusplus
}
#endif
