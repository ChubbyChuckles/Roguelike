#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void step(unsigned dt_ms) { rogue_vfx_update(dt_ms); }

int main(void)
{
    /* Phase 8.5: Stress test 100 simultaneous impacts under pacing/budgets */
    rogue_vfx_registry_clear();
    rogue_vfx_set_perf_scale(1.0f);
    rogue_vfx_set_pacing_guard(0, 0);
    rogue_vfx_set_spawn_budgets(0, 0);

    /* Register an impact VFX with modest per-instance emitter to keep totals sane */
    int rc = rogue_vfx_registry_register("impact", ROGUE_VFX_LAYER_MID, 1500, 1);
    assert(rc == 0);
    rc = rogue_vfx_registry_set_emitter("impact", 200.0f, 800, 512);
    assert(rc == 0);

    /* Spawn 100 instances in the same frame */
    for (int i = 0; i < 100; ++i)
        assert(rogue_vfx_spawn_by_id("impact", 0.0f, 0.0f) == 0);

    /* Baseline frame (no pacing/budgets): expect many spawns, no culls, and 100+ instances alive */
    step(16);
    RogueVfxFrameStats st = {0};
    rogue_vfx_profiler_get_last(&st);
    int total = st.spawned_core + st.spawned_trail;
    assert(st.active_instances >= 100);
    assert(total > 0);
    assert(st.culled_pacing == 0 && st.culled_soft == 0 && st.culled_hard == 0);

    /* Enable pacing guard: threshold 150 per frame should cap spawns and increment culled_pacing */
    rogue_vfx_set_pacing_guard(1, 150);
    step(16);
    rogue_vfx_profiler_get_last(&st);
    total = st.spawned_core + st.spawned_trail;
    assert(total <= 150);
    assert(st.culled_pacing > 0);

    /* Disable pacing; apply soft=200, hard=250: soft should cap first and increment culled_soft */
    rogue_vfx_set_pacing_guard(0, 0);
    rogue_vfx_set_spawn_budgets(200, 250);
    step(16);
    rogue_vfx_profiler_get_last(&st);
    total = st.spawned_core + st.spawned_trail;
    assert(total <= 200);
    assert(st.culled_soft > 0);

    /* Hard-only: soft=0, hard=100 should cap and increment culled_hard */
    rogue_vfx_set_spawn_budgets(0, 100);
    step(16);
    rogue_vfx_profiler_get_last(&st);
    total = st.spawned_core + st.spawned_trail;
    assert(total <= 100);
    assert(st.culled_hard > 0);

    /* Pools remain sane */
    int a = 0, f = 0, runs = 0, maxrun = 0;
    rogue_vfx_particle_pool_audit(&a, &f, &runs, &maxrun);
    assert(a == st.active_particles);
    assert(f >= 0 && runs >= 1 && maxrun >= 1);

    printf("ok\n");
    return 0;
}
