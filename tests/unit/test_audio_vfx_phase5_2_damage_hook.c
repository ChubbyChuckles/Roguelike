/* Prevent SDL from redefining main on Windows test binaries */
#define SDL_MAIN_HANDLED 1
#include "../../src/audio_vfx/effects.h"
#include "../../src/game/combat.h"
#include <stdio.h>
#include <string.h>

/* Phase 5.2: Verify that damage events trigger mapped FX via observer hook */
int main(void)
{
    /* Clear state */
    rogue_fx_map_clear();
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_damage_events_clear();

    /* Register a simple VFX for fire hit and crit via mapping */
    /* VFX def */
    rogue_vfx_registry_register("hit_spark", ROGUE_VFX_LAYER_MID, 120, 1);
    rogue_vfx_registry_set_emitter("hit_spark", 50.0f, 100, 8);

    /* Map damage/fire/hit and damage/fire/crit to spawn the same VFX */
    rogue_fx_map_register("damage/fire/hit", ROGUE_FX_MAP_VFX, "hit_spark", ROGUE_FX_PRI_COMBAT);
    rogue_fx_map_register("damage/fire/crit", ROGUE_FX_MAP_VFX, "hit_spark", ROGUE_FX_PRI_CRITICAL);

    /* Bind hook */
    if (rogue_fx_damage_hook_bind() < 0)
        return 1;

    /* Begin a frame */
    rogue_fx_frame_begin(1);

    /* Record a fire, crit hit */
    rogue_damage_event_record(10 /*attack_id*/, ROGUE_DMG_FIRE, 1 /*crit*/, 100, 80, 0, 0);

    /* End frame and dispatch */
    rogue_fx_frame_end();
    int processed = rogue_fx_dispatch_process();

    /* Step VFX to spawn particles */
    rogue_vfx_update(100);

    /* Expect that at least one VFX instance/particles exist due to two triggers (hit+crit) */
    int active_vfx = rogue_vfx_active_count();
    int particles = rogue_vfx_particles_active_count();

    if (processed <= 0 || active_vfx <= 0 || particles <= 0)
    {
        fprintf(stderr, "processed=%d active_vfx=%d particles=%d\n", processed, active_vfx,
                particles);
        return 2;
    }

    /* Unbind to be tidy */
    rogue_fx_damage_hook_unbind();
    return 0;
}
