#include "../../src/audio_vfx/effects.h"
#include <assert.h>

int main(void)
{
    /* Arrange */
    rogue_audio_registry_clear();
    assert(rogue_audio_registry_register("mute_test", "assets/sfx/mute_test.wav",
                                         ROGUE_AUDIO_CAT_UI, 0.9f) == 0);

    rogue_audio_mixer_set_master(1.0f);
    rogue_audio_mixer_set_category(ROGUE_AUDIO_CAT_UI, 1.0f);
    rogue_audio_mixer_set_mute(0);

    float g_before = rogue_audio_debug_effective_gain("mute_test", 1, 0.0f, 0.0f);
    assert(g_before > 0.0f);

    /* Act: mute should zero output */
    rogue_audio_mixer_set_mute(1);
    float g_muted = rogue_audio_debug_effective_gain("mute_test", 1, 0.0f, 0.0f);
    assert(g_muted == 0.0f);

    /* Unmute restores prior non-zero */
    rogue_audio_mixer_set_mute(0);
    float g_after = rogue_audio_debug_effective_gain("mute_test", 1, 0.0f, 0.0f);
    assert(g_after > 0.8f && g_after <= 1.0f);

    return 0;
}
