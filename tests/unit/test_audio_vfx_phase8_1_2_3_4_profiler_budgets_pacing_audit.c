#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void step(unsigned dt_ms) { rogue_vfx_update(dt_ms); }

int main(void)
{
    /* Setup a simple VFX with steady emission */
    rogue_vfx_registry_clear();
    rogue_vfx_set_perf_scale(1.0f);
    /* Lifetime must exceed the multi-step window so pacing/budgets apply after baseline. */
    int rc = rogue_vfx_registry_register("spark", ROGUE_VFX_LAYER_MID, 4000, 1);
    assert(rc == 0);
    rc = rogue_vfx_registry_set_emitter("spark", 100.0f, 1500, 1000);
    assert(rc == 0);
    rc = rogue_vfx_registry_set_trail("spark", 50.0f, 1200, 1000);
    assert(rc == 0);
    rc = rogue_vfx_spawn_by_id("spark", 0.0f, 0.0f);
    assert(rc == 0);

    /* No budgets: expect spawns to accrue (100 core + 50 trail) per 1s at full scale */
    step(1000);
    RogueVfxFrameStats st = {0};
    rogue_vfx_profiler_get_last(&st);
    assert(st.spawned_core >= 99 && st.spawned_core <= 101);
    assert(st.spawned_trail >= 49 && st.spawned_trail <= 51);
    int baseline_active = st.active_particles;
    assert(baseline_active > 0);

    /* Apply pacing guard to 60 per frame; expect culled_pacing and capped total spawns */
    rogue_vfx_set_pacing_guard(1, 60);
    step(1000);
    rogue_vfx_profiler_get_last(&st);
    printf("DBG after pacing: core=%d trail=%d total=%d culled_pacing=%d culled_soft=%d "
           "culled_hard=%d\n",
           st.spawned_core, st.spawned_trail, st.spawned_core + st.spawned_trail, st.culled_pacing,
           st.culled_soft, st.culled_hard);
    assert(st.spawned_core + st.spawned_trail <= 60);
    assert(st.culled_pacing > 0);

    /* Apply soft budget 30 and hard budget 40; soft should limit before hard kicks in */
    rogue_vfx_set_spawn_budgets(30, 40);
    step(1000);
    rogue_vfx_profiler_get_last(&st);
    printf(
        "DBG after soft budget: core=%d trail=%d culled_soft=%d culled_hard=%d culled_pacing=%d\n",
        st.spawned_core, st.spawned_trail, st.culled_soft, st.culled_hard, st.culled_pacing);
    assert(st.spawned_core + st.spawned_trail <= 30);
    assert(st.culled_soft > 0);
    /* Now disable soft, leave hard=40; expect <=40 and culled_hard.
           Step only 500ms to stay under the 4000ms lifetime and ensure emissions occur. */
    rogue_vfx_set_spawn_budgets(0, 40);
    step(500);
    rogue_vfx_profiler_get_last(&st);
    printf("DBG after hard budget: core=%d trail=%d total=%d culled_hard=%d culled_soft=%d "
           "culled_pacing=%d\n",
           st.spawned_core, st.spawned_trail, st.spawned_core + st.spawned_trail, st.culled_hard,
           st.culled_soft, st.culled_pacing);
    assert(st.spawned_core + st.spawned_trail <= 40);
    assert(st.culled_hard > 0);

    /* Pool audit should return sane counts */
    int a = 0, f = 0, runs = 0, maxrun = 0;
    rogue_vfx_particle_pool_audit(&a, &f, &runs, &maxrun);
    assert(a == st.active_particles);
    assert(f >= 0 && runs >= 1 && maxrun >= 1);
    rogue_vfx_instance_pool_audit(&a, &f, &runs, &maxrun);
    assert(a == st.active_instances);

    /* Disable pacing and budgets; stats still retrievable */
    rogue_vfx_set_pacing_guard(0, 0);
    rogue_vfx_set_spawn_budgets(0, 0);
    step(16);
    rogue_vfx_profiler_get_last(&st);
    assert(st.active_particles >= 0);

    printf("ok\n");
    return 0;
}
