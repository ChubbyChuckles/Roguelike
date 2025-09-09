/**
 * @file animation.h
 * @brief Sprite animation system with Aseprite integration.
 *
 * Provides a simple animation system that can load sprite animations from
 * Aseprite-exported data (JSON + PNG) or fall back to fixed grid slicing.
 * Supports frame timing, looping animations, and frame sampling based on
 * elapsed time. Designed to work with Aseprite's JSON array export format.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#ifndef ROGUE_GRAPHICS_ANIMATION_H
#define ROGUE_GRAPHICS_ANIMATION_H

#include "sprite.h"
#include <stdbool.h>

/**
 * @brief Individual animation frame data.
 *
 * Represents a single frame in an animation sequence, containing
 * the sprite graphics and timing information for frame display.
 */
typedef struct RogueAnimFrame
{
    RogueSprite sprite;  ///< The sprite graphics for this frame
    int duration_ms;     ///< Frame display duration in milliseconds
} RogueAnimFrame;

/**
 * @brief Complete animation sequence.
 *
 * Contains all frames of an animation along with the shared texture
 * and timing information. Supports up to 32 frames per animation
 * and provides total duration for seamless looping.
 */
typedef struct RogueAnimation
{
    RogueTexture texture;      ///< Texture containing all animation frames
    RogueAnimFrame frames[32]; ///< Array of animation frames (max 32)
    int frame_count;           ///< Number of frames in this animation
    int total_duration_ms;     ///< Total animation duration for looping calculations
} RogueAnimation;

/**
 * @brief Load an animation from Aseprite export or PNG grid.
 *
 * Attempts to load animation data from Aseprite-exported JSON and PNG files.
 * The JSON should be exported using Aseprite's format:
 * `aseprite -b input.aseprite --data output.json --sheet output.png --format json-array`
 * 
 * If JSON metadata is missing or cannot be parsed, falls back to slicing
 * the PNG into a regular grid using the provided fallback dimensions.
 *
 * @param anim Pointer to animation structure to populate
 * @param png_path Path to the PNG sprite sheet file
 * @param json_path Path to the JSON metadata file (can be NULL for grid fallback)
 * @param fallback_frame_w Frame width for grid fallback (used if JSON unavailable)
 * @param fallback_frame_h Frame height for grid fallback (used if JSON unavailable)
 * @return true on successful load, false on failure
 */
bool rogue_animation_load(RogueAnimation* anim, const char* png_path, const char* json_path,
                          int fallback_frame_w, int fallback_frame_h);

/**
 * @brief Unload and free animation resources.
 *
 * Releases all resources used by an animation including textures
 * and resets the animation structure. The animation is no longer
 * valid for use after calling this function.
 *
 * @param anim Pointer to the animation to unload
 */
void rogue_animation_unload(RogueAnimation* anim);

/**
 * @brief Sample the animation at a specific time for looping playback.
 *
 * Returns the appropriate animation frame for the given elapsed time.
 * The animation loops seamlessly - when elapsed_ms exceeds the total
 * animation duration, it wraps around to the beginning. This allows
 * continuous animation playback by simply passing increasing time values.
 *
 * @param anim Pointer to the animation to sample
 * @param elapsed_ms Total elapsed time since animation start in milliseconds
 * @return Pointer to the current animation frame, or NULL if animation is invalid
 * 
 * @note The returned pointer is only valid until the next call to this function
 *       or until the animation is unloaded.
 */
const RogueAnimFrame* rogue_animation_sample(const RogueAnimation* anim, int elapsed_ms);

#endif
