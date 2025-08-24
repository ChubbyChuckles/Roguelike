#include "../../src/audio_vfx/effects.h"
#include <stdio.h>
#include <string.h>

/* Phase 3.4: Validate particle layer ordering helper returns BG->MID->FG->UI for active layers. */
int main(void)
{
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();

    /* Register four VFX, each on a distinct layer, with particle emitters. */
    rogue_vfx_registry_register("fx_bg", ROGUE_VFX_LAYER_BG, 1000, 1);
    rogue_vfx_registry_register("fx_mid", ROGUE_VFX_LAYER_MID, 1000, 1);
    rogue_vfx_registry_register("fx_fg", ROGUE_VFX_LAYER_FG, 1000, 1);
    rogue_vfx_registry_register("fx_ui", ROGUE_VFX_LAYER_UI, 1000, 0);

    /* Emitters: 20 Hz, particles 200ms lifetime, cap 4 each. */
    rogue_vfx_registry_set_emitter("fx_bg", 20.0f, 200, 4);
    rogue_vfx_registry_set_emitter("fx_mid", 20.0f, 200, 4);
    rogue_vfx_registry_set_emitter("fx_fg", 20.0f, 200, 4);
    rogue_vfx_registry_set_emitter("fx_ui", 20.0f, 200, 4);

    /* Spawn across layers. */
    rogue_vfx_spawn_by_id("fx_mid", 0, 0);
    rogue_vfx_spawn_by_id("fx_fg", 0, 0);
    rogue_vfx_spawn_by_id("fx_bg", 0, 0);

    /* Advance 100ms -> ensure particles present for the spawned layers. */
    rogue_vfx_update(100);

    uint8_t layers[4] = {255, 255, 255, 255};
    int n = rogue_vfx_particles_collect_ordered(layers, 4);
    if (n < 1)
    {
        fprintf(stderr, "No layers collected\n");
        return 1;
    }

    /* Expect BG then MID then FG; UI not present as no spawn for UI. */
    if (n != 3)
    {
        fprintf(stderr, "Expected 3 layers, got %d\n", n);
        return 1;
    }
    if (layers[0] != (uint8_t) ROGUE_VFX_LAYER_BG)
    {
        fprintf(stderr, "First layer not BG\n");
        return 1;
    }
    if (layers[1] != (uint8_t) ROGUE_VFX_LAYER_MID)
    {
        fprintf(stderr, "Second layer not MID\n");
        return 1;
    }
    if (layers[2] != (uint8_t) ROGUE_VFX_LAYER_FG)
    {
        fprintf(stderr, "Third layer not FG\n");
        return 1;
    }

    /* Now spawn UI and advance to get particles, expect 4 layers in order. */
    rogue_vfx_spawn_by_id("fx_ui", 0, 0);
    rogue_vfx_update(100);
    memset(layers, 255, sizeof layers);
    n = rogue_vfx_particles_collect_ordered(layers, 4);
    if (n != 4)
    {
        fprintf(stderr, "Expected 4 layers after UI, got %d\n", n);
        return 1;
    }
    if (layers[0] != (uint8_t) ROGUE_VFX_LAYER_BG)
        return 1;
    if (layers[1] != (uint8_t) ROGUE_VFX_LAYER_MID)
        return 1;
    if (layers[2] != (uint8_t) ROGUE_VFX_LAYER_FG)
        return 1;
    if (layers[3] != (uint8_t) ROGUE_VFX_LAYER_UI)
        return 1;

    printf("VFX_P3_4_LAYER_ORDER_OK\n");
    return 0;
}
