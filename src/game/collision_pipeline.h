/* collision_pipeline.h - Milestone 2.1: Multi-Resolution Collision Pipeline
 * Public API for a staged collision detection pipeline with adaptive quality,
 * spatial culling, temporal coherence, and pluggable stages. The initial slice
 * started minimal; subsequent slices added:
 *  - High-resolution timing & adaptive quality (frame budget aware)
 *  - Quadtree spatial culling with predictive (velocity horizon) inclusion
 *  - AABB prefilter + distance-based LOD heuristic
 *  - Temporal coherence cache (reuses prior frame candidate subset when stable)
 *
 * Upcoming (deferred) work:
 *  - SIMD & hierarchical broad-phase (BV trees / wide AABB tests)
 *  - Pixel-perfect stage + multi-resolution mask selection
 *  - Dynamic load balancing & stage reordering
 *  - Advanced frustum & priority ordering (threat / distance)
 */
#ifndef ROGUE_COLLISION_PIPELINE_H
#define ROGUE_COLLISION_PIPELINE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Quality tiers controlling depth / precision of the pipeline. */
    typedef enum RogueCollisionQuality
    {
        ROGUE_COLLISION_FAST = 0,     /* Broad approximations only */
        ROGUE_COLLISION_BALANCED = 1, /* Mixed broad + selective pixel tests */
        ROGUE_COLLISION_PRECISE = 2,  /* Full pixel-perfect for candidates */
        ROGUE_COLLISION_ULTRA = 3     /* Reserved for sub-pixel / interpolation */
    } RogueCollisionQuality;

    /* Forward declare opaque context to keep initial slice independent. */
    struct RogueCollisionContext;

    /* Minimal metrics to support early profiling & future expansion. */
    typedef struct RogueCollisionMetrics
    {
        float last_ms;              /* Time spent in last execution of stage. */
        float avg_ms;               /* Simple moving average (ema alpha=0.1f). */
        uint32_t input_candidates;  /* Candidates entering the stage. */
        uint32_t output_candidates; /* Candidates surviving the stage. */
        uint32_t calls;             /* Invocation count (lifetime). */
    } RogueCollisionMetrics;

    /* Stage function signature: returns true if pipeline should continue. */
    typedef bool (*RogueCollisionStageFn)(struct RogueCollisionContext* ctx,
                                          RogueCollisionMetrics* metrics);

    typedef struct RogueCollisionStage
    {
        const char* name;                 /* Descriptive stage label. */
        RogueCollisionStageFn stage_func; /* Implementation callback. */
        float time_budget_ms;             /* Optional soft budget (0 = ignore). */
        uint32_t max_candidates;          /* Hard cap for safety (0 = unlimited). */
        RogueCollisionMetrics metrics;    /* Accumulated metrics. */
    } RogueCollisionStage;

#define ROGUE_COLLISION_MAX_STAGES 8

    /* Pipeline container; adaptive_quality reserved for future dynamic downgrades. */
    typedef struct RogueCollisionPipeline
    {
        RogueCollisionStage stages[ROGUE_COLLISION_MAX_STAGES];
        uint8_t stage_count;
        RogueCollisionQuality quality_level;
        float frame_time_budget_ms;
        bool adaptive_quality;
        /* Aggregated metrics */
        float total_last_ms;
    } RogueCollisionPipeline;

    /* Minimal collision context prototype (expands later). */
    typedef struct RogueCollisionCandidate
    {
        uint32_t id;         /* Identifier (entity, enemy, etc). */
        float x, y;          /* World-space center position (pixels or tiles). */
        float half_w;        /* Half-width (AABB / broad radius). */
        float half_h;        /* Half-height. */
        float vx, vy;        /* Velocity (units per ms) for predictive culling. */
        uint32_t layer_mask; /* Collision layer bits (weapon/enemy filtering). */
        /* Optional pixel mask reference (if available) for pixel-perfect stage. */
        struct RogueHitPixelMaskFrame* pixel_mask; /* forward-declared in hit_pixel_mask.h */
        int pixel_mask_lx, pixel_mask_ly;          /* Local origin offset (top-left) */
    } RogueCollisionCandidate;

    typedef struct RogueCollisionContext
    {
        RogueCollisionCandidate* candidates; /* Dynamic array (owned externally). */
        uint32_t candidate_count;            /* Current active candidates. */
        RogueCollisionQuality quality_level; /* Mirror of pipeline quality. */
        void* user_data;                     /* Extension hook. */
        /* View / frustum rectangle (axis-aligned) for spatial culling. */
        float view_x, view_y, view_w, view_h;
        /* Internal scratch: last frame quality adjustment flag (0 none, +/-1 change). */
        int8_t quality_delta;
        /* Internal: temporal cache hint to skip spatial stage when set (cleared each execute). */
        int8_t skip_spatial;
    } RogueCollisionContext;

    /* Built-in stage helpers (implemented in .c). */
    bool rogue_collision_stage_spatial_cull(struct RogueCollisionContext* ctx,
                                            RogueCollisionMetrics* m);
    bool rogue_collision_stage_aabb_prefilter(struct RogueCollisionContext* ctx,
                                              RogueCollisionMetrics* m);
    bool rogue_collision_stage_temporal_cache(struct RogueCollisionContext* ctx,
                                              RogueCollisionMetrics* m);
    /* Hierarchical broad-phase (baseline stub): lightweight pass that prunes any candidates
        clearly outside the expanded view bounds using a simple two-level grouping heuristic.
        Future slice will replace with SIMD + BV tree. */
    bool rogue_collision_stage_hierarchical_broad(struct RogueCollisionContext* ctx,
                                                  RogueCollisionMetrics* m);
    /* Pixel-perfect stage (baseline stub): placeholder performing quality-tier dependent
        refinement work. Future slice will integrate real pixel mask bit tests and multi-resolution
        mask selection. */
    bool rogue_collision_stage_pixel_perfect(struct RogueCollisionContext* ctx,
                                             RogueCollisionMetrics* m);

    /* Initialization helpers */
    static inline void rogue_collision_pipeline_init(RogueCollisionPipeline* p,
                                                     RogueCollisionQuality quality,
                                                     float frame_budget_ms, bool adaptive)
    {
        if (!p)
            return;
        p->stage_count = 0;
        p->quality_level = quality;
        p->frame_time_budget_ms = frame_budget_ms;
        p->adaptive_quality = adaptive;
        p->total_last_ms = 0.f;
    }

    static inline bool rogue_collision_pipeline_add_stage(RogueCollisionPipeline* p,
                                                          const char* name,
                                                          RogueCollisionStageFn fn,
                                                          float time_budget_ms,
                                                          uint32_t max_candidates)
    {
        if (!p || p->stage_count >= ROGUE_COLLISION_MAX_STAGES || !fn)
            return false;
        RogueCollisionStage* s = &p->stages[p->stage_count++];
        s->name = name;
        s->stage_func = fn;
        s->time_budget_ms = time_budget_ms;
        s->max_candidates = max_candidates;
        s->metrics.last_ms = s->metrics.avg_ms = 0.f;
        s->metrics.input_candidates = s->metrics.output_candidates = 0;
        s->metrics.calls = 0;
        return true;
    }

    /* Execution entry point (stub implementation in .c). */
    bool rogue_collision_pipeline_execute(
        RogueCollisionPipeline* p, RogueCollisionContext* ctx,
        float simulated_stage_cost_ms[] /* optional scratch length >= stage_count */);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_COLLISION_PIPELINE_H */
