#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Phase 5.1: Gameplay event -> effect mapping */
int main(void)
{
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_fx_map_clear();

    /* Register audio and vfx ids */
    assert(rogue_audio_registry_register("SND_HIT", "assets/sfx/hit.wav", ROGUE_AUDIO_CAT_SFX,
                                         0.8f) == 0);
    assert(rogue_vfx_registry_register("FX_SPARK", ROGUE_VFX_LAYER_FG, 500, 1) == 0);

    /* Wire emitter so particles can be observed */
    assert(rogue_vfx_registry_set_emitter("FX_SPARK", 20.0f, 100u, 8) == 0);

    /* Map single gameplay event key to both audio and vfx */
    assert(rogue_fx_map_register("gameplay/hit/light", ROGUE_FX_MAP_AUDIO, "SND_HIT",
                                 ROGUE_FX_PRI_COMBAT) == 0);
    assert(rogue_fx_map_register("gameplay/hit/light", ROGUE_FX_MAP_VFX, "FX_SPARK",
                                 ROGUE_FX_PRI_COMBAT) == 0);

    /* Begin a new FX frame and trigger */
    rogue_fx_frame_begin(1);
    int enq = rogue_fx_trigger_event("gameplay/hit/light", 3.0f, 4.0f);
    assert(enq == 2);
    rogue_fx_frame_end();

    /* Process dispatch: should spawn a VFX instance */
    int n = rogue_fx_dispatch_process();
    assert(n == 2);

    /* Update a bit so particles spawn */
    for (int i = 0; i < 10; ++i)
        rogue_vfx_update(16);

    /* Validate active VFX and particles present */
    assert(rogue_vfx_active_count() >= 1);
    assert(rogue_vfx_particles_active_count() > 0);

    puts("test_audio_vfx_phase5_1_mapping OK");
    return 0;
}
