/* enemy_collision_opt.h - Milestone 2.3: Enemy Collision Optimization
 * Phase progression:
 *   Slice 1 (scaffolding): basic radius+density heuristic & batch aggregate (DONE)
 *   Slice 2 (this patch): shape descriptor heuristics, SIMD batch_process stub,
 *                         dynamic LOD bias adaptation API, function rename,
 *                         performance prediction placeholders.
 *
 *   Future (still deferred):
 *     - Full sprite convex hull / symmetry sampling pass (replacing heuristics)
 *     - Wider SIMD specializations (AVX2/NEON) & prefetch scheduling
 *     - Runtime importance feedback blending (threat, visibility, camera focus)
 *     - Descriptor persistence & hot‑reload invalidation hooks
 */
#ifndef ROGUE_GAME_ENEMY_COLLISION_OPT_H
#define ROGUE_GAME_ENEMY_COLLISION_OPT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Forward declarations (avoid heavy includes) */
    struct RogueEnemy;             /* defined in entities/enemy.h */
    struct RogueHitPixelMaskFrame; /* hit_pixel_mask.h */

    typedef struct RogueEnemyCollisionProfile
    {
        float radius_estimate;     /* Fast circular approximation (>=0). */
        uint8_t complexity_level;  /* 0=circle,1=poly-like,2=pixel-rich (heuristic). */
        uint16_t pixel_density;    /* Collision pixels per unit area (scaled*100). */
        uint8_t supports_rotation; /* bool (stored as u8) indicates rotated sprite usage. */
        uint8_t lod_bias;          /* Signed bias encoded as uint8_t offset of 8 (0 => -8). */
        /* --- Shape descriptor heuristics (lightweight, constant time) --- */
        float aspect_ratio;    /* max(w,h)/min(w,h) (>=1). */
        float fill_ratio;      /* solid_pixels/(w*h) clamped 0..1. */
        float symmetry_score;  /* 0..1 (1 ~ near square aspect). */
        float convexity_score; /* 0..1 placeholder (future precise). */
        /* --- Performance prediction placeholders (coarse, heuristic) --- */
        uint32_t cycles_estimate; /* Approx predicted cycles for detailed collision path. */
        uint8_t method_hint;      /* 0=circle,1=poly-lite,2=pixel-dense (select algorithm). */
        uint8_t _reserved_pad[3]; /* Future expansion / alignment. */
    } RogueEnemyCollisionProfile;

#define ROGUE_ENEMY_BATCH_CAP 32

    typedef struct RogueEnemyCollisionBatch
    {
        uint16_t enemy_indices[ROGUE_ENEMY_BATCH_CAP]; /* Indices into caller enemy array. */
        uint8_t batch_size;                            /* Active count (<=32). */
        float centroid_x;                              /* Cached geometric center X. */
        float centroid_y;                              /* Cached geometric center Y. */
        float batch_radius;      /* Radius covering all approximated circles. */
        uint32_t collision_mask; /* Generic result bitfield (test flags). */
    } RogueEnemyCollisionBatch;

    /* Encode signed lod bias (-8..+7) into profile.lod_bias (offset 8). */
    static inline void rogue_enemy_collision_profile_set_lod_bias(RogueEnemyCollisionProfile* p,
                                                                  int bias)
    {
        if (!p)
            return;
        if (bias < -8)
            bias = -8;
        if (bias > 7)
            bias = 7;
        p->lod_bias = (uint8_t) (bias + 8); /* store 0..15 */
    }
    static inline int
    rogue_enemy_collision_profile_get_lod_bias(const RogueEnemyCollisionProfile* p)
    {
        return p ? ((int) p->lod_bias - 8) : 0;
    }

    /* Heuristic profile analysis (expanded): derives radius, density buckets, and shape
     * descriptors from AABB dimensions and pixel mask stats. Deterministic & O(1). */
    void rogue_enemy_collision_profile_analyze(RogueEnemyCollisionProfile* out, float aabb_w,
                                               float aabb_h, uint32_t solid_pixels);

    /* Backwards compatibility wrapper (will be removed once call sites are migrated). */
    static inline void rogue_enemy_collision_profile_analyze_dims(RogueEnemyCollisionProfile* out,
                                                                  float aabb_w, float aabb_h,
                                                                  uint32_t solid_pixels)
    {
        rogue_enemy_collision_profile_analyze(out, aabb_w, aabb_h, solid_pixels);
    }

    /* Batch builder: Append an enemy index with its position & radius approximation updating
     * centroid and covering radius. Returns 0 on success, -1 if batch full. */
    int rogue_enemy_collision_batch_add(RogueEnemyCollisionBatch* b, uint16_t enemy_index,
                                        float enemy_x, float enemy_y, float enemy_radius);

    /* Reset batch to empty. */
    static inline void rogue_enemy_collision_batch_reset(RogueEnemyCollisionBatch* b)
    {
        if (!b)
            return;
        b->batch_size = 0;
        b->centroid_x = b->centroid_y = 0.f;
        b->batch_radius = 0.f;
        b->collision_mask = 0;
    }

    /* Finalize after additions: compute centroid & covering radius (if not incrementally done). */
    void rogue_enemy_collision_batch_finalize(RogueEnemyCollisionBatch* b, const float* xs,
                                              const float* ys, const float* radii);

    /* SIMD-friendly batch processing (prototype implementation): fills centroid & covering
     * radius directly from arrays (count capped at ROGUE_ENEMY_BATCH_CAP). Target indices are
     * sequential (0..count-1) for caller convenience when building transient batches.
     * If ROGUE_ENEMY_COLLISION_ENABLE_SIMD is defined at compile time and a supported
     * instruction set is available, a specialized path may be taken (currently stub). */
    void rogue_enemy_collision_batch_process(RogueEnemyCollisionBatch* b, const float* xs,
                                             const float* ys, const float* radii, uint8_t count);

    /* Dynamic LOD bias adaptation: adjusts lod_bias based on gameplay importance signals.
     * distance_to_player: world units (smaller => higher importance)
     * screen_area_px: approximate on-screen projected area (higher => more visible)
     * recent_damage: damage dealt/received window (normalized 0..1 suggested)
     * focus_time_s: seconds the player/camera focused target (0..n)
     * The resulting bias is negative for high-importance (higher fidelity) and positive for
     * low-importance (more aggressive simplification). */
    void rogue_enemy_collision_profile_adapt_lod(RogueEnemyCollisionProfile* p,
                                                 float distance_to_player, float screen_area_px,
                                                 float recent_damage, float focus_time_s);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_GAME_ENEMY_COLLISION_OPT_H */
