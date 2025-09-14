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
