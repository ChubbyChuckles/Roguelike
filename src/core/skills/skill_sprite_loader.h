#ifndef ROGUE_SKILL_SPRITE_LOADER_H
#define ROGUE_SKILL_SPRITE_LOADER_H

#include "../../graphics/sprite.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Build a row-major grid of frames from a texture using explicit rows/cols.
     * Returns the number of frames written to out_frames (up to max_out).
     * Each frame references the provided texture (tex pointer is assigned).
     * Assumes texture dimensions are evenly divisible by cols/rows. If not,
     * the remainder pixels on the right/bottom are ignored.
     */
    int rogue_skill_build_grid_frames(const RogueTexture* tex, int cols, int rows,
                                      RogueSprite* out_frames, int max_out);

    /* Compute a frame index for a simple uniform-timing animation.
     * - frame_count: number of frames in the animation (> 0)
     * - frame_duration_ms: duration per frame (ms); if <= 0 defaults to 100ms
     * - elapsed_ms: total elapsed time (ms) since the animation started
     * - loop: if 1, wraps around; if 0, clamps to last frame
     */
    int rogue_skill_anim_sample_index(int frame_count, int frame_duration_ms, int elapsed_ms,
                                      int loop);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_SKILL_SPRITE_LOADER_H */
