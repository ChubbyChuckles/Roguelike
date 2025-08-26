/* Phase 6: Music ducking via category mixer */
#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <stdio.h>

static int feq(float a, float b) { return (a > b ? a - b : b - a) < 1e-4f; }

int main(void)
{
    /* Clean slate */
    rogue_audio_registry_clear();
    rogue_audio_mixer_set_mute(0);
    rogue_audio_mixer_set_master(1.0f);
    for (int i = 0; i < 4; ++i)
        rogue_audio_mixer_set_category((RogueAudioCategory) i, 1.0f);

    /* Register a music track and an sfx event */
    assert(rogue_audio_registry_register("bgm", "assets/sfx/bgm.ogg", ROGUE_AUDIO_CAT_MUSIC,
                                         1.0f) == 0);
    assert(rogue_audio_registry_register("hit", "assets/sfx/hit.wav", ROGUE_AUDIO_CAT_SFX, 0.5f) ==
           0);

    /* Initialize music state machine so music weight is explicitly 1.0 for 'bgm'. */
    assert(rogue_audio_music_register(ROGUE_MUSIC_STATE_EXPLORE, "bgm") == 0);
    assert(rogue_audio_music_set_state(ROGUE_MUSIC_STATE_EXPLORE, 0) == 0);
    /* Baseline: no ducking, category gains = 1.0 */
    float g_bgm = rogue_audio_debug_effective_gain("bgm", 1, 0.0f, 0.0f);
    float g_hit = rogue_audio_debug_effective_gain("hit", 1, 0.0f, 0.0f);
    printf("DBG after state init bgm=%f hit=%f\n", g_bgm, g_hit);
    assert(feq(g_bgm, 1.0f));
    assert(feq(g_hit, 0.5f));

    /* Apply ducking by reducing MUSIC category gain; SFX should remain unchanged */
    rogue_audio_mixer_set_category(ROGUE_AUDIO_CAT_MUSIC, 0.2f);
    float g_bgm_duck = rogue_audio_debug_effective_gain("bgm", 1, 0.0f, 0.0f);
    float g_hit_duck = rogue_audio_debug_effective_gain("hit", 1, 0.0f, 0.0f);
    assert(feq(g_bgm_duck, 0.2f));
    assert(feq(g_hit_duck, 0.5f));

    /* Restore */
    rogue_audio_mixer_set_category(ROGUE_AUDIO_CAT_MUSIC, 1.0f);
    assert(feq(rogue_audio_debug_effective_gain("bgm", 1, 0.0f, 0.0f), 1.0f));

    return 0;
}
