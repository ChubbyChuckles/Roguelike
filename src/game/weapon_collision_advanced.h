/* weapon_collision_advanced.h - Milestone 2.2 initial slice
 * Provides minimal advanced weapon pose integration primitives:
 *  - Weapon collision state with transform (2D affine 3x3), velocity, layer mask
 *  - Simple trail ring buffer for sweep tests
 *  - Transform computation (translation + rotation + uniform scale) baseline
 *  - Trail sweep producing an AABB covering recent motion (coarse)
 *  - Layer filtering helper against RogueCollisionCandidate layer_mask
 *
 * Deferred (roadmap): motion blur sampling, bilinear pixel mask transform, per-weapon quality
 * overrides.
 */
#ifndef ROGUE_WEAPON_COLLISION_ADVANCED_H
#define ROGUE_WEAPON_COLLISION_ADVANCED_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueMatrix3x3
    {
        float m[9]; /* Row-major 3x3 affine matrix (last row implicit 0 0 1 when
                       m[6]=m[7]=0,m[8]=1). */
    } RogueMatrix3x3;

    static inline RogueMatrix3x3 rogue_matrix_identity(void)
    {
        RogueMatrix3x3 r = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
        return r;
    }

    static inline RogueMatrix3x3 rogue_matrix_mul(RogueMatrix3x3 a, RogueMatrix3x3 b)
    {
        RogueMatrix3x3 r;
        r.m[0] = a.m[0] * b.m[0] + a.m[1] * b.m[3] + a.m[2] * b.m[6];
        r.m[1] = a.m[0] * b.m[1] + a.m[1] * b.m[4] + a.m[2] * b.m[7];
        r.m[2] = a.m[0] * b.m[2] + a.m[1] * b.m[5] + a.m[2] * b.m[8];
        r.m[3] = a.m[3] * b.m[0] + a.m[4] * b.m[3] + a.m[5] * b.m[6];
        r.m[4] = a.m[3] * b.m[1] + a.m[4] * b.m[4] + a.m[5] * b.m[7];
        r.m[5] = a.m[3] * b.m[2] + a.m[4] * b.m[5] + a.m[5] * b.m[8];
        r.m[6] = a.m[6] * b.m[0] + a.m[7] * b.m[3] + a.m[8] * b.m[6];
        r.m[7] = a.m[6] * b.m[1] + a.m[7] * b.m[4] + a.m[8] * b.m[7];
        r.m[8] = a.m[6] * b.m[2] + a.m[7] * b.m[5] + a.m[8] * b.m[8];
        return r;
    }

    static inline RogueMatrix3x3 rogue_matrix_translate(float tx, float ty)
    {
        RogueMatrix3x3 r = {{1, 0, tx, 0, 1, ty, 0, 0, 1}};
        return r;
    }
    static inline RogueMatrix3x3 rogue_matrix_rotate(float radians)
    {
        float c = cosf(radians), s = sinf(radians);
        RogueMatrix3x3 r = {{c, -s, 0, s, c, 0, 0, 0, 1}};
        return r;
    }
    static inline RogueMatrix3x3 rogue_matrix_scale(float sx, float sy)
    {
        RogueMatrix3x3 r = {{sx, 0, 0, 0, sy, 0, 0, 0, 1}};
        return r;
    }

    typedef struct RogueWeaponTrail
    {
        float positions[16 * 2]; /* x,y pairs */
        float timestamps[16];
        uint8_t sample_count;    /* number of valid samples */
        float trail_duration_ms; /* window */
    } RogueWeaponTrail;

    static inline void rogue_weapon_trail_init(RogueWeaponTrail* t, float duration_ms)
    {
        t->sample_count = 0;
        t->trail_duration_ms = duration_ms;
    }

    static inline void rogue_weapon_trail_add(RogueWeaponTrail* t, float x, float y, float now_ms)
    {
        if (t->sample_count < 16)
        {
            uint8_t i = t->sample_count++;
            t->positions[i * 2] = x;
            t->positions[i * 2 + 1] = y;
            t->timestamps[i] = now_ms;
        }
        else
        { /* simple FIFO shift */
            for (int i = 1; i < 16; ++i)
            {
                t->positions[(i - 1) * 2] = t->positions[i * 2];
                t->positions[(i - 1) * 2 + 1] = t->positions[i * 2 + 1];
                t->timestamps[i - 1] = t->timestamps[i];
            }
            t->positions[30] = x;
            t->positions[31] = y;
            t->timestamps[15] = now_ms;
        }
        /* prune old */
        int start = 0;
        while (start < t->sample_count && (now_ms - t->timestamps[start]) > t->trail_duration_ms)
            start++;
        if (start > 0)
        {
            for (int i = start; i < t->sample_count; ++i)
            {
                t->positions[(i - start) * 2] = t->positions[i * 2];
                t->positions[(i - start) * 2 + 1] = t->positions[i * 2 + 1];
                t->timestamps[i - start] = t->timestamps[i];
            }
            t->sample_count -= start;
        }
    }

    typedef struct RogueWeaponCollisionState
    {
        RogueMatrix3x3 transform_matrix; /* world transform */
        float velocity_x, velocity_y;
        float animation_time;
        uint32_t collision_layer_mask;
        uint8_t quality_override; /* 0 = none else RogueCollisionQuality */
        RogueWeaponTrail trail;
        float aabb_min_x, aabb_min_y, aabb_max_x, aabb_max_y; /* cached bounding */
    } RogueWeaponCollisionState;

    static inline void rogue_weapon_collision_state_init(RogueWeaponCollisionState* s,
                                                         float trail_ms)
    {
        s->transform_matrix = rogue_matrix_identity();
        s->velocity_x = s->velocity_y = 0.f;
        s->animation_time = 0.f;
        s->collision_layer_mask = 0xFFFFFFFFu;
        s->quality_override = 0;
        rogue_weapon_trail_init(&s->trail, trail_ms);
        s->aabb_min_x = s->aabb_min_y = 0.f;
        s->aabb_max_x = s->aabb_max_y = 0.f;
    }

    static inline void rogue_weapon_collision_compute_transform(RogueWeaponCollisionState* s,
                                                                float px, float py, float rot_rad,
                                                                float scale, float vx, float vy)
    {
        RogueMatrix3x3 T = rogue_matrix_translate(px, py);
        RogueMatrix3x3 R = rogue_matrix_rotate(rot_rad);
        /* Note: For the purposes of collision trail and orientation checks in this
         * milestone, we retain a unit-orthonormal rotation in the transform matrix
         * (scale is not baked into the 2x2 rotation block). Scaling can be applied
         * downstream to collision geometry as needed in later slices. */
        s->transform_matrix = rogue_matrix_mul(T, R);
        s->velocity_x = vx;
        s->velocity_y = vy;
    }

    static inline void rogue_weapon_collision_update_trail(RogueWeaponCollisionState* s,
                                                           float now_ms)
    {
        float x = s->transform_matrix.m[2];
        float y = s->transform_matrix.m[5];
        rogue_weapon_trail_add(&s->trail, x, y, now_ms);
        /* Recompute coarse trail AABB from trail samples */
        if (s->trail.sample_count > 0)
        {
            float minx = s->trail.positions[0];
            float miny = s->trail.positions[1];
            float maxx = minx, maxy = miny;
            for (uint8_t i = 1; i < s->trail.sample_count; ++i)
            {
                float tx = s->trail.positions[i * 2];
                float ty = s->trail.positions[i * 2 + 1];
                if (tx < minx)
                    minx = tx;
                if (ty < miny)
                    miny = ty;
                if (tx > maxx)
                    maxx = tx;
                if (ty > maxy)
                    maxy = ty;
            }
            s->aabb_min_x = minx;
            s->aabb_min_y = miny;
            s->aabb_max_x = maxx;
            s->aabb_max_y = maxy;
        }
    }

    /* Compute coarse sweep AABB from trail samples; returns false if insufficient samples */
    static inline bool rogue_weapon_collision_trail_aabb(const RogueWeaponCollisionState* s,
                                                         float* out_minx, float* out_miny,
                                                         float* out_maxx, float* out_maxy)
    {
        if (s->trail.sample_count < 1)
            return false;
        float minx = s->trail.positions[0], miny = s->trail.positions[1];
        float maxx = minx, maxy = miny;
        for (uint8_t i = 1; i < s->trail.sample_count; ++i)
        {
            float x = s->trail.positions[i * 2];
            float y = s->trail.positions[i * 2 + 1];
            if (x < minx)
                minx = x;
            if (y < miny)
                miny = y;
            if (x > maxx)
                maxx = x;
            if (y > maxy)
                maxy = y;
        }
        if (out_minx)
            *out_minx = minx;
        if (out_miny)
            *out_miny = miny;
        if (out_maxx)
            *out_maxx = maxx;
        if (out_maxy)
            *out_maxy = maxy;
        return true;
    }

    /* Layer filter helper: returns true if candidate layers overlap weapon mask */
    struct RogueCollisionCandidate; /* fwd */
    static inline bool rogue_weapon_candidate_layer_match(uint32_t weapon_mask,
                                                          uint32_t candidate_mask)
    {
        return (weapon_mask & candidate_mask) != 0;
    }

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WEAPON_COLLISION_ADVANCED_H */
