/* Phase 4.4: Parameter overrides (lifetime, scale, color) */
#include "../../src/audio_vfx/effects.h"
#include <assert.h>

static void step(uint32_t ms) { rogue_vfx_update(ms); }

int main(void)
{
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();

    /* Define a VFX with emitter so we can inspect particles */
    assert(rogue_vfx_registry_register("spark", ROGUE_VFX_LAYER_FG, 200, 1) == 0);
    assert(rogue_vfx_registry_set_emitter("spark", 100.0f, 50, 10) == 0);

    /* Spawn without overrides -> default behavior */
    assert(rogue_vfx_spawn_by_id("spark", 0.0f, 0.0f) == 0);
    step(10);
    {
        float scales[16];
        uint32_t colors[16];
        int ns = rogue_vfx_particles_collect_scales(scales, 16);
        int nc = rogue_vfx_particles_collect_colors(colors, 16);
        assert(ns > 0 && nc == ns);
        for (int i = 0; i < ns; ++i)
        {
            assert(scales[i] == 1.0f);
            assert(colors[i] == 0xFFFFFFFFu);
        }
    }

    /* Spawn with overrides: shorter lifetime, larger scale, tinted color */
    RogueVfxOverrides ov = {0};
    ov.lifetime_ms = 50; /* shorter than registry 200 */
    ov.scale = 2.0f;
    ov.color_rgba = 0x80FF0000u; /* semi-red */
    assert(rogue_vfx_spawn_with_overrides("spark", 1.0f, 2.0f, &ov) == 0);

    /* Shortly after spawn, particles exist (from either instance). */
    step(25);
    assert(rogue_vfx_particles_active_count() > 0);

    /* After total 60ms since override spawn, overridden instance should be gone, default remains */
    step(35);
    int active = rogue_vfx_active_count();
    assert(active == 1);

    return 0;
}
