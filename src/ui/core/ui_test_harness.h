/* Phase 10 Test & QA Automation Harness Helpers
   Provides lightweight, deterministic facilities for:
   - Golden diff style structural comparison (10.2)
   - Layout property fuzzing (10.4)
   - Performance smoke build (10.5) */
#pragma once
#include "ui_context.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueUIDrawSample
    {
        float x, y, w, h;
        uint32_t color;
        int kind;
    } RogueUIDrawSample;

    /* Capture up to max samples from current node list (panel/text/etc.). Returns count captured.
     */
    size_t rogue_ui_draw_capture(const RogueUIContext* ctx, RogueUIDrawSample* out, size_t max);
    /* Compute diff (# of differing samples or structural mismatches) between baseline capture and
     * current ctx. */
    int rogue_ui_golden_diff(const RogueUIContext* ctx, const RogueUIDrawSample* baseline,
                             size_t baseline_count, int* out_changed);
    /* Convenience predicate: returns 1 if diff <= tolerance. */
    int rogue_ui_golden_within_tolerance(const RogueUIContext* ctx,
                                         const RogueUIDrawSample* baseline, size_t baseline_count,
                                         int tolerance, int* out_changed);

    /* Layout fuzz: randomly generate row/column child placement inside a root; returns violations
     * (0 == pass). */
    int rogue_ui_layout_fuzz(int iterations);

    /* Build many simple panels (count) into a fresh context frame (caller supplies ctx already
     * begun). Returns nodes emitted. */
    int rogue_ui_perf_build_many(RogueUIContext* ctx, int count);

#ifdef __cplusplus
}
#endif
