/* Unit: asset classification heuristics */
#include "../../src/util/asset_classification.h"
#include <assert.h>
#include <string.h>

static void test_basic_images()
{
    assert(rogue_asset_classify("assets/graphics/sprites/characters/player/player_idle.png") ==
           ROGUE_ASSET_GRAPHICS_SPRITE);
    assert(rogue_asset_classify("assets/graphics/textures/backgrounds/forest.png") ==
           ROGUE_ASSET_GRAPHICS_TEXTURE);
}

static void test_audio()
{
    assert(rogue_asset_classify("assets/audio/music/ambient/loop01.ogg") ==
           ROGUE_ASSET_AUDIO_MUSIC);
    assert(rogue_asset_classify("assets/audio/sfx/ui/click.wav") == ROGUE_ASSET_AUDIO_SFX);
    assert(rogue_asset_classify("assets/audio/voice/dialogue/intro.mp3") ==
           ROGUE_ASSET_AUDIO_VOICE);
}

static void test_json_cfg()
{
    assert(rogue_asset_classify("assets/data/levels/dungeons/level1.json") ==
           ROGUE_ASSET_DATA_LEVEL);
    assert(rogue_asset_classify("assets/data/localization/en/ui.json") ==
           ROGUE_ASSET_DATA_LOCALIZATION);
    assert(rogue_asset_classify("assets/data/schemas/sprite.schema.json") ==
           ROGUE_ASSET_META_SCHEMA);
    assert(rogue_asset_classify("assets/data/configs/gameplay/balance.json") ==
           ROGUE_ASSET_DATA_CONFIG);
}

static void test_misc()
{
    assert(rogue_asset_classify("assets/shaders/vertex/basic.vert") == ROGUE_ASSET_SHADER_VERTEX);
    assert(rogue_asset_classify("assets/shaders/fragment/lighting.frag") ==
           ROGUE_ASSET_SHADER_FRAGMENT);
    assert(rogue_asset_classify("assets/shaders/compute/particles.comp") ==
           ROGUE_ASSET_SHADER_COMPUTE);
    assert(rogue_asset_classify("assets/fonts/ui/opensans.ttf") == ROGUE_ASSET_GRAPHICS_FONT);
    assert(rogue_asset_classify("README.md") == ROGUE_ASSET_UNKNOWN);
}

int main(void)
{
    test_basic_images();
    test_audio();
    test_json_cfg();
    test_misc();
    return 0; /* success */
}
