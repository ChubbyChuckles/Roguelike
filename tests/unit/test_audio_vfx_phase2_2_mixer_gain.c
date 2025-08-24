#include "audio_vfx/effects.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Arrange: registry with two categories */
    assert(rogue_audio_registry_register("ui_click", "assets/sfx/ui_click.wav", ROGUE_AUDIO_CAT_UI,
                                         0.5f) == 0);
    assert(rogue_audio_registry_register("hit_light", "assets/sfx/hit_light.wav",
                                         ROGUE_AUDIO_CAT_SFX, 0.8f) == 0);

    rogue_audio_mixer_set_master(0.5f);
    rogue_audio_mixer_set_category(ROGUE_AUDIO_CAT_UI, 0.5f);
    rogue_audio_mixer_set_category(ROGUE_AUDIO_CAT_SFX, 1.0f);

    float g1 = rogue_audio_debug_effective_gain("ui_click", 1, 0.0f, 0.0f);
    float g2 = rogue_audio_debug_effective_gain("hit_light", 2, 0.0f, 0.0f);

    /* ui_click: base 0.5 * master 0.5 * cat 0.5 = 0.125 */
    assert(g1 > 0.12f && g1 < 0.13f);

    /* hit_light with repeats=2 gets 0.8 * (0.7 + 0.3*2) -> 0.8 * 1.3 = 1.04 -> clipped to 1.0
       then master 0.5 * cat 1.0 => 0.5 */
    assert(g2 > 0.49f && g2 <= 0.5f + 1e-5f);

    printf("ok\n");
    return 0;
}
