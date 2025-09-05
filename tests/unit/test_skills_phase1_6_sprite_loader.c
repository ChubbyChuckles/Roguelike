#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../src/core/skills/skill_sprite_loader.h"
#include "../../src/graphics/sprite.h"

static void make_dummy_tex(RogueTexture* t, int w, int h)
{
    memset(t, 0, sizeof *t);
    t->w = w;
    t->h = h;
}

static void test_grid_builder_basic()
{
    RogueTexture tex;
    make_dummy_tex(&tex, 128, 64);
    RogueSprite frames[16];
    int n = rogue_skill_build_grid_frames(&tex, 4, 2, frames, 16);
    assert(n == 8);
    // frame size
    for (int i = 0; i < n; ++i)
    {
        assert(frames[i].tex == &tex);
        assert(frames[i].sw == 32);
        assert(frames[i].sh == 32);
    }
    // spot-check first row
    assert(frames[0].sx == 0 && frames[0].sy == 0);
    assert(frames[1].sx == 32 && frames[1].sy == 0);
    assert(frames[3].sx == 96 && frames[3].sy == 0);
    // second row first frame
    assert(frames[4].sx == 0 && frames[4].sy == 32);
}

static void test_anim_sampler()
{
    int idx;
    idx = rogue_skill_anim_sample_index(5, 100, 0, 1);
    assert(idx == 0);
    idx = rogue_skill_anim_sample_index(5, 100, 99, 1);
    assert(idx == 0);
    idx = rogue_skill_anim_sample_index(5, 100, 100, 1);
    assert(idx == 1);
    idx = rogue_skill_anim_sample_index(5, 100, 499, 1);
    assert(idx == 4);
    idx = rogue_skill_anim_sample_index(5, 100, 500, 1);
    assert(idx == 0);

    idx = rogue_skill_anim_sample_index(5, 100, 500, 0);
    assert(idx == 4);
}

int main(void)
{
    test_grid_builder_basic();
    test_anim_sampler();
    printf("ok\n");
    return 0;
}
