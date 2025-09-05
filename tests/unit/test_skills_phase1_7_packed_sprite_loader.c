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

static void test_parse_array_frames()
{
    const char* json = "{\n"
                       "  \"frames\": [ {\"x\":0,\"y\":0,\"w\":16,\"h\":16}, "
                       "{\"x\":16,\"y\":0,\"w\":16,\"h\":16} ]\n"
                       "}";
    RogueTexture tex;
    make_dummy_tex(&tex, 64, 32);
    RogueSprite frames[8];
    int n = rogue_skill_build_packed_frames_from_json_text(&tex, json, frames, 8);
    assert(n == 2);
    assert(frames[0].sx == 0 && frames[0].sy == 0 && frames[0].sw == 16 && frames[0].sh == 16);
    assert(frames[1].sx == 16 && frames[1].sy == 0 && frames[1].sw == 16 && frames[1].sh == 16);
}

static void test_parse_object_frames()
{
    const char* json = "{\n"
                       "  \"frames\": {\n"
                       "    \"walk_0\": { \"frame\": {\"x\":0,\"y\":0,\"w\":8,\"h\":8} },\n"
                       "    \"walk_1\": { \"frame\": {\"x\":8,\"y\":0,\"w\":8,\"h\":8} }\n"
                       "  }\n"
                       "}";
    RogueTexture tex;
    make_dummy_tex(&tex, 32, 16);
    RogueSprite frames[8];
    int n = rogue_skill_build_packed_frames_from_json_text(&tex, json, frames, 8);
    assert(n == 2);
    assert(frames[0].sx == 0 && frames[0].sy == 0 && frames[0].sw == 8 && frames[0].sh == 8);
    assert(frames[1].sx == 8 && frames[1].sy == 0 && frames[1].sw == 8 && frames[1].sh == 8);
}

int main(void)
{
    test_parse_array_frames();
    test_parse_object_frames();
    printf("ok\n");
    return 0;
}
