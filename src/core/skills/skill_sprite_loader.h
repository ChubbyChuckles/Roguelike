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

    /* Build frames from a simple packed-atlas JSON description.
     * Supported JSON shapes:
     * 1) { "frames": [ { "x":int, "y":int, "w":int, "h":int }, ... ] }
     * 2) { "frames": { "name": { "frame": {"x":int,"y":int,"w":int,"h":int} }, ... } }
     * Returns count of frames written (up to max_out). Ignores invalid entries.
     * The texture pointer is assigned to each RogueSprite; ownership remains with caller.
     */
    int rogue_skill_build_packed_frames_from_json_text(const RogueTexture* tex,
                                                       const char* json_text,
                                                       RogueSprite* out_frames, int max_out);

    /* Convenience: load JSON from file path and parse like the text variant. */
    int rogue_skill_build_packed_frames_from_file(const RogueTexture* tex, const char* json_path,
                                                  RogueSprite* out_frames, int max_out);

    /* Phase 1.4: PNG Sequence Loader
         Scan a directory for files matching prefix + zero-padded/incremented numeric suffix
       pattern: <prefix>_001.png, <prefix>_002.png, ... or <prefix>1.png, <prefix>2.png ... Loads
       each frame into a texture then builds a sprite referencing an atlas frame list. For now we
       load each PNG as an independent texture-backed sprite (no atlas packing yet). Returns number
       of frames loaded (up to max_out). Missing gaps terminate the scan. */
    int rogue_skill_load_png_sequence(const char* directory, const char* prefix,
                                      RogueSprite* out_frames, int max_out);

    /* Phase 1.4: Sprite Atlas Generation (horizontal pack)
       Take an array of frames that currently each own an independent texture (e.g. output of
       rogue_skill_load_png_sequence) and pack them into a single horizontal atlas texture to
       reduce draw calls. On success:
         - out_atlas is populated (caller owns & must destroy via rogue_texture_destroy)
         - The input frames are rewritten in-place so each references out_atlas with updated
           sx/sy/sw/sh describing its region.
         - All per-frame textures previously owned are destroyed & freed.
       Returns number of frames repacked (== frame_count) or 0 on failure / unsupported backend.
       Safe no-op if frame_count <= 0. Headless (no renderer) builds return 0 without error. */
    int rogue_skill_pack_frames_horizontal(RogueSprite* frames, int frame_count,
                                           RogueTexture* out_atlas);

    /* Free a PNG sequence frame array produced by rogue_skill_load_png_sequence WITHOUT packing.
       Destroys & frees each owning texture pointer. Frames are zeroed. Safe to call with NULL or
       frame_count <= 0. */
    void rogue_skill_free_sequence_frames(RogueSprite* frames, int frame_count);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_SKILL_SPRITE_LOADER_H */
