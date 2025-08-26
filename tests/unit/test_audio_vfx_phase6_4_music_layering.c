/* Phase 6.4: Procedural music layering selection & gain contribution */
#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <string.h>

static int feq(float a, float b) { return (a > b ? a - b : b - a) < 1e-4f; }

int main(void)
{
    rogue_audio_registry_clear();
    rogue_audio_mixer_set_mute(0);
    rogue_audio_mixer_set_master(1.0f);
    for (int i = 0; i < 4; ++i)
        rogue_audio_mixer_set_category((RogueAudioCategory) i, 1.0f);

    /* Register base explore track and three possible sweetener layers */
    assert(rogue_audio_registry_register("music_explore_base", "assets/sfx/music_explore_base.ogg",
                                         ROGUE_AUDIO_CAT_MUSIC, 1.0f) == 0);
    assert(rogue_audio_registry_register("music_explore_layer_a",
                                         "assets/sfx/music_explore_layer_a.ogg",
                                         ROGUE_AUDIO_CAT_MUSIC, 0.6f) == 0);
    assert(rogue_audio_registry_register("music_explore_layer_b",
                                         "assets/sfx/music_explore_layer_b.ogg",
                                         ROGUE_AUDIO_CAT_MUSIC, 0.65f) == 0);
    assert(rogue_audio_registry_register("music_explore_layer_c",
                                         "assets/sfx/music_explore_layer_c.ogg",
                                         ROGUE_AUDIO_CAT_MUSIC, 0.7f) == 0);

    /* Register base track for state and add layers */
    assert(rogue_audio_music_register(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_base") == 0);
    assert(rogue_audio_music_layer_add(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_layer_a", 0.5f) ==
           0);
    assert(rogue_audio_music_layer_add(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_layer_b", 0.5f) ==
           0);
    assert(rogue_audio_music_layer_add(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_layer_c", 0.5f) ==
           0);
    assert(rogue_audio_music_layer_count(ROGUE_MUSIC_STATE_EXPLORE) == 3);

    /* Activate state (immediate switch, deterministic choose one sweetener) */
    assert(rogue_audio_music_set_state(ROGUE_MUSIC_STATE_EXPLORE, 0) == 0);

    const char* sweet = rogue_audio_music_layer_current();
    assert(sweet == NULL || strlen(sweet) > 0);

    float base_gain = rogue_audio_debug_effective_gain("music_explore_base", 1, 0, 0);
    assert(feq(base_gain, 1.0f)); /* base fully audible */

    if (sweet)
    {
        float sweet_gain = rogue_audio_debug_effective_gain(sweet, 1, 0, 0);
        /* Sweetener contributes scaled version; since base_gain==1 and sweetener track base_gain <1
         * plus layer gain 0.5, expect >0 but <=0.5 (cap) */
        assert(sweet_gain > 0.0f && sweet_gain <= 0.5f + 1e-4f);
    }

    /* Change frame seed and re-apply state to force possible different deterministic pick */
    rogue_fx_frame_begin(1234);
    rogue_audio_music_set_state(ROGUE_MUSIC_STATE_EXPLORE, 0); /* reapply same state */
    const char* sweet2 = rogue_audio_music_layer_current();
    assert(sweet2 == NULL || strlen(sweet2) > 0);

    return 0;
}
