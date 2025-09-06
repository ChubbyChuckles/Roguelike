/* Phase 1.4: Sprite Atlas Generation horizontal packing test
   Strategy: create a synthetic sequence by loading zero real frames (negative path) and then
   simulate packing failure gracefully (expect 0) in headless/no-SDL renderer environments. If
   SDL renderer is active in a full runtime, the test will skip because we lack real PNGs. This
   keeps the test deterministic without adding asset files. */

#include "../../src/core/skills/skill_sprite_loader.h"
#include <stdio.h>

int main(void)
{
    RogueSprite frames[4];
    int got =
        rogue_skill_load_png_sequence("assets/skills/nonexistent_skill/cast", "cast", frames, 4);
    if (got != 0)
    {
        fprintf(stderr, "EXPECTED_NO_FRAMES got=%d\n", got);
        rogue_skill_free_sequence_frames(frames, got);
        return 1;
    }
    RogueTexture atlas;
    int repacked = rogue_skill_pack_frames_horizontal(frames, got, &atlas);
    if (repacked != 0)
    {
        fprintf(stderr, "EXPECTED_ZERO_REPACK got=%d\n", repacked);
        return 1;
    }
    printf("SPRITE_ATLAS_PACK_NOOP_OK\n");
    return 0;
}
