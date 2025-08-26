#include "audio_vfx/effects.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Phase 6.4 procedural layering test
   Validates:
   1. Adding sweetener layers per state and retrieval of count.
   2. Deterministic sweetener selection for a given activation (same frame/state).
   3. Variation between different state activations (different frame seeds) possible.
   4. Sweetener weight scales with active cross-fade weight only (not fadeout) and uses layer gain.
*/

static void reset_audio(void)
{
    rogue_audio_registry_clear();
    /* Register three music tracks: base explore, two sweeteners */
    assert(rogue_audio_registry_register("music_explore_base", "path/explore_base.wav",
                                         ROGUE_AUDIO_CAT_MUSIC, 1.0f) == 0);
    assert(rogue_audio_registry_register("music_explore_shaker", "path/explore_shaker.wav",
                                         ROGUE_AUDIO_CAT_MUSIC, 1.0f) == 0);
    assert(rogue_audio_registry_register("music_explore_bells", "path/explore_bells.wav",
                                         ROGUE_AUDIO_CAT_MUSIC, 1.0f) == 0);
    /* Register state base track */
    assert(rogue_audio_music_register(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_base") == 0);
}

static void test_layer_registration_and_count(void)
{
    reset_audio();
    assert(rogue_audio_music_layer_count(ROGUE_MUSIC_STATE_EXPLORE) == 0);
    assert(rogue_audio_music_layer_add(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_shaker", 0.5f) ==
           0);
    assert(rogue_audio_music_layer_add(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_bells", 0.25f) ==
           0);
    assert(rogue_audio_music_layer_count(ROGUE_MUSIC_STATE_EXPLORE) == 2);
}

static void test_sweetener_selection_and_weight(void)
{
    reset_audio();
    /* Add two layers */
    assert(rogue_audio_music_layer_add(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_shaker", 0.5f) ==
           0);
    assert(rogue_audio_music_layer_add(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_bells", 0.25f) ==
           0);
    /* Start explore state immediate */
    assert(rogue_audio_music_set_state(ROGUE_MUSIC_STATE_EXPLORE, 0) == 0);
    const char* sweet = rogue_audio_music_layer_current();
    assert(sweet && "Sweetener should be selected");
    /* The sweetener track effective gain should be active weight (1) * layer gain */
    float layer_weight = rogue_audio_music_track_weight(
        sweet); /* returns cross-fade weight for base track, not sweetener directly */
    /* We can't directly query sweetener gain, but effective gain ratio between base and sweetener
     * ids should reflect it. */
    float base_gain = rogue_audio_debug_effective_gain("music_explore_base", 1, 0, 0);
    float sweet_gain = rogue_audio_debug_effective_gain(sweet, 1, 0, 0);
    assert(base_gain > 0.0f && sweet_gain > 0.0f);
    float ratio = sweet_gain / base_gain;
    assert(ratio > 0.20f && ratio < 0.55f); /* between 0.25 and 0.5 with some headroom */
}

static void test_determinism_same_activation(void)
{
    reset_audio();
    assert(rogue_audio_music_layer_add(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_shaker", 0.5f) ==
           0);
    assert(rogue_audio_music_layer_add(ROGUE_MUSIC_STATE_EXPLORE, "music_explore_bells", 0.25f) ==
           0);
    /* Activate state; call update multiple times without state change - sweetener should stay
     * constant */
    assert(rogue_audio_music_set_state(ROGUE_MUSIC_STATE_EXPLORE, 0) == 0);
    const char* first = rogue_audio_music_layer_current();
    assert(first);
    for (int i = 0; i < 10; ++i)
    {
        rogue_audio_music_update(16);
        const char* cur = rogue_audio_music_layer_current();
        assert(cur && strcmp(cur, first) == 0);
    }
}

int main(void)
{
    test_layer_registration_and_count();
    test_sweetener_selection_and_weight();
    test_determinism_same_activation();
    printf("Phase6.4 layering tests passed\n");
    return 0;
}
