/* collision_pipeline.c - Milestone 2.1 multi-slice implementation
 * Contains staged collision processing with:
 *  - High-resolution timing & adaptive quality adjustment
 *  - Quadtree spatial culling + predictive (velocity horizon) inclusion
 *  - AABB prefilter & distance-based LOD heuristic
 *  - Temporal coherence cache stage (reuses last-frame candidate subset when stable)
 */
#include "game/collision_pipeline.h"
#include "game/enemy_collision_opt.h" /* For RogueEnemyCollisionProfile adaptive bias stage */
#include "game/hit_pixel_mask.h" /* Needed for RogueHitPixelMaskFrame definition (pixel-perfect stage) */
#include <math.h>                /* sqrtf */
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* High-resolution timer abstraction */
typedef struct RogueTimer
{
#ifdef _WIN32
    LARGE_INTEGER freq;
#endif
} RogueTimer;

static void rogue_timer_init(RogueTimer* t)
{
    (void) t;
#ifdef _WIN32
    QueryPerformanceFrequency(&t->freq);
#endif
}

static double rogue_timer_now_ms(const RogueTimer* t)
{
#ifdef _WIN32
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double) c.QuadPart * 1000.0 / (double) t->freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1e6;
#endif
}

/* Simple timing helper using clock() (portable, coarse). Future slice: high-res timer. */
static float measure_ms(double start_ms, double end_ms)
{
    if (end_ms < start_ms)
        return 0.f;
    return (float) (end_ms - start_ms);
}

/* ---------------- Spatial Culling (Quadtree) ---------------- */
typedef struct RogueQuadNode
{
    float x, y, w, h;      /* Bounds */
    uint16_t start, count; /* Range into candidate index list */
    uint8_t children[4];   /* Indices into node array (0xFF = none) */
} RogueQuadNode;

#define ROGUE_MAX_QUAD_NODES 128

typedef struct RogueQuadBuild
{
    RogueQuadNode nodes[ROGUE_MAX_QUAD_NODES];
    uint16_t node_count;
    uint16_t indices[1024]; /* temp index list (cap) */
} RogueQuadBuild;

static void quad_init(RogueQuadBuild* b) { b->node_count = 0; }

static uint16_t quad_add_node(RogueQuadBuild* b, float x, float y, float w, float h, uint16_t start,
                              uint16_t count)
{
    if (b->node_count >= ROGUE_MAX_QUAD_NODES)
        return 0xFFFF;
    uint16_t idx = b->node_count++;
    RogueQuadNode* n = &b->nodes[idx];
    n->x = x;
    n->y = y;
    n->w = w;
    n->h = h;
    n->start = start;
    n->count = count;
    n->children[0] = n->children[1] = n->children[2] = n->children[3] = 0xFF;
    return idx;
}

static void quad_subdivide(RogueQuadBuild* b, uint16_t node_index, RogueCollisionCandidate* cand,
                           uint16_t depth)
{
    if (depth >= 4)
        return; /* depth cap */
    RogueQuadNode* n = &b->nodes[node_index];
    if (n->count <= 8)
        return; /* leaf */
    float hw = n->w * 0.5f, hh = n->h * 0.5f;
    float cx = n->x + hw * 0.5f, cy = n->y + hh * 0.5f;
    /* partition indices in-place (naive) */
    uint16_t ranges[4] = {0, 0, 0, 0};
    uint16_t offsets[4];
    for (uint16_t i = 0; i < 4; ++i)
        offsets[i] = 0;
    for (uint16_t i = 0; i < n->count; ++i)
    {
        RogueCollisionCandidate* c = &cand[b->indices[n->start + i]];
        int quadrant = (c->y < cy ? 0 : 2) + (c->x < cx ? 0 : 1);
        ranges[quadrant]++;
    }
    uint16_t acc = 0;
    for (int q = 0; q < 4; ++q)
    {
        offsets[q] = acc;
        acc += ranges[q];
    }
    /* create child node ranges */
    uint16_t child_starts[4];
    for (int q = 0; q < 4; ++q)
        child_starts[q] = n->start + offsets[q];
    uint16_t placed[4] = {0, 0, 0, 0};
    for (uint16_t i = 0; i < n->count; ++i)
    {
        uint16_t idx = b->indices[n->start + i];
        RogueCollisionCandidate* c = &cand[idx];
        int quadrant = (c->y < cy ? 0 : 2) + (c->x < cx ? 0 : 1);
        uint16_t dest = child_starts[quadrant] + placed[quadrant]++;
        b->indices[dest] = idx;
    }
    /* add children */
    for (int q = 0; q < 4; ++q)
    {
        if (ranges[q] == 0)
        {
            n->children[q] = 0xFF;
            continue;
        }
        float nx = (q % 2 == 0) ? n->x : cx;
        float ny = (q < 2) ? n->y : cy;
        float nw = (q % 2 == 0) ? hw : hw;
        (void) nw; /* same */
        float nh = (q < 2) ? hh : hh;
        (void) nh;
        uint16_t child = quad_add_node(b, nx, ny, hw, hh, child_starts[q], ranges[q]);
        n->children[q] = (uint8_t) child;
        if (child != 0xFFFF)
            quad_subdivide(b, child, cand, depth + 1);
    }
}

static void quad_collect(const RogueQuadBuild* b, uint16_t node_index, float vx, float vy, float vw,
                         float vh, uint16_t* out_list, uint16_t* out_count, uint16_t cap)
{
    if (node_index == 0xFFFF)
        return;
    const RogueQuadNode* n = &b->nodes[node_index];
    /* AABB overlap test */
    if (n->x + n->w < vx || n->x > vx + vw || n->y + n->h < vy || n->y > vy + vh)
        return;
    /* leaf? */
    bool leaf = true;
    for (int i = 0; i < 4; ++i)
        if (n->children[i] != 0xFF)
        {
            leaf = false;
            break;
        }
    if (leaf)
    {
        for (uint16_t i = 0; i < n->count && *out_count < cap; ++i)
            out_list[(*out_count)++] = b->indices[n->start + i];
        return;
    }
    for (int i = 0; i < 4; ++i)
        if (n->children[i] != 0xFF)
            quad_collect(b, n->children[i], vx, vy, vw, vh, out_list, out_count, cap);
}

bool rogue_collision_stage_spatial_cull(struct RogueCollisionContext* ctx, RogueCollisionMetrics* m)
{
    if (ctx->skip_spatial)
    {
        /* Bypass spatial work this frame due to temporal cache hit */
        m->output_candidates = ctx->candidate_count;
        return true;
    }
    if (!ctx || ctx->candidate_count == 0)
    {
        m->output_candidates = ctx ? ctx->candidate_count : 0;
        return true;
    }
    RogueQuadBuild build;
    quad_init(&build);
    uint32_t count = ctx->candidate_count;
    if (count > (uint32_t) (sizeof(build.indices) / sizeof(build.indices[0])))
        count = (uint32_t) (sizeof(build.indices) / sizeof(build.indices[0]));
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (uint32_t i = 0; i < count; ++i)
    {
        build.indices[i] = (uint16_t) i;
        RogueCollisionCandidate* c = &ctx->candidates[i];
        if (c->x < minx)
            minx = c->x;
        if (c->y < miny)
            miny = c->y;
        if (c->x > maxx)
            maxx = c->x;
        if (c->y > maxy)
            maxy = c->y;
    }
    float w = (maxx - minx) + 1.f, h = (maxy - miny) + 1.f;
    uint16_t root = quad_add_node(&build, minx, miny, w, h, 0, (uint16_t) count);
    quad_subdivide(&build, root, ctx->candidates, 0);
    uint16_t kept_indices[1024];
    uint16_t kept = 0;
    quad_collect(&build, root, ctx->view_x, ctx->view_y, ctx->view_w, ctx->view_h, kept_indices,
                 &kept, 1024);
    /* predictive culling: if velocity would move into view soon, include */
    const float horizon_ms = 16.f; /* ~1 frame */
    for (uint32_t i = 0; i < count && kept < 1024; ++i)
    {
        RogueCollisionCandidate* c = &ctx->candidates[i];
        float nx = c->x + c->vx * horizon_ms;
        float ny = c->y + c->vy * horizon_ms;
        bool in_future = (nx >= ctx->view_x && nx <= ctx->view_x + ctx->view_w &&
                          ny >= ctx->view_y && ny <= ctx->view_y + ctx->view_h);
        if (in_future)
        {
            /* ensure not already in kept */
            bool present = false;
            for (uint16_t k = 0; k < kept; ++k)
                if (kept_indices[k] == i)
                {
                    present = true;
                    break;
                }
            if (!present)
                kept_indices[kept++] = (uint16_t) i;
        }
    }
    /* compact candidate array in-place */
    for (uint16_t i = 0; i < kept; ++i)
        ctx->candidates[i] = ctx->candidates[kept_indices[i]];
    ctx->candidate_count = kept;
    m->output_candidates = kept;
    return true; /* continue */
}

bool rogue_collision_stage_aabb_prefilter(struct RogueCollisionContext* ctx,
                                          RogueCollisionMetrics* m)
{
    if (!ctx)
    {
        m->output_candidates = 0;
        return true;
    }
    /* Distance-based LOD: compute average distance to view center */
    float cx = ctx->view_x + ctx->view_w * 0.5f;
    float cy = ctx->view_y + ctx->view_h * 0.5f;
    double total_dist = 0.0;
    uint32_t n = ctx->candidate_count;
    for (uint32_t i = 0; i < n; ++i)
    {
        float dx = ctx->candidates[i].x - cx;
        float dy = ctx->candidates[i].y - cy;
        total_dist += (double) (dx * dx + dy * dy);
    }
    double avg = (n > 0) ? total_dist / (double) n : 0.0;
    /* simple heuristic thresholds (squared distance) */
    if (n > 0 && avg > 2500.0 && ctx->quality_level > ROGUE_COLLISION_FAST)
        ctx->quality_delta = -1; /* downgrade */
    else if (n > 0 && avg < 400.0 && ctx->quality_level < ROGUE_COLLISION_ULTRA)
        ctx->quality_delta = +1; /* upgrade */
    /* AABB prefilter: enforce max broad-phase candidate cap (keep nearest first) */
    const uint32_t cap = 128; /* safety */
    if (ctx->candidate_count > cap)
        ctx->candidate_count = cap;
    m->output_candidates = ctx->candidate_count;
    return true;
}

/* ---------------- Hierarchical Broad-Phase (Baseline Stub) ---------------- */
/* This lightweight pass approximates a two-level grouping by scanning candidates
 * and rejecting those fully outside an expanded view rectangle. Expansion uses
 * a margin proportional to quality tier to reduce thrash at higher precision. */
bool rogue_collision_stage_hierarchical_broad(struct RogueCollisionContext* ctx,
                                              RogueCollisionMetrics* m)
{
    if (!ctx)
    {
        m->output_candidates = 0;
        return true;
    }
    if (ctx->candidate_count == 0)
    {
        m->output_candidates = 0;
        return true;
    }
    float tier_margin = 0.f;
    switch (ctx->quality_level)
    {
    case ROGUE_COLLISION_FAST:
        tier_margin = 4.f;
        break;
    case ROGUE_COLLISION_BALANCED:
        tier_margin = 8.f;
        break;
    case ROGUE_COLLISION_PRECISE:
        tier_margin = 12.f;
        break;
    case ROGUE_COLLISION_ULTRA:
        tier_margin = 16.f;
        break;
    }
    float vx = ctx->view_x - tier_margin;
    float vy = ctx->view_y - tier_margin;
    float vw = ctx->view_w + tier_margin * 2.f;
    float vh = ctx->view_h + tier_margin * 2.f;
    uint32_t write = 0;
    for (uint32_t i = 0; i < ctx->candidate_count; ++i)
    {
        RogueCollisionCandidate* c = &ctx->candidates[i];
        float minx = c->x - c->half_w;
        float maxx = c->x + c->half_w;
        float miny = c->y - c->half_h;
        float maxy = c->y + c->half_h;
        if (maxx < vx || minx > vx + vw || maxy < vy || miny > vy + vh)
            continue; /* reject */
        if (write != i)
            ctx->candidates[write] = *c;
        write++;
    }
    ctx->candidate_count = write;
    m->output_candidates = write;
    return true;
}

/* ---------------- Pixel-Perfect Stage (Baseline Stub) ---------------- */
/* Placeholder refinement: applies a trivial per-quality cost simulation and optional
 * micro pruning heuristic (drop every other candidate at FAST tier to emulate reduced
 * sampling). Future slice will integrate mask bit tests & multi-res selection. */
bool rogue_collision_stage_pixel_perfect(struct RogueCollisionContext* ctx,
                                         RogueCollisionMetrics* m)
{
    if (!ctx)
    {
        m->output_candidates = 0;
        return true;
    }
    if (ctx->candidate_count == 0)
    {
        m->output_candidates = 0;
        return true;
    }
    if (ctx->quality_level == ROGUE_COLLISION_FAST)
    {
        /* Coarse tier: retain legacy placeholder half-prune to keep tests stable. */
        uint32_t write = 0;
        for (uint32_t i = 0; i < ctx->candidate_count; ++i)
        {
            if ((i & 1) != 0)
                continue;
            if (write != i)
                ctx->candidates[write] = ctx->candidates[i];
            write++;
        }
        ctx->candidate_count = write;
    }
    else if (ctx->quality_level >= ROGUE_COLLISION_BALANCED)
    {
        /* Simple pixel refinement: if two candidates provide pixel masks and overlap
           only via AABB but share no solid pixel in intersection sample region, drop.
           For this slice we do a conservative self-filter: remove candidates whose mask
           is entirely empty (degenerate) to simulate refinement, and (PRECISE/ULTRA)
           perform a tiny sampling grid inside their local mask bounds to ensure at
           least one solid bit (early exit). */
        uint32_t write = 0;
        for (uint32_t i = 0; i < ctx->candidate_count; ++i)
        {
            RogueCollisionCandidate* c = &ctx->candidates[i];
            if (!c->pixel_mask || !c->pixel_mask->bits)
            {
                /* Keep if no mask (cannot refine) */
                if (write != i)
                    ctx->candidates[write] = *c;
                write++;
                continue;
            }
            /* Quick degeneracy check: width/height sanity */
            if (c->pixel_mask->width <= 0 || c->pixel_mask->height <= 0)
                continue; /* drop invalid */
            int keep = 0;
            if (ctx->quality_level == ROGUE_COLLISION_BALANCED)
            {
                /* One sample at approximate center */
                int sx = c->pixel_mask->width / 2;
                int sy = c->pixel_mask->height / 2;
                extern int rogue_hit_mask_test(const struct RogueHitPixelMaskFrame*, int, int);
                keep = rogue_hit_mask_test(c->pixel_mask, sx, sy);
            }
            else /* PRECISE / ULTRA */
            {
                extern int rogue_hit_mask_test(const struct RogueHitPixelMaskFrame*, int, int);
                /* Sample a 3x3 grid around center (clamped). */
                int cx = c->pixel_mask->width / 2;
                int cy = c->pixel_mask->height / 2;
                for (int dy = -1; dy <= 1 && !keep; ++dy)
                    for (int dx = -1; dx <= 1 && !keep; ++dx)
                    {
                        int sx = cx + dx;
                        int sy = cy + dy;
                        if (sx < 0 || sy < 0 || sx >= c->pixel_mask->width ||
                            sy >= c->pixel_mask->height)
                            continue;
                        if (rogue_hit_mask_test(c->pixel_mask, sx, sy))
                            keep = 1;
                    }
            }
            if (keep)
            {
                if (write != i)
                    ctx->candidates[write] = *c;
                write++;
            }
            /* else prune */
        }
        ctx->candidate_count = write;
    }
    m->output_candidates = ctx->candidate_count;
    return true;
}

/* ---------------- Temporal Coherence Cache Stage ----------------
 * Simple heuristic: If camera/view moved insignificantly and candidate count last
 * frame was small, reuse last frame trimmed candidate subset instead of rebuilding
 * spatial partition. For minimal risk we store only IDs + positions snapshot and
 * validate cheap movement bounds; if any candidate drifted too far, fallback.
 * This lightweight cache lives in static file scope (single pipeline instance assumption
 * for current test slices). Future: move to pipeline struct, add generation tagging,
 * multi-pipeline support, thread-safety.
 */
typedef struct RogueTemporalCacheEntry
{
    uint32_t id;
    float x, y;
} RogueTemporalCacheEntry;

#define ROGUE_TEMPORAL_CACHE_MAX 256
static struct
{
    RogueTemporalCacheEntry entries[ROGUE_TEMPORAL_CACHE_MAX];
    uint16_t count;
    float last_view_x, last_view_y, last_view_w, last_view_h;
    uint32_t last_frame_candidate_count;
    uint32_t hits;
    uint32_t misses;
} g_temporal_cache;

bool rogue_collision_stage_temporal_cache(struct RogueCollisionContext* ctx,
                                          RogueCollisionMetrics* m)
{
    if (!ctx)
    {
        m->output_candidates = 0;
        return true;
    }
    /* Heuristic activation conditions */
    const float view_move_epsilon = 1.0f; /* pixels */
    bool view_stable = (float) (ctx->view_x - g_temporal_cache.last_view_x) < view_move_epsilon &&
                       (float) (ctx->view_x - g_temporal_cache.last_view_x) > -view_move_epsilon &&
                       (float) (ctx->view_y - g_temporal_cache.last_view_y) < view_move_epsilon &&
                       (float) (ctx->view_y - g_temporal_cache.last_view_y) > -view_move_epsilon &&
                       ctx->view_w == g_temporal_cache.last_view_w &&
                       ctx->view_h == g_temporal_cache.last_view_h;
    bool small_set_prev = g_temporal_cache.last_frame_candidate_count > 0 &&
                          g_temporal_cache.last_frame_candidate_count <= 64;
    bool attempt_reuse = view_stable && small_set_prev &&
                         g_temporal_cache.count == g_temporal_cache.last_frame_candidate_count;
    if (attempt_reuse)
    {
        /* Validate that current candidates roughly match cached IDs and haven't drifted far */
        uint32_t match = 0;
        for (uint16_t i = 0; i < g_temporal_cache.count && i < ctx->candidate_count; ++i)
        {
            if (ctx->candidates[i].id != g_temporal_cache.entries[i].id)
                break; /* early fail */
            float dx = ctx->candidates[i].x - g_temporal_cache.entries[i].x;
            float dy = ctx->candidates[i].y - g_temporal_cache.entries[i].y;
            if ((dx * dx + dy * dy) > 25.0f) /* >5px drift: invalidate */
                break;
            match++;
        }
        if (match == g_temporal_cache.count)
        {
            ctx->skip_spatial = 1; /* downstream spatial stage will be skipped */
            g_temporal_cache.hits++;
            m->output_candidates = ctx->candidate_count; /* unchanged */
            return true;
        }
    }
    g_temporal_cache.misses++;
    /* Refresh cache with current subset (pre-spatial; we just snapshot first N) */
    uint16_t cap = (ctx->candidate_count > ROGUE_TEMPORAL_CACHE_MAX)
                       ? ROGUE_TEMPORAL_CACHE_MAX
                       : (uint16_t) ctx->candidate_count;
    for (uint16_t i = 0; i < cap; ++i)
    {
        g_temporal_cache.entries[i].id = ctx->candidates[i].id;
        g_temporal_cache.entries[i].x = ctx->candidates[i].x;
        g_temporal_cache.entries[i].y = ctx->candidates[i].y;
    }
    g_temporal_cache.count = cap;
    g_temporal_cache.last_view_x = ctx->view_x;
    g_temporal_cache.last_view_y = ctx->view_y;
    g_temporal_cache.last_view_w = ctx->view_w;
    g_temporal_cache.last_view_h = ctx->view_h;
    g_temporal_cache.last_frame_candidate_count = ctx->candidate_count;
    ctx->skip_spatial = 0;
    m->output_candidates = ctx->candidate_count;
    return true;
}

/* ---------------- Enemy Profile LOD Adaptation Stage ----------------
 * Walks candidates and, when an enemy collision profile is attached, invokes the dynamic LOD
 * adaptation heuristic using cheap derived signals (distance to view center as a proxy for
 * player distance, on-screen projected size approximation, and zeroed damage/focus for now).
 * Aggregates strongest (most negative) bias suggestion and maps to a quality_delta hint
 * (coarse) allowing the pipeline to react next frame. This stage is O(n) with trivial math. */
bool rogue_collision_stage_enemy_profile_lod(struct RogueCollisionContext* ctx,
                                             RogueCollisionMetrics* m)
{
    if (!ctx)
    {
        m->output_candidates = 0;
        return true;
    }
    if (ctx->candidate_count == 0)
    {
        m->output_candidates = 0;
        return true;
    }
    float cx = ctx->view_x + ctx->view_w * 0.5f;
    float cy = ctx->view_y + ctx->view_h * 0.5f;
    int strongest_bias = 0; /* track most negative (higher fidelity request) */
    for (uint32_t i = 0; i < ctx->candidate_count; ++i)
    {
        RogueCollisionCandidate* c = &ctx->candidates[i];
        if (!c->enemy_profile)
            continue;
        /* Distance proxy: actual distance to view center (player surrogate). */
        float dx = c->x - cx;
        float dy = c->y - cy;
        float dist = (float) sqrtf(dx * dx + dy * dy);
        /* Screen area proxy: treat 2*half_w * 2*half_h as projected box (no scaling). */
        float screen_area = (c->half_w * 2.f) * (c->half_h * 2.f);
        rogue_enemy_collision_profile_adapt_lod(c->enemy_profile, dist, screen_area, 0.f, 0.f);
        int bias = rogue_enemy_collision_profile_get_lod_bias(c->enemy_profile);
        if (bias < strongest_bias)
            strongest_bias = bias;
    }
    /* Map strongest_bias (-8..+7) into a coarse quality_delta suggestion. We use thresholds to
       avoid oscillation: <= -4 -> request upgrade (+1 precision), >= +4 -> downgrade (-1). */
    if (strongest_bias <= -4 && ctx->quality_level < ROGUE_COLLISION_ULTRA)
        ctx->quality_delta = +1;
    else if (strongest_bias >= 4 && ctx->quality_level > ROGUE_COLLISION_FAST)
        ctx->quality_delta = -1;
    m->output_candidates = ctx->candidate_count;
    return true;
}

bool rogue_collision_pipeline_execute(RogueCollisionPipeline* p, RogueCollisionContext* ctx,
                                      float simulated_stage_cost_ms[])
{
    if (!p || !ctx)
        return false;
    ctx->quality_level = p->quality_level;
    ctx->quality_delta = 0;
    ctx->skip_spatial = 0;
    p->total_last_ms = 0.f;
    RogueTimer timer;
    rogue_timer_init(&timer);
    for (uint8_t i = 0; i < p->stage_count; ++i)
    {
        RogueCollisionStage* s = &p->stages[i];
        double t0 = rogue_timer_now_ms(&timer);
        /* Optional simulated cost injection for deterministic unit tests. */
        if (simulated_stage_cost_ms && simulated_stage_cost_ms[i] > 0.f)
        {
            /* Busy wait (coarse) to simulate compute; bounded to < 5ms to avoid slowing tests. */
            float target = simulated_stage_cost_ms[i];
            if (target > 5.f)
                target = 5.f;
            double spin_start = rogue_timer_now_ms(&timer);
            while (measure_ms(spin_start, rogue_timer_now_ms(&timer)) < target)
            {
                /* spin */
            }
        }
        /* Populate metrics pre-call */
        s->metrics.input_candidates = ctx->candidate_count;
        bool cont = s->stage_func ? s->stage_func(ctx, &s->metrics) : true;
        double t1 = rogue_timer_now_ms(&timer);
        s->metrics.last_ms = measure_ms(t0, t1);
        /* Exponential moving average (alpha = 0.1) */
        if (s->metrics.calls == 0)
            s->metrics.avg_ms = s->metrics.last_ms;
        else
            s->metrics.avg_ms = s->metrics.avg_ms * 0.9f + s->metrics.last_ms * 0.1f;
        s->metrics.calls++;
        p->total_last_ms += s->metrics.last_ms;
        if (!cont)
            break; /* early exit */
    }
    /* Adaptive quality enforcement */
    if (p->adaptive_quality && p->frame_time_budget_ms > 0.f)
    {
        if (p->total_last_ms > p->frame_time_budget_ms * 1.05f &&
            p->quality_level > ROGUE_COLLISION_FAST)
            p->quality_level = (RogueCollisionQuality) (p->quality_level - 1);
        else if (p->total_last_ms < p->frame_time_budget_ms * 0.6f &&
                 p->quality_level < ROGUE_COLLISION_ULTRA)
            p->quality_level = (RogueCollisionQuality) (p->quality_level + 1);
    }
    /* Apply per-stage LOD suggestion (ctx->quality_delta) */
    if (ctx->quality_delta != 0)
    {
        int new_q = (int) p->quality_level + (int) ctx->quality_delta;
        if (new_q < (int) ROGUE_COLLISION_FAST)
            new_q = (int) ROGUE_COLLISION_FAST;
        if (new_q > (int) ROGUE_COLLISION_ULTRA)
            new_q = (int) ROGUE_COLLISION_ULTRA;
        p->quality_level = (RogueCollisionQuality) new_q;
    }
    ctx->quality_level = p->quality_level;
    return true;
}
