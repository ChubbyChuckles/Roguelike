/* weapon_collision_advanced.c - Milestone 2.2 implementation slice
 * Minimal non-inline helpers backing weapon_collision_advanced.h
 */
#include "game/weapon_collision_advanced.h"

#include <float.h>

void rogue_weapon_matrix_transform_point(const RogueMatrix3x3* m, float x, float y, float* out_x,
                                         float* out_y)
{
    if (!m)
        return;
    float tx = m->m[0] * x + m->m[1] * y + m->m[2];
    float ty = m->m[3] * x + m->m[4] * y + m->m[5];
    if (out_x)
        *out_x = tx;
    if (out_y)
        *out_y = ty;
}

bool rogue_weapon_collision_sweep_bounds(const RogueWeaponCollisionState* s, float radius,
                                         float* out_minx, float* out_miny, float* out_maxx,
                                         float* out_maxy)
{
    if (!s)
        return false;
    float minx, miny, maxx, maxy;
    if (!rogue_weapon_collision_trail_aabb(s, &minx, &miny, &maxx, &maxy))
    {
        /* No trail yet: fall back to current transform translation as a degenerate AABB */
        float cx = s->transform_matrix.m[2];
        float cy = s->transform_matrix.m[5];
        minx = maxx = cx;
        miny = maxy = cy;
    }
    float r = radius < 0.f ? 0.f : radius;
    minx -= r;
    miny -= r;
    maxx += r;
    maxy += r;
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

bool rogue_weapon_collision_sweep_bounds_advanced(const RogueWeaponCollisionState* s, float radius,
                                                  float now_ms, float* out_minx, float* out_miny,
                                                  float* out_maxx, float* out_maxy)
{
    (void) now_ms; /* reserved for future predictive extension */
    if (!s)
        return false;
    const RogueWeaponTrail* t = &s->trail;
    if (t->sample_count == 0)
        return false;

    /* Determine samples per segment from quality override. */
    int q = (int) s->quality_override;
    int samples_per_segment = 3;
    if (q <= 1)
        samples_per_segment = 1; /* FAST */
    else if (q >= 3)
        samples_per_segment = 5; /* PRECISE/ULTRA */

    float minx = t->positions[0];
    float miny = t->positions[1];
    float maxx = minx, maxy = miny;

    /* Always include recorded points. */
    for (uint8_t i = 1; i < t->sample_count; ++i)
    {
        float x = t->positions[i * 2];
        float y = t->positions[i * 2 + 1];
        if (x < minx)
            minx = x;
        if (y < miny)
            miny = y;
        if (x > maxx)
            maxx = x;
        if (y > maxy)
            maxy = y;
    }

    /* Resample along each consecutive pair for conservative coverage. */
    if (samples_per_segment > 1 && t->sample_count > 1)
    {
        for (uint8_t i = 0; i + 1 < t->sample_count; ++i)
        {
            float x0 = t->positions[i * 2];
            float y0 = t->positions[i * 2 + 1];
            float x1 = t->positions[(i + 1) * 2];
            float y1 = t->positions[(i + 1) * 2 + 1];
            int steps = samples_per_segment - 1; /* interior points */
            for (int sidx = 1; sidx <= steps; ++sidx)
            {
                float tfrac = (float) sidx / (float) samples_per_segment;
                float xi = x0 + (x1 - x0) * tfrac;
                float yi = y0 + (y1 - y0) * tfrac;
                if (xi < minx)
                    minx = xi;
                if (yi < miny)
                    miny = yi;
                if (xi > maxx)
                    maxx = xi;
                if (yi > maxy)
                    maxy = yi;
            }
        }
    }

    float r = radius < 0.f ? 0.f : radius;
    minx -= r;
    miny -= r;
    maxx += r;
    maxy += r;

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

void rogue_weapon_collision_set_layer_mask(RogueWeaponCollisionState* s, uint32_t mask)
{
    if (!s)
        return;
    s->collision_layer_mask = mask;
}

uint32_t rogue_weapon_collision_get_layer_mask(const RogueWeaponCollisionState* s)
{
    if (!s)
        return 0u;
    return s->collision_layer_mask;
}

void rogue_weapon_collision_set_quality_override(RogueWeaponCollisionState* s, uint8_t q)
{
    if (!s)
        return;
    s->quality_override = q;
}

uint8_t rogue_weapon_collision_get_quality_override(const RogueWeaponCollisionState* s)
{
    if (!s)
        return 0u;
    return s->quality_override;
}
