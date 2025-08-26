#include "../../src/audio_vfx/effects.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int assert_true(int cond, const char* msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

int main(void)
{
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_vfx_set_timescale(1.0f);
    rogue_vfx_set_frozen(0);
    int ok = 1;

    /* Blend mode registration */
    ok &= assert_true(rogue_vfx_registry_register("fx_blend", ROGUE_VFX_LAYER_MID, 1000, 0) == 0,
                      "register blend vfx");
    RogueVfxBlend bm = (RogueVfxBlend) 0xFF;
    ok &= assert_true(rogue_vfx_registry_get_blend("fx_blend", &bm) == 0, "get default blend");
    ok &= assert_true(bm == ROGUE_VFX_BLEND_ALPHA, "default blend alpha");
    ok &= assert_true(rogue_vfx_registry_set_blend("fx_blend", ROGUE_VFX_BLEND_ADD) == 0,
                      "set blend add");
    bm = (RogueVfxBlend) 0xFF;
    ok &=
        assert_true(rogue_vfx_registry_get_blend("fx_blend", &bm) == 0 && bm == ROGUE_VFX_BLEND_ADD,
                    "blend stored");

    /* Performance scaling: register emitter */
    ok &= assert_true(rogue_vfx_registry_register("fx_perf", ROGUE_VFX_LAYER_MID, 2000, 0) == 0,
                      "register perf vfx");
    ok &= assert_true(rogue_vfx_registry_set_emitter("fx_perf", 100.0f, 500, 1000) == 0,
                      "set emitter");
    ok &= assert_true(rogue_vfx_spawn_by_id("fx_perf", 0, 0) == 0, "spawn perf instance");
    rogue_vfx_set_perf_scale(1.0f);
    for (int i = 0; i < 10; ++i)
        rogue_vfx_update(100); /* 1s total -> steady-state ~50 active (rate * lifetime) */
    int full_particles = rogue_vfx_particles_active_count();
    ok &= assert_true(full_particles >= 50, "full scale particle count steady-state >=50");
    /* Reset active & particles by clearing registry and re-registering */
    rogue_vfx_clear_active();
    rogue_vfx_registry_clear();
    ok &= assert_true(rogue_vfx_registry_register("fx_perf", ROGUE_VFX_LAYER_MID, 2000, 0) == 0,
                      "re-register perf vfx");
    ok &= assert_true(rogue_vfx_registry_set_emitter("fx_perf", 100.0f, 500, 1000) == 0,
                      "re-set emitter");
    ok &= assert_true(rogue_vfx_spawn_by_id("fx_perf", 0, 0) == 0, "spawn perf instance again");
    rogue_vfx_set_perf_scale(0.4f);
    for (int i = 0; i < 10; ++i)
        rogue_vfx_update(100);
    int reduced_particles = rogue_vfx_particles_active_count();
    ok &= assert_true(reduced_particles > 10, "reduced > 10");
    ok &= assert_true(reduced_particles < full_particles, "reduced < full");

    /* Screen shake */
    int sh = rogue_vfx_shake_add(8.0f, 5.0f, 1000);
    ok &= assert_true(sh >= 0, "shake add");
    float prev_mag = 0.0f;
    int changes = 0;
    float ox = 0, oy = 0;
    for (int t = 0; t < 1200; t += 100)
    {
        rogue_vfx_update(100); /* also ages shakes */
        rogue_vfx_shake_get_offset(&ox, &oy);
        float mag = sqrtf(ox * ox + oy * oy);
        if (t == 0)
            prev_mag = mag;
        else if (fabsf(mag - prev_mag) > 0.01f)
        {
            changes++;
            prev_mag = mag;
        }
    }
    ok &= assert_true(changes > 3, "shake offset varied");
    rogue_vfx_shake_get_offset(&ox, &oy);
    ok &= assert_true(fabsf(ox) + fabsf(oy) < 8.0f, "shake decayed not huge");

    /* GPU batch stub flag */
    rogue_vfx_set_gpu_batch_enabled(1);
    ok &= assert_true(rogue_vfx_get_gpu_batch_enabled() == 1, "gpu batch on");
    rogue_vfx_set_gpu_batch_enabled(0);
    ok &= assert_true(rogue_vfx_get_gpu_batch_enabled() == 0, "gpu batch off");

    printf("Phase7 core tests %s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}
