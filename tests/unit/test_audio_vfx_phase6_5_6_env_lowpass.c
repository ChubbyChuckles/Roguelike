/* Phase 6.5/6.6: Reverb preset smoothing + distance low-pass attenuation */
#include "../../src/audio_vfx/effects.h"
#include <assert.h>

static int feq_tol(float a, float b, float tol) { return (a > b ? a - b : b - a) < tol; }

int main(void)
{
    rogue_audio_registry_clear();
    rogue_audio_mixer_set_master(1.0f);
    for (int i = 0; i < 4; ++i)
        rogue_audio_mixer_set_category((RogueAudioCategory) i, 1.0f);

    /* Register two SFX so we can test distance attenuation + low-pass factor relative scaling */
    assert(rogue_audio_registry_register("near_sfx", "assets/sfx/near.wav", ROGUE_AUDIO_CAT_SFX,
                                         1.0f) == 0);
    assert(rogue_audio_registry_register("far_sfx", "assets/sfx/far.wav", ROGUE_AUDIO_CAT_SFX,
                                         1.0f) == 0);

    /* Baseline positional off: both equal */
    float g_near0 = rogue_audio_debug_effective_gain("near_sfx", 1, 0, 0);
    float g_far0 = rogue_audio_debug_effective_gain("far_sfx", 1, 9, 0);
    assert(feq_tol(g_near0, g_far0, 1e-4f));

    /* Enable positional + low-pass and compare ratio */
    rogue_audio_enable_positional(1);
    rogue_audio_set_falloff_radius(10.0f);
    rogue_audio_enable_distance_lowpass(1);
    rogue_audio_set_lowpass_params(0.8f, 0.4f);
    float g_near_lp = rogue_audio_debug_effective_gain("near_sfx", 1, 0, 0); /* attenuation=1 */
    float g_far_lp = rogue_audio_debug_effective_gain("far_sfx", 1, 9, 0);   /* attenuation ~0.1 */
    assert(g_near_lp > g_far_lp);
    /* Disable low-pass should increase far/near ratio (less suppression of highs) */
    rogue_audio_enable_distance_lowpass(0);
    float g_far_no_lp = rogue_audio_debug_effective_gain("far_sfx", 1, 9, 0);
    assert(g_far_no_lp >= g_far_lp - 1e-4f);

    /* Reverb preset smoothing: set cave then tick updates until wet approaches target */
    assert(rogue_audio_env_get_reverb_preset() == ROGUE_AUDIO_REVERB_NONE);
    rogue_audio_env_set_reverb_preset(ROGUE_AUDIO_REVERB_CAVE);
    float initial_wet = rogue_audio_env_get_reverb_wet();
    for (int t = 0; t < 6; ++t)
    {
        rogue_audio_music_update(50); /* 6*50ms = 300ms > tau */
    }
    float wet_after = rogue_audio_env_get_reverb_wet();
    assert(wet_after > initial_wet);
    /* Change to NONE should start decreasing */
    rogue_audio_env_set_reverb_preset(ROGUE_AUDIO_REVERB_NONE);
    float before_down = rogue_audio_env_get_reverb_wet();
    for (int t = 0; t < 6; ++t)
        rogue_audio_music_update(50);
    float after_down = rogue_audio_env_get_reverb_wet();
    assert(after_down < before_down + 1e-4f);
    return 0;
}
