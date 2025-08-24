/*
   Phase 3.6: Screen-space vs world-space coordinate support
   - Register two VFX: one world-space MID, one screen-space UI
   - Configure emitters and spawn instances at known positions
   - Tick update and collect screen-space particle positions
   - Assert that world-space particles are offset by camera & scaled, while screen-space
     particles are not affected by camera and appear at pixel coordinates directly.
*/

#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <string.h>

static void reset_all(void)
{
    rogue_audio_registry_clear();
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_vfx_set_timescale(1.0f);
    rogue_vfx_set_frozen(0);
}

int main(void)
{
    reset_all();

    /* Define world-space MID layer VFX with particle emitter */
    assert(rogue_vfx_registry_register("dust_world", ROGUE_VFX_LAYER_MID, 1000, 1) == 0);
    assert(rogue_vfx_registry_set_emitter("dust_world", 100.0f, 200, 8) == 0);

    /* Define screen-space UI layer VFX with particle emitter */
    assert(rogue_vfx_registry_register("spark_ui", ROGUE_VFX_LAYER_UI, 1000, 0) == 0);
    assert(rogue_vfx_registry_set_emitter("spark_ui", 100.0f, 200, 8) == 0);

    /* Spawn at distinct positions */
    assert(rogue_vfx_spawn_by_id("dust_world", 10.0f, 5.0f) == 0);
    assert(rogue_vfx_spawn_by_id("spark_ui", 200.0f, 100.0f) == 0);

    /* Set camera and scale */
    rogue_vfx_set_camera(8.0f, 4.0f, 32.0f); /* camera at (8,4) world units, 32 px per unit */

    /* Advance time to spawn some particles */
    rogue_vfx_update(50); /* 50 ms, expect a few particles at 100 Hz */

    float xy[64];
    uint8_t layers[32];
    int n = rogue_vfx_particles_collect_screen(xy, layers, 16);
    assert(n > 0);

    /* We expect at least one particle for each effect; check that we see both layers present */
    int saw_mid = 0, saw_ui = 0;
    for (int i = 0; i < n; ++i)
    {
        if (layers[i] == (uint8_t) ROGUE_VFX_LAYER_MID)
            saw_mid = 1;
        if (layers[i] == (uint8_t) ROGUE_VFX_LAYER_UI)
            saw_ui = 1;
    }
    assert(saw_mid && saw_ui);

    /* Validate transform math by locating one MID and one UI particle and checking positions. */
    int mid_idx = -1, ui_idx = -1;
    for (int i = 0; i < n; ++i)
    {
        if (mid_idx < 0 && layers[i] == (uint8_t) ROGUE_VFX_LAYER_MID)
            mid_idx = i;
        if (ui_idx < 0 && layers[i] == (uint8_t) ROGUE_VFX_LAYER_UI)
            ui_idx = i;
    }
    assert(mid_idx >= 0 && ui_idx >= 0);

    /* World-space particle spawned at (10,5) with camera (8,4) and 32 px/unit -> (64,32) */
    float mx = xy[mid_idx * 2 + 0];
    float my = xy[mid_idx * 2 + 1];
    assert((int) (mx + 0.5f) == 64);
    assert((int) (my + 0.5f) == 32);

    /* Screen-space particle at (200,100) should be unaffected by camera */
    float ux = xy[ui_idx * 2 + 0];
    float uy = xy[ui_idx * 2 + 1];
    assert((int) (ux + 0.5f) == 200);
    assert((int) (uy + 0.5f) == 100);

    return 0;
}
