#include "../../src/audio_vfx/effects.h"
#include "../../src/core/audio_vfx/audiovfx_debug.h"
#include <assert.h>

int main(void)
{
    /* Ensure registries are empty, then register minimal entries */
    rogue_audio_registry_clear();
    rogue_vfx_registry_clear();
    assert(rogue_audio_registry_register("click", "assets/sfx/click.wav", ROGUE_AUDIO_CAT_UI,
                                         0.8f) == 0);
    assert(rogue_vfx_registry_register("SPARKLE", ROGUE_VFX_LAYER_UI, 100, 0) == 0);

    /* Validate references */
    assert(rogue_audiovfx_debug_validate_audio("click") == 0);
    assert(rogue_audiovfx_debug_validate_vfx("SPARKLE") == 0);

    /* Play sound and spawn VFX via debug wrappers */
    assert(rogue_audiovfx_debug_play("click") == 0);
    assert(rogue_audiovfx_debug_spawn_at("SPARKLE", 10.0f, 20.0f) == 0);

    /* Perf controls do not crash */
    rogue_audiovfx_debug_set_master(0.7f);
    rogue_audiovfx_debug_set_category(1, 0.6f);
    rogue_audiovfx_debug_set_mute(0);
    rogue_audiovfx_debug_set_perf(1.0f);
    rogue_audiovfx_debug_set_budgets(0, 0);

    /* Stats snapshot is callable */
    struct RogueVfxFrameStats st;
    rogue_audiovfx_debug_get_last_stats(&st);

    return 0;
}
