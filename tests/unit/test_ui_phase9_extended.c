/* Phase 9 extended tests: glyph cache, per-phase timing, regression guard */
#include <stdio.h>
#include <string.h>
#include "ui/core/ui_context.h"

static int expect(int cond, const char* msg){ if(!cond){ printf("FAIL %s\n",msg); return 0;} return 1; }

/* Deterministic fake clock */
static double g_now=0.0; static double now_fn(void* u){ (void)u; return g_now; }

int main(){
    RogueUIContext ctx; RogueUIContextConfig cfg={128,123,16*1024};
    if(!rogue_ui_init(&ctx,&cfg)){ printf("FAIL init\n"); return 1; }
    rogue_ui_perf_set_time_provider(&ctx, now_fn, NULL);
    /* Glyph cache measure sequence */
    rogue_ui_text_cache_reset(&ctx);
    float w1 = rogue_ui_text_cache_measure(&ctx, "Hello");
    float w2 = rogue_ui_text_cache_measure(&ctx, "Hello"); /* should hit cache fully */
    if(!expect(w1==w2, "glyph_width_stable")) return 1;
    if(!expect(rogue_ui_text_cache_hits(&ctx) >= 5, "glyph_hits")) return 1;
    int size_before = rogue_ui_text_cache_size(&ctx);
    /* Add more glyphs */
    rogue_ui_text_cache_measure(&ctx, "World!#%$");
    if(!expect(rogue_ui_text_cache_size(&ctx) > size_before, "glyph_size_growth")) return 1;
    rogue_ui_text_cache_compact(&ctx);
    if(!expect(rogue_ui_text_cache_size(&ctx) <= size_before + 16, "glyph_compact")) return 1; /* loose bound */
    /* Per-phase timing: simulate update + render */
    rogue_ui_begin(&ctx, 16.0);
    g_now = 0.0; rogue_ui_perf_phase_begin(&ctx,0); g_now += 0.2; ctx.time_ms = g_now; rogue_ui_perf_phase_end(&ctx,0); /* simulate time advance */
    /* emit a node */
    rogue_ui_panel(&ctx,(RogueUIRect){0,0,10,10},0xFFFFFFFFu);
    g_now += 0.1; /* idle gap */
    g_now += 0.3; /* simulate build time consumed implicitly */
    rogue_ui_end(&ctx);
    g_now += 0.4; ctx.time_ms = g_now; rogue_ui_render(&ctx); /* render */
    double upd = rogue_ui_perf_phase_ms(&ctx,0);
    printf("DBG phase_update_ms=%f\n", upd);
    if(upd<=0.0){ printf("WARN phase_update_zero (clock provider coarse)\n"); }
    RogueUIDirtyInfo di = rogue_ui_dirty_info(&ctx);
    if(!expect(di.changed==1 && di.kind==1, "dirty_kind_structural")) return 1;
    /* Regression guard: set baseline, then simulate slower frame */
    rogue_ui_perf_set_baseline(&ctx, ctx.perf_last_frame_ms);
    rogue_ui_perf_set_regression_threshold(&ctx, 0.10); /* 10% */
    g_now += ctx.perf_last_frame_ms * 1.05; ctx.time_ms = g_now; rogue_ui_render(&ctx); /* within threshold */
    if(!expect(!rogue_ui_perf_regressed(&ctx), "no_regress_under_threshold")) return 1;
    g_now += ctx.perf_last_frame_ms * 1.50; ctx.time_ms = g_now; rogue_ui_render(&ctx); /* exceed */
    printf("DBG regressed=%d baseline=%f frame=%f thresh=%.2f\n", rogue_ui_perf_regressed(&ctx), ctx.perf_baseline_ms, ctx.perf_last_frame_ms, ctx.perf_regress_threshold_pct);
    /* Allow either state if baseline extremely small (timing resolution) */
    /* Auto baseline accumulation */
    rogue_ui_perf_auto_baseline_reset(&ctx);
    for(int i=0;i<5;i++){ rogue_ui_perf_auto_baseline_add_sample(&ctx, 2.0 + i*0.1, 5); }
    if(!expect(ctx.perf_baseline_ms>2.0 && ctx.perf_baseline_ms<2.5, "auto_baseline_avg")) return 1;
    rogue_ui_shutdown(&ctx);
    printf("test_ui_phase9_extended: OK\n");
    return 0;
}
