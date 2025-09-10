/* enemy_collision_opt.c - Milestone 2.3: Enemy Collision Optimization (expanded slice)
 * Adds lightweight shape descriptors (aspect/fill/symmetry/convexity), dynamic LOD bias
 * adaptation heuristic, SIMD-friendly batch_process stub, and renames analyze_dims → analyze.
 * Heavyweight geometric analysis and real SIMD specializations are deferred to later slices.
 */
#include "game/enemy_collision_opt.h"
#include <math.h>
#include <string.h>

/* Heuristic profiling: constant-time estimates without inspecting full pixel mask layout.
 *  - radius_estimate : 0.5 * sqrt(w^2 + h^2)
 *  - density         : solid_pixels/(w*h) (fill_ratio)
 *  - aspect_ratio    : max(w,h)/max(1,min(w,h)) (>=1)
 *  - symmetry_score  : 1 - |log2(aspect_ratio)| clamped 0..1 (1 = square-ish)
 *  - convexity_score : placeholder = symmetry_score * 0.9 (future: hull/perimeter test)
 *  - complexity_level buckets on density (same thresholds) with an extra bump if aspect extreme
 */
void rogue_enemy_collision_profile_analyze(RogueEnemyCollisionProfile* out, float aabb_w,
                                           float aabb_h, uint32_t solid_pixels)
{
    if (!out)
    {
        return;
    }
    if (aabb_w < 0.f)
        aabb_w = 0.f;
    if (aabb_h < 0.f)
        aabb_h = 0.f;
    float diag = sqrtf(aabb_w * aabb_w + aabb_h * aabb_h);
    out->radius_estimate = 0.5f * diag;
    float area = (aabb_w <= 0.f || aabb_h <= 0.f) ? 1.f : (aabb_w * aabb_h);
    float density = (float) solid_pixels / area;
    if (density < 0.f)
        density = 0.f; /* clamp */
    if (density > 1000.f)
        density = 1000.f; /* sanity guard */
    out->fill_ratio = density > 1.f ? 1.f : density;
    out->pixel_density = (uint16_t) ((density * 100.f) > 65535.f ? 65535.f : (density * 100.f));
    float maxd = (aabb_w > aabb_h ? aabb_w : aabb_h);
    float mind = (aabb_w < aabb_h ? aabb_w : aabb_h);
    if (mind <= 0.f)
        mind = 1.f;
    out->aspect_ratio = maxd / mind;
    float symmetry = 1.f - fabsf(log2f(out->aspect_ratio));
    if (symmetry < 0.f)
        symmetry = 0.f;
    if (symmetry > 1.f)
        symmetry = 1.f;
    out->symmetry_score = symmetry;
    out->convexity_score = symmetry * 0.9f; /* placeholder */
    if (density < 0.05f)
        out->complexity_level = 0;
    else if (density < 0.25f)
        out->complexity_level = 1;
    else
        out->complexity_level = 2;
    /* Slight complexity bump if extremely elongated but dense */
    if (out->complexity_level < 2 && out->aspect_ratio > 3.f && density >= 0.20f)
        out->complexity_level = 2;
    out->supports_rotation = (out->aspect_ratio < 1.2f) ? 1 : 0; /* guess; refine later */
    /* Performance prediction placeholders: cycles_estimate & method_hint.
     * Very coarse heuristic mapping for early instrumentation: */
    if (out->complexity_level == 0)
    {
        out->method_hint = 0;       /* circle */
        out->cycles_estimate = 150; /* nominal cheap path */
    }
    else if (out->complexity_level == 1)
    {
        out->method_hint = 1;       /* poly-lite */
        out->cycles_estimate = 450; /* moderate */
    }
    else
    {
        out->method_hint = 2; /* pixel-rich */
        /* Inflate estimate if very elongated which may hurt cache locality later */
        out->cycles_estimate = (uint32_t) (900 + (out->aspect_ratio > 3.f ? 200 : 0));
    }
    rogue_enemy_collision_profile_set_lod_bias(out, 0);
}

int rogue_enemy_collision_batch_add(RogueEnemyCollisionBatch* b, uint16_t enemy_index,
                                    float enemy_x, float enemy_y, float enemy_radius)
{
    if (!b)
        return -1;
    if (b->batch_size >= ROGUE_ENEMY_BATCH_CAP)
        return -1;
    b->enemy_indices[b->batch_size++] = enemy_index;
    /* Incremental centroid using running mean formula */
    float n = (float) b->batch_size;
    if (b->batch_size == 1)
    {
        b->centroid_x = enemy_x;
        b->centroid_y = enemy_y;
        b->batch_radius = enemy_radius;
        return 0;
    }
    float inv = 1.f / n;
    b->centroid_x = b->centroid_x + (enemy_x - b->centroid_x) * inv;
    b->centroid_y = b->centroid_y + (enemy_y - b->centroid_y) * inv;
    /* Conservative covering radius: max distance + enemy_radius */
    float dx = enemy_x - b->centroid_x;
    float dy = enemy_y - b->centroid_y;
    float dist = sqrtf(dx * dx + dy * dy) + enemy_radius;
    if (dist > b->batch_radius)
        b->batch_radius = dist;
    return 0;
}

void rogue_enemy_collision_batch_finalize(RogueEnemyCollisionBatch* b, const float* xs,
                                          const float* ys, const float* radii)
{
    if (!b)
        return;
    if (b->batch_size == 0)
        return;
    /* Recompute centroid for numerical stability if arrays provided */
    if (xs && ys)
    {
        double sumx = 0.0, sumy = 0.0;
        for (uint8_t i = 0; i < b->batch_size; ++i)
        {
            sumx += xs[i];
            sumy += ys[i];
        }
        b->centroid_x = (float) (sumx / (double) b->batch_size);
        b->centroid_y = (float) (sumy / (double) b->batch_size);
    }
    /* Recompute covering radius if radii provided */
    if (xs && ys && radii)
    {
        float maxr = 0.f;
        for (uint8_t i = 0; i < b->batch_size; ++i)
        {
            float dx = xs[i] - b->centroid_x;
            float dy = ys[i] - b->centroid_y;
            float r = sqrtf(dx * dx + dy * dy) + radii[i];
            if (r > maxr)
                maxr = r;
        }
        b->batch_radius = maxr;
    }
}

void rogue_enemy_collision_batch_process(RogueEnemyCollisionBatch* b, const float* xs,
                                         const float* ys, const float* radii, uint8_t count)
{
    if (!b || !xs || !ys || count == 0)
        return;
    if (count > ROGUE_ENEMY_BATCH_CAP)
        count = ROGUE_ENEMY_BATCH_CAP;
    rogue_enemy_collision_batch_reset(b);
#ifdef ROGUE_ENEMY_COLLISION_ENABLE_SIMD
    /* SIMD specialization placeholder: currently falls back to scalar path.
       Future implementation: horizontal sums via vector registers, radius expansion using
       fused distance computations. */
#endif
    double sumx = 0.0, sumy = 0.0;
    for (uint8_t i = 0; i < count; ++i)
    {
        b->enemy_indices[i] = i; /* sequential */
        sumx += xs[i];
        sumy += ys[i];
    }
    b->batch_size = count;
    b->centroid_x = (float) (sumx / (double) count);
    b->centroid_y = (float) (sumy / (double) count);
    float maxr = 0.f;
    if (radii)
    {
        for (uint8_t i = 0; i < count; ++i)
        {
            float dx = xs[i] - b->centroid_x;
            float dy = ys[i] - b->centroid_y;
            float r = sqrtf(dx * dx + dy * dy) + radii[i];
            if (r > maxr)
                maxr = r;
        }
    }
    b->batch_radius = maxr;
}

void rogue_enemy_collision_profile_adapt_lod(RogueEnemyCollisionProfile* p,
                                             float distance_to_player, float screen_area_px,
                                             float recent_damage, float focus_time_s)
{
    if (!p)
        return;
    /* Normalize crude signals: distance → importance inverse, screen_area direct, damage & focus
     * direct */
    if (distance_to_player < 0.f)
        distance_to_player = 0.f;
    if (screen_area_px < 0.f)
        screen_area_px = 0.f;
    if (recent_damage < 0.f)
        recent_damage = 0.f;
    if (focus_time_s < 0.f)
        focus_time_s = 0.f;
    /* Heuristic scales */
    float importance = 0.f;
    /* Distance: closer than 10 units strongly important, >100 minimal */
    float dist_factor = 0.f;
    if (distance_to_player <= 10.f)
        dist_factor = 1.f;
    else if (distance_to_player >= 100.f)
        dist_factor = 0.f;
    else
        dist_factor = 1.f - (distance_to_player - 10.f) / 90.f;
    float area_factor = screen_area_px / 10000.f; /* assume 10k px ~ full importance */
    if (area_factor > 1.f)
        area_factor = 1.f;
    float damage_factor = recent_damage;
    if (damage_factor > 1.f)
        damage_factor = 1.f;
    float focus_factor = focus_time_s / 5.f; /* 5s focus saturates */
    if (focus_factor > 1.f)
        focus_factor = 1.f;
    importance = (dist_factor * 0.35f) + (area_factor * 0.25f) + (damage_factor * 0.25f) +
                 (focus_factor * 0.15f);
    if (importance < 0.f)
        importance = 0.f;
    if (importance > 1.f)
        importance = 1.f;
    /* Map importance (0..1) to bias so that high importance -> negative (higher fidelity request).
        Importance 1 -> -4, importance 0 -> +3. */
    float desired = 3.f * (1.f - importance) - 4.f * importance; /* rearranged for clarity */
    int ibias = (int) (desired >= 0.f ? desired + 0.5f : desired - 0.5f);
    rogue_enemy_collision_profile_set_lod_bias(p, ibias);
}
