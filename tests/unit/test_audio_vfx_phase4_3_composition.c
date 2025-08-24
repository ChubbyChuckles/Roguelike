/* Phase 4.3: Effect composition (chain & parallel) */
#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <string.h>

static void step_ms(unsigned ms)
{
    /* Advance VFX by ms in chunks to simulate frames. */
    rogue_vfx_update(ms);
}

int main(void)
{
    /* Define two leaf effects and one composite that chains them with delays. */
    rogue_vfx_registry_clear();

    assert(rogue_vfx_registry_register("leafA", ROGUE_VFX_LAYER_MID, 100, 1) == 0);
    assert(rogue_vfx_registry_set_emitter("leafA", 0.0f, 0, 0) == 0);

    assert(rogue_vfx_registry_register("leafB", ROGUE_VFX_LAYER_MID, 100, 1) == 0);
    assert(rogue_vfx_registry_set_emitter("leafB", 0.0f, 0, 0) == 0);

    const char* chain_children[] = {"leafA", "leafB"};
    const uint32_t chain_delays[] = {0u, 50u}; /* B after 50 ms from A */
    assert(rogue_vfx_registry_define_composite("combo_chain", ROGUE_VFX_LAYER_MID, 200, 1,
                                               chain_children, chain_delays, 2, 1) == 0);

    /* Also define a parallel composite that spawns both at t=0 and t=50. */
    const char* par_children[] = {"leafA", "leafB"};
    const uint32_t par_delays[] = {0u, 50u};
    assert(rogue_vfx_registry_define_composite("combo_parallel", ROGUE_VFX_LAYER_MID, 200, 1,
                                               par_children, par_delays, 2, 0) == 0);

    /* Spawn CHAIN at (10,20) */
    assert(rogue_vfx_spawn_by_id("combo_chain", 10.0f, 20.0f) == 0);
    assert(rogue_vfx_active_count() == 1); /* only composite active initially */

    step_ms(0);
    /* After time 0, leafA should spawn immediately */
    assert(rogue_vfx_active_count() == 2);

    step_ms(49);
    /* Not yet spawned B */
    assert(rogue_vfx_active_count() == 2);

    step_ms(1);
    /* At 50ms, leafB spawns */
    assert(rogue_vfx_active_count() == 3);

    /* Let them expire */
    step_ms(200);
    /* After composite lifetime, composite should be gone; leafs should also be gone */
    assert(rogue_vfx_active_count() == 0);

    /* Spawn PARALLEL at a different spot */
    assert(rogue_vfx_spawn_by_id("combo_parallel", 5.0f, 6.0f) == 0);
    step_ms(0);
    /* immediate spawn of first child only (A), second at 50ms */
    assert(rogue_vfx_active_count() == 2);
    step_ms(50);
    assert(rogue_vfx_active_count() == 3);
    step_ms(200);
    assert(rogue_vfx_active_count() == 0);

    return 0;
}
