/* Phase 6.1-6.4: Music state machine, cross-fade, and ducking tests */
#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <string.h>

static int feq(float a, float b) { return (a > b ? a - b : b - a) < 1e-4f; }

int main(void)
{
    /* Reset mixer */
    rogue_audio_registry_clear();
    rogue_audio_mixer_set_master(1.0f);
    for (int i = 0; i < 4; ++i)
        rogue_audio_mixer_set_category((RogueAudioCategory) i, 1.0f);
    rogue_audio_mixer_set_mute(0);

    /* Register two music tracks + one SFX */
    assert(rogue_audio_registry_register("music_explore", "assets/sfx/explore.ogg",
                                         ROGUE_AUDIO_CAT_MUSIC, 1.0f) == 0);
    assert(rogue_audio_registry_register("music_combat", "assets/sfx/combat.ogg",
                                         ROGUE_AUDIO_CAT_MUSIC, 1.0f) == 0);
    assert(rogue_audio_registry_register("hit", "assets/sfx/hit.wav", ROGUE_AUDIO_CAT_SFX, 0.5f) ==
           0);

    /* Register music states */
    assert(rogue_audio_music_register(ROGUE_MUSIC_STATE_EXPLORE, "music_explore") == 0);
    assert(rogue_audio_music_register(ROGUE_MUSIC_STATE_COMBAT, "music_combat") == 0);

    /* Start in explore immediately */
    assert(rogue_audio_music_set_state(ROGUE_MUSIC_STATE_EXPLORE, 0) == 0);
    assert(strcmp(rogue_audio_music_current(), "music_explore") == 0);
    assert(feq(rogue_audio_music_track_weight("music_explore"), 1.0f));

    /* Cross-fade to combat over 1000 ms */
    assert(rogue_audio_music_set_state(ROGUE_MUSIC_STATE_COMBAT, 1000) == 0);
    /* At t=0: explore fading out weight=1, combat=0 */
    assert(feq(rogue_audio_music_track_weight("music_combat"), 0.0f));
    assert(feq(rogue_audio_music_track_weight("music_explore"), 1.0f));

    /* Advance 500 ms -> weights approx 0.5 */
    rogue_audio_music_update(500);
    float w_explore_mid = rogue_audio_music_track_weight("music_explore");
    float w_combat_mid = rogue_audio_music_track_weight("music_combat");
    assert(feq(w_explore_mid + w_combat_mid, 1.0f));
    assert(w_combat_mid > 0.45f && w_combat_mid < 0.55f);

    /* Advance remaining 500 ms -> combat fully active */
    rogue_audio_music_update(500);
    assert(feq(rogue_audio_music_track_weight("music_combat"), 1.0f));
    assert(feq(rogue_audio_music_track_weight("music_explore"), 0.0f));
    assert(strcmp(rogue_audio_music_current(), "music_combat") == 0);

    /* Duck music: attack 200ms to 0.2 gain, hold 300ms, release 200ms */
    rogue_audio_duck_music(0.2f, 200, 300, 200);
    /* Immediately after call with non-zero attack, gain should start near 1 (attack phase) */
    float g0 = rogue_audio_debug_effective_gain("music_combat", 1, 0, 0);
    assert(g0 > 0.8f);
    /* After 200ms (end attack) -> near target */
    rogue_audio_music_update(200);
    float g_attack_end = rogue_audio_debug_effective_gain("music_combat", 1, 0, 0);
    assert(g_attack_end < 0.25f && g_attack_end > 0.15f);
    /* During hold (another 200ms) stays near target */
    rogue_audio_music_update(200);
    float g_hold_mid = rogue_audio_debug_effective_gain("music_combat", 1, 0, 0);
    assert(g_hold_mid < 0.25f && g_hold_mid > 0.15f);
    /* Advance past remaining hold 100ms + release 200ms total 300ms */
    rogue_audio_music_update(300);
    float g_release_end = rogue_audio_debug_effective_gain("music_combat", 1, 0, 0);
    assert(feq(g_release_end, 1.0f));

    /* Ensure SFX unaffected by ducking */
    float g_sfx = rogue_audio_debug_effective_gain("hit", 1, 0, 0);
    assert(feq(g_sfx, 0.5f));

    return 0;
}
