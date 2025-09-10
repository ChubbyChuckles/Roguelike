/* collision_pipeline.c - Milestone 2.1 initial executable slice
 * Provides a stub execution framework for multi-stage collision processing.
 * The intent is to allow incremental landing of later advanced features
 * (adaptive quality, spatial partition, SIMD) without blocking integration.
 */
#include "game/collision_pipeline.h"
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

bool rogue_collision_pipeline_execute(RogueCollisionPipeline* p, RogueCollisionContext* ctx,
                                      float simulated_stage_cost_ms[])
{
    if (!p || !ctx)
        return false;
    ctx->quality_level = p->quality_level;
    ctx->quality_delta = 0;
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
