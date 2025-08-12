#ifndef ROGUE_GRAPHICS_ANIMATION_H
#define ROGUE_GRAPHICS_ANIMATION_H

#include "graphics/sprite.h"
#include <stdbool.h>

/*
     Simple Aseprite-driven animation loader.
     It attempts to parse the JSON exported with:
         aseprite -b input.aseprite --data output.json --sheet output.png --format json-array
     If the JSON is missing it falls back to slicing the PNG into a fixed grid (fw x fh).
*/

typedef struct RogueAnimFrame {
        RogueSprite sprite;
        int duration_ms; /* frame display length */
} RogueAnimFrame;

typedef struct RogueAnimation {
    RogueTexture texture; /* texture for all frames */
    RogueAnimFrame frames[32]; /* support up to 32 frames */
    int frame_count;
    int total_duration_ms; /* sum for looping */
} RogueAnimation;

/* Load an animation from PNG + optional JSON metadata; fw/fh used only if JSON absent */
bool rogue_animation_load(RogueAnimation* anim, const char* png_path, const char* json_path, int fallback_frame_w, int fallback_frame_h);
void rogue_animation_unload(RogueAnimation* anim);

/* Looping sample: elapsed_ms is total time since animation start */
const RogueAnimFrame* rogue_animation_sample(const RogueAnimation* anim, int elapsed_ms);

#endif
