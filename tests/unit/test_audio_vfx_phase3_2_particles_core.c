#include "../../src/audio_vfx/effects.h"
#include <stdio.h>

int main(void)
{
    /* Clean slate */
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_vfx_set_frozen(0);
    rogue_vfx_set_timescale(1.0f);

    /* Register SPARKLE with 1000ms lifetime, world-space on MID layer */
    if (rogue_vfx_registry_register("SPARKLE", ROGUE_VFX_LAYER_MID, 1000, 1) != 0)
        return 1;

    /* Configure particle emitter: 50 Hz, particles live 200ms, cap 10 per instance */
    if (rogue_vfx_registry_set_emitter("SPARKLE", 50.0f, 200u, 10) != 0)
        return 2;

    /* Spawn via direct API (dispatcher already covered in 3.1 test) */
    if (rogue_vfx_spawn_by_id("SPARKLE", 1.0f, 2.0f) != 0)
        return 3;

    /* After 100ms @ 50Hz -> expect 5 particles active */
    rogue_vfx_update(100);
    if (rogue_vfx_particles_active_count() != 5)
        return 4;
    if (rogue_vfx_particles_layer_count(ROGUE_VFX_LAYER_MID) != 5)
        return 5;

    /* After another 100ms -> cap reached (10), none expired yet */
    rogue_vfx_update(100);
    if (rogue_vfx_particles_active_count() != 10)
        return 6;

    /* After another 100ms -> older half expire (200ms lifetime), should drop to 5 */
    rogue_vfx_update(100);
    if (rogue_vfx_particles_active_count() != 5)
        return 7;

    /* Instance still active; particle count stabilizes around 5 with this spawn/expiry cadence */
    rogue_vfx_update(100);
    if (rogue_vfx_particles_active_count() != 5)
        return 8;

    printf("VFX_P3_2_OK\n");
    return 0;
}
