#include "ui_context.h"
#include <stdlib.h>
#include <string.h>

static double ui_perf_now(const RogueUIContext* ctx)
{
    if (ctx && ctx->perf_now)
        return ctx->perf_now(ctx->perf_now_user);
    return ctx ? ctx->time_ms : 0.0;
}

RogueUIDirtyInfo rogue_ui_dirty_info(const RogueUIContext* ctx)
{
    RogueUIDirtyInfo di = {0};
    if (!ctx)
        return di;
    di.changed = ctx->dirty_changed;
    di.x = ctx->dirty_x;
    di.y = ctx->dirty_y;
    di.w = ctx->dirty_w;
    di.h = ctx->dirty_h;
    di.changed_node_count = ctx->dirty_node_count;
    di.kind = ctx->dirty_kind;
    return di;
}
void rogue_ui_perf_set_budget(RogueUIContext* ctx, double frame_budget_ms)
{
    if (ctx)
        ctx->perf_budget_ms = frame_budget_ms;
}
int rogue_ui_perf_frame_over_budget(const RogueUIContext* ctx)
{
    return (ctx && ctx->perf_last_frame_ms > ctx->perf_budget_ms && ctx->perf_budget_ms > 0) ? 1
                                                                                             : 0;
}
double rogue_ui_perf_last_update_ms(const RogueUIContext* ctx)
{
    return ctx ? ctx->perf_last_update_ms : 0.0;
}
double rogue_ui_perf_last_render_ms(const RogueUIContext* ctx)
{
    return ctx ? ctx->perf_last_render_ms : 0.0;
}
void rogue_ui_perf_set_time_provider(RogueUIContext* ctx, double (*now_ms_fn)(void*), void* user)
{
    if (!ctx)
        return;
    ctx->perf_now = now_ms_fn;
    ctx->perf_now_user = user;
}

void rogue_ui_render(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    double render_start = ui_perf_now(ctx);
    int node_delta = ctx->node_count - ctx->prev_node_count;
    int diff = rogue_ui_diff_changed(ctx);
    if (node_delta != 0 || (diff && !ctx->dirty_reported_this_frame))
    {
        ctx->dirty_changed = 1;
        ctx->dirty_node_count = ctx->node_count;
        ctx->dirty_kind = node_delta != 0 ? 1 : 2;
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (int i = 0; i < ctx->node_count; i++)
        {
            RogueUIRect r = ctx->nodes[i].rect;
            if (r.x < minx)
                minx = r.x;
            if (r.y < miny)
                miny = r.y;
            if (r.x + r.w > maxx)
                maxx = r.x + r.w;
            if (r.y + r.h > maxy)
                maxy = r.y + r.h;
        }
        if (ctx->node_count > 0)
        {
            ctx->dirty_x = minx;
            ctx->dirty_y = miny;
            ctx->dirty_w = maxx - minx;
            ctx->dirty_h = maxy - miny;
        }
        ctx->dirty_reported_this_frame = 1;
    }
    else
    {
        ctx->dirty_changed = 0;
        ctx->dirty_kind = 0;
    }
    ctx->prev_node_count = ctx->node_count;
    double render_end = ui_perf_now(ctx);
    ctx->perf_last_render_ms = render_end - render_start;
    double frame_end = render_end;
    ctx->perf_last_frame_ms = frame_end - ctx->perf_frame_start_ms;
    ctx->perf_last_update_ms = ctx->perf_last_frame_ms - ctx->perf_last_render_ms;
    ctx->perf_phase_accum[1] += ctx->perf_last_render_ms;
    if (ctx->perf_baseline_ms > 0 && ctx->perf_regress_threshold_pct > 0)
    {
        double allowed = ctx->perf_baseline_ms * (1.0 + ctx->perf_regress_threshold_pct);
        if (ctx->perf_last_frame_ms > allowed)
            ctx->perf_regressed_flag = 1;
    }
}

void rogue_ui_perf_phase_begin(RogueUIContext* ctx, int phase_id)
{
    if (!ctx || phase_id < 0 || phase_id > 7)
        return;
    ctx->perf_phase_start[phase_id] = ui_perf_now(ctx);
}
void rogue_ui_perf_phase_end(RogueUIContext* ctx, int phase_id)
{
    if (!ctx || phase_id < 0 || phase_id > 7)
        return;
    double now = ui_perf_now(ctx);
    double start = ctx->perf_phase_start[phase_id];
    if (start > 0 && now >= start)
        ctx->perf_phase_accum[phase_id] += (now - start);
    ctx->perf_phase_start[phase_id] = 0;
}
double rogue_ui_perf_phase_ms(const RogueUIContext* ctx, int phase_id)
{
    if (!ctx || phase_id < 0 || phase_id > 7)
        return 0.0;
    return ctx->perf_phase_accum[phase_id];
}
void rogue_ui_perf_set_baseline(RogueUIContext* ctx, double baseline_ms)
{
    if (ctx)
    {
        ctx->perf_baseline_ms = baseline_ms;
        ctx->perf_regressed_flag = 0;
    }
}
void rogue_ui_perf_set_regression_threshold(RogueUIContext* ctx, double pct_over_baseline)
{
    if (ctx)
        ctx->perf_regress_threshold_pct = pct_over_baseline;
}
int rogue_ui_perf_regressed(const RogueUIContext* ctx)
{
    return ctx ? ctx->perf_regressed_flag : 0;
}
void rogue_ui_perf_auto_baseline_reset(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    ctx->perf_autob_count = 0;
    ctx->perf_baseline_ms = 0;
    ctx->perf_regressed_flag = 0;
}
void rogue_ui_perf_auto_baseline_add_sample(RogueUIContext* ctx, double frame_ms, int target_count)
{
    if (!ctx || target_count <= 0)
        return;
    if (ctx->perf_autob_count <
        (int) (sizeof(ctx->perf_autob_samples) / sizeof(ctx->perf_autob_samples[0])))
        ctx->perf_autob_samples[ctx->perf_autob_count++] = frame_ms;
    if (ctx->perf_autob_count >= target_count)
    {
        double sum = 0;
        for (int i = 0; i < ctx->perf_autob_count; i++)
            sum += ctx->perf_autob_samples[i];
        ctx->perf_baseline_ms = sum / (double) ctx->perf_autob_count;
        ctx->perf_autob_count = 0;
        ctx->perf_regressed_flag = 0;
    }
}

/* Glyph cache */
void rogue_ui_text_cache_reset(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    free(ctx->glyph_cache);
    ctx->glyph_cache = NULL;
    ctx->glyph_cache_capacity = 0;
    ctx->glyph_cache_count = 0;
    ctx->glyph_cache_hits = ctx->glyph_cache_misses = 0;
    ctx->glyph_cache_tick = 1;
}
static float glyph_synth_advance(unsigned int cp) { return 6.0f + (float) (cp % 5); }
static int glyph_cache_find(RogueUIContext* ctx, unsigned int cp)
{
    for (int i = 0; i < ctx->glyph_cache_count; i++)
    {
        if (ctx->glyph_cache[i].codepoint == cp)
        {
            ctx->glyph_cache[i].lru_tick = ++ctx->glyph_cache_tick;
            return i;
        }
    }
    return -1;
}
static int glyph_cache_insert(RogueUIContext* ctx, unsigned int cp, float adv)
{
    if (ctx->glyph_cache_count >= ctx->glyph_cache_capacity)
    {
        int new_cap = ctx->glyph_cache_capacity ? ctx->glyph_cache_capacity * 2 : 64;
        ctx->glyph_cache = (struct RogueUIGlyphEntry*) realloc(
            ctx->glyph_cache, (size_t) new_cap * sizeof(*ctx->glyph_cache));
        ctx->glyph_cache_capacity = new_cap;
    }
    int idx = ctx->glyph_cache_count++;
    ctx->glyph_cache[idx].codepoint = cp;
    ctx->glyph_cache[idx].advance = adv;
    ctx->glyph_cache[idx].lru_tick = ++ctx->glyph_cache_tick;
    return idx;
}
float rogue_ui_text_cache_measure(RogueUIContext* ctx, const char* text)
{
    if (!ctx || !text)
        return 0;
    float w = 0;
    for (const unsigned char* p = (const unsigned char*) text; *p; ++p)
    {
        unsigned int cp = *p;
        int idx = glyph_cache_find(ctx, cp);
        if (idx >= 0)
        {
            w += ctx->glyph_cache[idx].advance;
            ctx->glyph_cache_hits++;
        }
        else
        {
            float adv = glyph_synth_advance(cp);
            glyph_cache_insert(ctx, cp, adv);
            ctx->glyph_cache_misses++;
            w += adv;
        }
    }
    return w;
}
int rogue_ui_text_cache_hits(const RogueUIContext* ctx) { return ctx ? ctx->glyph_cache_hits : 0; }
int rogue_ui_text_cache_misses(const RogueUIContext* ctx)
{
    return ctx ? ctx->glyph_cache_misses : 0;
}
int rogue_ui_text_cache_size(const RogueUIContext* ctx) { return ctx ? ctx->glyph_cache_count : 0; }
void rogue_ui_text_cache_compact(RogueUIContext* ctx)
{
    if (!ctx || ctx->glyph_cache_count < 2)
        return;
    int limit = ctx->glyph_cache_count / 2;
    unsigned int* ticks =
        (unsigned int*) malloc((size_t) ctx->glyph_cache_count * sizeof(unsigned int));
    for (int i = 0; i < ctx->glyph_cache_count; i++)
        ticks[i] = ctx->glyph_cache[i].lru_tick;
    for (int i = 0; i < limit; i++)
    {
        int max_i = i;
        for (int j = i + 1; j < ctx->glyph_cache_count; j++)
            if (ticks[j] > ticks[max_i])
                max_i = j;
        unsigned int tmp = ticks[i];
        ticks[i] = ticks[max_i];
        ticks[max_i] = tmp;
    }
    unsigned int cutoff = ticks[limit - 1];
    free(ticks);
    int write = 0;
    for (int i = 0; i < ctx->glyph_cache_count; i++)
    {
        if (ctx->glyph_cache[i].lru_tick >= cutoff)
        {
            if (write != i)
                ctx->glyph_cache[write] = ctx->glyph_cache[i];
            write++;
        }
    }
    ctx->glyph_cache_count = write;
    if (ctx->glyph_cache_capacity > 128 && ctx->glyph_cache_count < ctx->glyph_cache_capacity / 4)
    {
        int new_cap = ctx->glyph_cache_capacity / 2;
        if (new_cap < 64)
            new_cap = 64;
        ctx->glyph_cache = (struct RogueUIGlyphEntry*) realloc(
            ctx->glyph_cache, (size_t) new_cap * sizeof(*ctx->glyph_cache));
        ctx->glyph_cache_capacity = new_cap;
    }
}
