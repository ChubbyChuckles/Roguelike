#include "../../src/core/skills/skill_sprite_loader.h"
#include "../../src/graphics/sprite.h"
#include <stdio.h>
#include <string.h>

/* Very small smoke test: attempts to load a non-existent sequence (expect 0) then, if assets for
   a known test prefix exist (future), ensures >0 frames. For now we focus on negative path so test
   passes deterministically without requiring new sample PNGs. */
int main(void)
{
    RogueSprite frames[16];
    int n = rogue_skill_load_png_sequence("assets/skills/test_skill", "skill_cast", frames, 16);
    if (n != 0)
    {
        fprintf(stderr, "UNEXPECTED_FRAMES_LOADED %d\n", n);
        return 1;
    }
    printf("PNG_SEQUENCE_NOOP_OK\n");
    return 0;
}
