/* Prevent SDL from redefining main on Windows test binaries */
#define SDL_MAIN_HANDLED 1
#include "../../src/audio_vfx/effects.h"
#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/game/buffs.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("audio_vfx_test_bus");
    if (!rogue_event_bus_init(&cfg))
        return 3;
    rogue_buffs_init();
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_fx_map_clear();

    /* Register cue VFX */
    rogue_vfx_registry_register("buff_fx", ROGUE_VFX_LAYER_UI, 120, 0);
    rogue_vfx_registry_set_emitter("buff_fx", 50.0f, 80, 12);

    /* Map gain/expire for strength buff (enum value 1) */
    rogue_fx_map_register("buff/1/gain", ROGUE_FX_MAP_VFX, "buff_fx", ROGUE_FX_PRI_UI);
    rogue_fx_map_register("buff/1/expire", ROGUE_FX_MAP_VFX, "buff_fx", ROGUE_FX_PRI_UI);

    /* Begin frame and apply buff */
    rogue_fx_frame_begin(1);
    /* Apply with longer duration to allow particle emission */
    int applied =
        rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH, 5, 200.0, 0.0, ROGUE_BUFF_STACK_ADD, 0);
    (void) applied;
    rogue_fx_frame_end();
    int p1 = rogue_fx_dispatch_process();
    for (int i = 0; i < 8; ++i)
        rogue_vfx_update(16);

    int v1 = rogue_vfx_active_count();
    int pr1 = rogue_vfx_particles_active_count();

    /* Expire by advancing time */
    rogue_fx_frame_begin(2);
    rogue_buffs_update(220.0);
    rogue_fx_frame_end();
    int p2 = rogue_fx_dispatch_process();
    for (int i = 0; i < 8; ++i)
        rogue_vfx_update(16);

    int v2 = rogue_vfx_active_count();
    int pr2 = rogue_vfx_particles_active_count();

    int ok1 = (v1 > 0) || (pr1 > 0);
    int ok2 = (v2 > 0) || (pr2 > 0);
    if (p1 <= 0 || p2 <= 0 || !ok1 || !ok2)
    {
        fprintf(stderr, "p1=%d p2=%d v1=%d v2=%d pr1=%d pr2=%d\n", p1, p2, v1, v2, pr1, pr2);
        return 2;
    }

    rogue_event_bus_shutdown();
    puts("test_audio_vfx_phase5_4_buff_triggers OK");
    return 0;
}
