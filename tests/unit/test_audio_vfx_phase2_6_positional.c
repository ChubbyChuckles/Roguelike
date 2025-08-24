#include "../../src/audio_vfx/effects.h"
#include <assert.h>

int main(void)
{
    rogue_audio_registry_clear();
    assert(rogue_audio_registry_register("pos_test", "assets/sfx/pos_test.wav", ROGUE_AUDIO_CAT_SFX,
                                         1.0f) == 0);

    /* No positional: attenuation = 1 */
    rogue_audio_enable_positional(0);
    rogue_audio_set_falloff_radius(10.0f);
    rogue_audio_set_listener(0.0f, 0.0f);
    float g0 = rogue_audio_debug_effective_gain("pos_test", 1, 9.0f, 0.0f);
    assert(g0 > 0.99f);

    /* Enable positional: at d=0: atten=1; at d=9 of r=10: atten=0.1 */
    rogue_audio_enable_positional(1);
    float g_center = rogue_audio_debug_effective_gain("pos_test", 1, 0.0f, 0.0f);
    float g_far = rogue_audio_debug_effective_gain("pos_test", 1, 9.0f, 0.0f);
    assert(g_center > g_far);
    assert(g_far > 0.09f && g_far < 0.11f);

    /* Outside radius: at d>=r -> 0 */
    float g_out = rogue_audio_debug_effective_gain("pos_test", 1, 10.0f, 0.0f);
    assert(g_out == 0.0f);

    /* Disable positional restores full gain */
    rogue_audio_enable_positional(0);
    float g_restore = rogue_audio_debug_effective_gain("pos_test", 1, 9.0f, 0.0f);
    assert(g_restore > 0.99f);
    return 0;
}
