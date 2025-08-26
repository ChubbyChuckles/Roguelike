/* Phase 6.2: Beat-aligned music transition test */
#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <string.h>

static int feq(float a, float b) { return (a > b ? a - b : b - a) < 1e-4f; }

int main(void)
{
    rogue_audio_registry_clear();
    rogue_audio_mixer_set_master(1.0f);
    for (int i = 0; i < 4; ++i)
        rogue_audio_mixer_set_category((RogueAudioCategory) i, 1.0f);
    rogue_audio_mixer_set_mute(0);

    assert(rogue_audio_registry_register("music_explore", "assets/sfx/explore.ogg",
                                         ROGUE_AUDIO_CAT_MUSIC, 1.0f) == 0);
    assert(rogue_audio_registry_register("music_combat", "assets/sfx/combat.ogg",
                                         ROGUE_AUDIO_CAT_MUSIC, 1.0f) == 0);

    assert(rogue_audio_music_register(ROGUE_MUSIC_STATE_EXPLORE, "music_explore") == 0);
    assert(rogue_audio_music_register(ROGUE_MUSIC_STATE_COMBAT, "music_combat") == 0);

    /* Set tempo: 120 BPM, 4/4 => beat=500ms, bar=2000ms */
    rogue_audio_music_set_tempo(120.0f, 4);

    /* Start explore immediately */
    assert(rogue_audio_music_set_state(ROGUE_MUSIC_STATE_EXPLORE, 0) == 0);
    assert(strcmp(rogue_audio_music_current(), "music_explore") == 0);

    /* Schedule combat on next bar with 1000ms cross-fade */
    assert(rogue_audio_music_set_state_on_next_bar(ROGUE_MUSIC_STATE_COMBAT, 1000) == 0);

    /* Advance 1500ms (within first bar, before boundary) -> still explore only */
    rogue_audio_music_update(1500);
    assert(strcmp(rogue_audio_music_current(), "music_explore") == 0);
    assert(feq(rogue_audio_music_track_weight("music_explore"), 1.0f));
    assert(feq(rogue_audio_music_track_weight("music_combat"), 0.0f));

    /* Advance another 600ms (crosses bar boundary at accumulated 2000ms).
        Of this 600ms, 500ms complete the first bar (no fade yet), and 100ms occur AFTER the
        bar boundary starting the cross-fade (1000ms duration). Thus we expect ~10% progression
        into the fade: explore ~=0.9, combat ~=0.1. */
    rogue_audio_music_update(600);                                  /* total sim time: 2100ms */
    float w_exp0 = rogue_audio_music_track_weight("music_explore"); /* fading out */
    float w_com0 = rogue_audio_music_track_weight("music_combat");  /* fading in  */
    assert(w_exp0 > 0.85f && w_exp0 < 0.95f);
    assert(w_com0 > 0.05f && w_com0 < 0.15f);
    assert(feq(w_exp0 + w_com0, 1.0f));

    /* Advance an additional 400ms so total fade time = 100ms + 400ms = 500ms (midpoint). */
    rogue_audio_music_update(400);
    float w_exp_mid = rogue_audio_music_track_weight("music_explore");
    float w_com_mid = rogue_audio_music_track_weight("music_combat");
    assert(w_com_mid > 0.45f && w_com_mid < 0.55f); /* ~50% into fade */
    assert(feq(w_exp_mid + w_com_mid, 1.0f));

    /* Finish remaining 500ms (total 1000ms) */
    rogue_audio_music_update(500);
    assert(feq(rogue_audio_music_track_weight("music_combat"), 1.0f));
    assert(feq(rogue_audio_music_track_weight("music_explore"), 0.0f));
    assert(strcmp(rogue_audio_music_current(), "music_combat") == 0);

    return 0;
}
