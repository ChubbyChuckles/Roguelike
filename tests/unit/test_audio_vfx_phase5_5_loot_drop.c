/* Prevent SDL from redefining main on Windows test binaries */
#define SDL_MAIN_HANDLED 1
#include "../../src/audio_vfx/effects.h"
#include <math.h>
#include <stdio.h>

static int nearly_equal(float a, float b) { return fabsf(a - b) < 0.01f; }

int main(void)
{
    /* Clean FX/VFX state */
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_fx_map_clear();

    /* Register a world-space VFX for loot drops */
    rogue_vfx_registry_register("loot_fx", ROGUE_VFX_LAYER_MID, 120, 1);
    rogue_vfx_registry_set_emitter("loot_fx", 40.0f, 60, 12);

    /* Map a rarity-tier loot drop cue (e.g., rarity=3) */
    rogue_fx_map_register("loot/3/drop", ROGUE_FX_MAP_VFX, "loot_fx", ROGUE_FX_PRI_COMBAT);

    /* Simulate a drop at a specific world position */
    const float drop_x = 4.0f, drop_y = 5.0f;
    rogue_fx_frame_begin(1);
    int enq = rogue_fx_trigger_event("loot/3/drop", drop_x, drop_y);
    (void) enq;
    rogue_fx_frame_end();
    int processed = rogue_fx_dispatch_process();

    /* Advance VFX a few ticks to emit particles */
    for (int i = 0; i < 4; ++i)
        rogue_vfx_update(16);

    int active = rogue_vfx_active_count();
    int parts = rogue_vfx_particles_active_count();

    int ws = 0;
    float x = -1.0f, y = -1.0f;
    int peek = rogue_vfx_debug_peek_first("loot_fx", &ws, &x, &y);

    int ok_visible = (active > 0) || (parts > 0);
    int ok_pos = (peek == 0) && (ws == 1) && nearly_equal(x, drop_x) && nearly_equal(y, drop_y);

    if (processed <= 0 || !ok_visible || !ok_pos)
    {
        fprintf(stderr, "proc=%d active=%d parts=%d peek=%d ws=%d pos=(%.2f,%.2f)\n", processed,
                active, parts, peek, ws, x, y);
        return 2;
    }

    puts("test_audio_vfx_phase5_5_loot_drop OK");
    return 0;
}
