#include "../../src/audio_vfx/effects.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Register a simple sparkle VFX, 100ms lifetime, world-space */
    int r = rogue_vfx_registry_register("SPARKLE", ROGUE_VFX_LAYER_MID, 100, 1);
    if (r != 0)
    {
        printf("VFX_REG_FAIL\n");
        return 1;
    }

    /* Emit spawn and dispatch */
    RogueEffectEvent ev = {0};
    ev.type = ROGUE_FX_VFX_SPAWN;
    ev.priority = ROGUE_FX_PRI_UI;
    strncpy(ev.id, "SPARKLE", sizeof ev.id - 1);
    ev.x = 3.0f;
    ev.y = 4.0f;

    rogue_fx_frame_begin(1);
    if (rogue_fx_emit(&ev) != 0)
        return 2;
    rogue_fx_frame_end();
    if (rogue_fx_dispatch_process() <= 0)
        return 3;

    /* One active instance exists on MID layer */
    if (rogue_vfx_active_count() != 1)
        return 4;
    if (rogue_vfx_layer_active_count(ROGUE_VFX_LAYER_MID) != 1)
        return 5;

    /* Check debug peek of position/world flag */
    int ws = 0;
    float px = 0, py = 0;
    if (rogue_vfx_debug_peek_first("SPARKLE", &ws, &px, &py) != 0)
        return 6;
    if (ws != 1)
        return 7;
    if (px != 3.0f || py != 4.0f)
        return 8;

    /* Time scaling: set frozen and advance -> should not age out */
    rogue_vfx_set_frozen(1);
    rogue_vfx_update(200);
    if (rogue_vfx_active_count() != 1)
        return 9;

    /* Unfreeze, set timescale=2, advance 50ms -> effective 100ms, instance should end */
    rogue_vfx_set_frozen(0);
    rogue_vfx_set_timescale(2.0f);
    rogue_vfx_update(50);
    if (rogue_vfx_active_count() != 0)
        return 10;

    printf("VFX_P3_1_OK\n");
    return 0;
}
