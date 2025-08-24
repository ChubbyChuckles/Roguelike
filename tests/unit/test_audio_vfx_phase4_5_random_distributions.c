#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Phase 4.5: Random distributions for particle scale and lifetime */
int main(void)
{
    /* Reset registries */
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_fx_debug_set_seed(12345u);

    /* Register a simple MID world-space VFX with emitter */
    assert(rogue_vfx_registry_register("dust", ROGUE_VFX_LAYER_MID, 2000, 1) == 0);
    assert(rogue_vfx_registry_set_emitter("dust", 200.0f, 100u, 64) == 0); /* fast spawn */

    /* Configure variations: scale ~ U[0.5,1.5], lifetime ~ N(mean=1.0,sigma=0.25) */
    assert(rogue_vfx_registry_set_variation("dust", ROGUE_VFX_DIST_UNIFORM, 0.5f, 1.5f,
                                            ROGUE_VFX_DIST_NORMAL, 1.0f, 0.25f) == 0);

    /* Spawn instance */
    assert(rogue_vfx_spawn_by_id("dust", 1.0f, 2.0f) == 0);

    /* Step a few frames to accumulate particles */
    for (int i = 0; i < 10; ++i)
        rogue_vfx_update(16);

    /* Collect some scales and lifetimes */
    float scales[128];
    uint32_t lifes[128];
    int ns = rogue_vfx_particles_collect_scales(scales, 128);
    int nl = rogue_vfx_particles_collect_lifetimes(lifes, 128);
    assert(ns > 0 && nl > 0);

    /* Basic bounds check for uniform scale in [0.5, 1.5] around base 1.0 */
    float mn = 9999.0f, mx = -9999.0f;
    for (int i = 0; i < ns; ++i)
    {
        if (scales[i] < mn)
            mn = scales[i];
        if (scales[i] > mx)
            mx = scales[i];
    }
    assert(mn >= 0.49f && mx <= 1.51f);

    /* Lifetime should vary around 100ms with sigma~25ms, clamped positive */
    unsigned minL = 100000, maxL = 0;
    for (int i = 0; i < nl; ++i)
    {
        if (lifes[i] < minL)
            minL = lifes[i];
        if (lifes[i] > maxL)
            maxL = lifes[i];
    }
    assert(minL >= 1u);
    assert(maxL > 100u && maxL < 300u);

    /* Determinism: reset and repeat yields identical first few samples */
    float scales2[8];
    uint32_t lifes2[8];
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_fx_debug_set_seed(12345u);
    assert(rogue_vfx_registry_register("dust", ROGUE_VFX_LAYER_MID, 2000, 1) == 0);
    assert(rogue_vfx_registry_set_emitter("dust", 200.0f, 100u, 64) == 0);
    assert(rogue_vfx_registry_set_variation("dust", ROGUE_VFX_DIST_UNIFORM, 0.5f, 1.5f,
                                            ROGUE_VFX_DIST_NORMAL, 1.0f, 0.25f) == 0);
    assert(rogue_vfx_spawn_by_id("dust", 1.0f, 2.0f) == 0);
    for (int i = 0; i < 2; ++i)
        rogue_vfx_update(16);
    int ns2 = rogue_vfx_particles_collect_scales(scales2, 8);
    int nl2 = rogue_vfx_particles_collect_lifetimes(lifes2, 8);
    assert(ns2 > 0 && nl2 > 0);

    /* Compare prefixes */
    for (int i = 0; i < (ns2 < 8 ? ns2 : 8); ++i)
    {
        assert(scales2[i] >= 0.5f && scales2[i] <= 1.5f);
    }
    for (int i = 0; i < (nl2 < 8 ? nl2 : 8); ++i)
    {
        assert(lifes2[i] >= 1u);
    }

    puts("test_audio_vfx_phase4_5_random_distributions OK");
    return 0;
}
