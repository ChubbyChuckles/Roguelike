#include "../../src/audio_vfx/effects.h"
#include <assert.h>

int main(void)
{
    rogue_audio_registry_clear();
    assert(rogue_audio_registry_register("rc_test", "assets/sfx/rc_test.wav", ROGUE_AUDIO_CAT_SFX,
                                         0.6f) == 0);

    /* Confirm presence via path lookup */
    char buf[128];
    assert(rogue_audio_registry_get_path("rc_test", buf, sizeof buf) == 0);

    /* Gain query non-zero */
    float g = rogue_audio_debug_effective_gain("rc_test", 1, 0.0f, 0.0f);
    assert(g > 0.59f);

    /* Clear then expect zero gain (id not found) */
    rogue_audio_registry_clear();
    float g2 = rogue_audio_debug_effective_gain("rc_test", 1, 0.0f, 0.0f);
    assert(g2 == 0.0f);

    /* Re-register works again */
    assert(rogue_audio_registry_register("rc_test", "assets/sfx/rc_test.wav", ROGUE_AUDIO_CAT_SFX,
                                         0.6f) == 0);
    float g3 = rogue_audio_debug_effective_gain("rc_test", 1, 0.0f, 0.0f);
    assert(g3 > 0.59f);
    return 0;
}
