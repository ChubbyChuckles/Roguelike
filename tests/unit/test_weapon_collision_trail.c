/* test_weapon_collision_trail.c - Milestone 2.2 initial weapon collision tests
 * Validates:
 *  - Transform computation (translation + rotation + scale) affects matrix components
 *  - Trail buffering keeps only recent samples within duration window
 *  - Coarse trail AABB encloses all recorded points
 *  - Layer mask filter logic
 */
#include "game/weapon_collision_advanced.h"
#include <stdio.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <math.h>

static int floats_close(float a, float b) { return fabsf(a - b) < 0.0005f; }

int main(void)
{
    RogueWeaponCollisionState w;
    rogue_weapon_collision_state_init(&w, 50.f); /* 50ms window */
    /* initial transform */
    rogue_weapon_collision_compute_transform(&w, 10.f, 20.f, 0.f, 1.f, 0.f, 0.f);
    if (!floats_close(w.transform_matrix.m[2], 10.f) ||
        !floats_close(w.transform_matrix.m[5], 20.f))
    {
        fprintf(stderr, "Translation failed %f %f\n", w.transform_matrix.m[2],
                w.transform_matrix.m[5]);
        return 1;
    }
    /* rotate 90deg around origin then translate */
    rogue_weapon_collision_compute_transform(&w, 0.f, 0.f, (float) M_PI * 0.5f, 2.f, 0.f, 0.f);
    float c = cosf((float) M_PI * 0.5f);
    float s = sinf((float) M_PI * 0.5f);
    if (!floats_close(w.transform_matrix.m[0], c) || !floats_close(w.transform_matrix.m[3], s))
    {
        fprintf(stderr, "Rotation components mismatch\n");
        return 2;
    }
    /* trail sampling */
    for (int i = 0; i < 20; ++i)
    {
        float t = (float) (i * 10); /* ms */
        /* simulate movement along x=y=i */
        rogue_weapon_collision_compute_transform(&w, (float) i, (float) i, 0.f, 1.f, 1.f, 1.f);
        rogue_weapon_trail_add(&w.trail, (float) i, (float) i, t);
    }
    if (w.trail.sample_count > 16)
    {
        fprintf(stderr, "Trail exceeded capacity %u\n", w.trail.sample_count);
        return 3;
    }
    /* ensure old samples (> window) pruned, last timestamp should be 190 */
    if (w.trail.sample_count == 0 || w.trail.timestamps[w.trail.sample_count - 1] != 190.f)
    {
        fprintf(stderr, "Unexpected last timestamp %f count=%u\n",
                w.trail.timestamps[w.trail.sample_count - 1], w.trail.sample_count);
        return 4;
    }
    float minx, miny, maxx, maxy;
    if (!rogue_weapon_collision_trail_aabb(&w, &minx, &miny, &maxx, &maxy))
    {
        return 5;
    }
    if (minx < 4.f - 0.01f)
    {
        fprintf(stderr, "Min prune failed %f\n", minx);
        return 6;
    }
    if (fabsf(maxx - (float) (19)) > 0.01f)
    {
        fprintf(stderr, "Max incorrect %f\n", maxx);
        return 7;
    }
    /* layer mask */
    uint32_t weapon_mask = 0b0101;
    uint32_t cand_good = 0b1001;
    uint32_t cand_bad = 0b0010;
    if (!rogue_weapon_candidate_layer_match(weapon_mask, cand_good))
    {
        fprintf(stderr, "Layer match false negative\n");
        return 8;
    }
    if (rogue_weapon_candidate_layer_match(weapon_mask, cand_bad))
    {
        fprintf(stderr, "Layer match false positive\n");
        return 9;
    }
    return 0;
}
