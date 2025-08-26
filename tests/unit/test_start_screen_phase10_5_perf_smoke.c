#include "../../src/core/app/app.h"
#include "../../src/core/app/app_state.h"
#include <assert.h>
#include <stdio.h>

/* Phase 10.5: Start Screen performance smoke test
   Purpose: Ensure the Start Screen's frame-time budget guard stays inactive under a generous
   threshold, and that a baseline is established quickly. We avoid asserting absolute timing
   to keep the test stable across machines/CI by setting a high budget and a small baseline
   sample window. */
int main(void)
{
    RogueAppConfig cfg = {
        "StartScreenPerfSmoke",    320, 180, 320, 180, 0, 0, 0, 1, ROGUE_WINDOW_WINDOWED,
        (RogueColor){0, 0, 0, 255}};
    assert(rogue_app_init(&cfg));

    /* Loosen budget and reduce baseline sample count so the test is robust and quick. */
    g_app.start_perf_budget_ms = 100.0;            /* very generous to avoid false positives */
    g_app.start_perf_target_samples = 3;           /* compute baseline quickly */
    g_app.start_perf_regress_threshold_pct = 10.0; /* allow wide swings over baseline (1000%) */
    /* Avoid prewarm work during this smoke test to reduce incidental variability. */
    g_app.start_prewarm_active = 0;
    g_app.start_prewarm_done = 1;
    g_app.start_perf_accum_ms = 0.0;
    g_app.start_perf_samples = 0;
    g_app.start_perf_baseline_ms = 0.0;
    g_app.start_perf_regressed = 0;
    g_app.start_perf_reduce_quality = 0;
    g_app.start_perf_warned = 0;

    /* Step several frames to establish baseline and exercise the guard. */
    for (int i = 0; i < 8; ++i)
        rogue_app_step();

    double baseline = g_app.start_perf_baseline_ms;
    int regressed = g_app.start_perf_regressed;
    int reduced = g_app.start_perf_reduce_quality;
    int warned = g_app.start_perf_warned;

    if (!(baseline > 0.0))
    {
        fprintf(stderr, "START_PERF_SMOKE baseline not established: %.3f\n", baseline);
        rogue_app_shutdown();
        return 1;
    }
    if (regressed || reduced || warned)
    {
        fprintf(stderr,
                "START_PERF_SMOKE unexpected guard trip: baseline=%.3f regressed=%d reduced=%d "
                "warned=%d\n",
                baseline, regressed, reduced, warned);
        rogue_app_shutdown();
        return 2;
    }

    printf("START_PERF_SMOKE_OK baseline=%.3f\n", baseline);
    rogue_app_shutdown();
    return 0;
}
