#include "../../src/audio_vfx/effects.h"
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
    int ok = 1;
    /* Trails */
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    ok &= assert_true(rogue_vfx_registry_register("fx_trail", ROGUE_VFX_LAYER_MID, 2000, 1) == 0,
                      "reg trail vfx");
    /* 50 Hz, 1000ms lifetime -> ~50 trails alive after 1s */
    ok &= assert_true(rogue_vfx_registry_set_trail("fx_trail", 50.0f, 1000, 200) == 0, "set trail");
    ok &= assert_true(rogue_vfx_spawn_by_id("fx_trail", 1.0f, 2.0f) == 0, "spawn trail inst");
    rogue_vfx_set_perf_scale(1.0f);
    for (int i = 0; i < 10; ++i)
        rogue_vfx_update(100); /* 1s -> ~50 trails alive (<= cap) */
    int trails = rogue_vfx_particles_trail_count();
    int totalp = rogue_vfx_particles_active_count();
    printf("debug: trails=%d total_particles=%d\n", trails, totalp);
    ok &= assert_true(trails >= 30 && trails <= 200, "trail count in expected range");

    /* Post-processing stubs */
    rogue_vfx_post_set_bloom_enabled(1);
    ok &= assert_true(rogue_vfx_post_get_bloom_enabled() == 1, "bloom enabled");
    rogue_vfx_post_set_bloom_params(0.8f, 1.2f);
    float th = 0, inten = 0;
    rogue_vfx_post_get_bloom_params(&th, &inten);
    ok &= assert_true(th == 0.8f && inten == 1.2f, "bloom params");
    rogue_vfx_post_set_color_lut("warm", 0.6f);
    char lut[24];
    float ls = 0;
    int has = rogue_vfx_post_get_color_lut(lut, sizeof lut, &ls);
    ok &= assert_true(has == 1 && ls > 0.5f && strncmp(lut, "warm", 4) == 0, "lut set");
    rogue_vfx_post_set_color_lut(NULL, 0.0f);
    has = rogue_vfx_post_get_color_lut(lut, sizeof lut, &ls);
    ok &= assert_true(has == 0 && ls == 0.0f, "lut cleared");

    /* Decals */
    rogue_vfx_decal_registry_clear();
    ok &= assert_true(
        rogue_vfx_decal_registry_register("blood", ROGUE_VFX_LAYER_BG, 500, 1, 1.0f) == 0,
        "decal reg");
    ok &= assert_true(rogue_vfx_decal_spawn("blood", 3.0f, 4.0f, 0.0f, 1.0f) == 0, "spawn decal");
    ok &= assert_true(rogue_vfx_decal_active_count() == 1, "one decal active");
    /* Age beyond lifetime to ensure expiry */
    for (int i = 0; i < 6; ++i)
        rogue_vfx_update(100);
    ok &= assert_true(rogue_vfx_decal_active_count() == 0, "decal expired");

    printf("Phase7.3/7.5/7.7 tests %s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}
