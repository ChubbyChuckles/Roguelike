/* test_weapon_collision_advanced_sweep.c - Milestone 2.2 advanced sweep tests
 * Validates quality-tied resampling of sweep bounds.
 */
#include "game/weapon_collision_advanced.h"
#include <math.h>
#include <stdio.h>

static int floats_close(float a, float b) { return fabsf(a - b) < 0.0005f; }

int main(void)
{
    RogueWeaponCollisionState w;
    rogue_weapon_collision_state_init(&w, 100.f);

    /* Build a simple L-shape motion: (0,0)->(10,0)->(10,10) */
    float t = 0.f;
    rogue_weapon_collision_compute_transform(&w, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f);
    rogue_weapon_trail_add(&w.trail, 0.f, 0.f, t);
    t += 10.f;
    rogue_weapon_collision_compute_transform(&w, 10.f, 0.f, 0.f, 1.f, 0.f, 0.f);
    rogue_weapon_trail_add(&w.trail, 10.f, 0.f, t);
    t += 10.f;
    rogue_weapon_collision_compute_transform(&w, 10.f, 10.f, 0.f, 1.f, 0.f, 0.f);
    rogue_weapon_trail_add(&w.trail, 10.f, 10.f, t);

    float minx, miny, maxx, maxy;

    /* FAST mode (q=1): endpoints only */
    rogue_weapon_collision_set_quality_override(&w, 1);
    if (!rogue_weapon_collision_sweep_bounds_advanced(&w, 0.0f, t, &minx, &miny, &maxx, &maxy))
    {
        fprintf(stderr, "advanced sweep failed (FAST)\n");
        return 1;
    }
    if (!floats_close(minx, 0.f) || !floats_close(miny, 0.f) || !floats_close(maxx, 10.f) ||
        !floats_close(maxy, 10.f))
    {
        fprintf(stderr, "FAST bounds mismatch: %f %f %f %f\n", minx, miny, maxx, maxy);
        return 2;
    }

    /* PRECISE mode (q=3): includes interior resamples but bounds stay identical for L-shape */
    rogue_weapon_collision_set_quality_override(&w, 3);
    if (!rogue_weapon_collision_sweep_bounds_advanced(&w, 1.5f, t, &minx, &miny, &maxx, &maxy))
    {
        fprintf(stderr, "advanced sweep failed (PRECISE)\n");
        return 3;
    }
    if (!floats_close(minx, -1.5f) || !floats_close(miny, -1.5f) || !floats_close(maxx, 11.5f) ||
        !floats_close(maxy, 11.5f))
    {
        fprintf(stderr, "PRECISE+radius mismatch: %f %f %f %f\n", minx, miny, maxx, maxy);
        return 4;
    }

    return 0;
}
