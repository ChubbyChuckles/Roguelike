/* test_enemy_collision_opt.c - Milestone 2.3 initial scaffolding tests
 * Validates deterministic profile analysis & batch aggregation helpers.
 */
#include "game/enemy_collision_opt.h"
#include <stdio.h>
#include <string.h>

static int test_profile_basic(void)
{
    RogueEnemyCollisionProfile p;
    memset(&p, 0, sizeof(p));
    rogue_enemy_collision_profile_analyze(&p, 10.f, 20.f,
                                          50); /* 10x20 area=200 -> density=0.25 fill=0.25 */
    fprintf(stderr, "DEBUG complexity=%u radius=%.3f aspect=%.3f fill=%.3f sym=%.3f bias=%d\n",
            p.complexity_level, p.radius_estimate, p.aspect_ratio, p.fill_ratio, p.symmetry_score,
            rogue_enemy_collision_profile_get_lod_bias(&p));
    if (p.complexity_level != 1 && p.complexity_level != 2) /* elongated bump may push to 2 */
    {
        fprintf(stderr, "expected complexity 1 or 2 got %u\n", p.complexity_level);
        return 1;
    }
    if (p.radius_estimate <= 0.f)
    {
        fprintf(stderr, "radius_estimate not set\n");
        return 2;
    }
    int bias = rogue_enemy_collision_profile_get_lod_bias(&p);
    if (bias != 0)
    {
        fprintf(stderr, "lod bias expected 0 got %d\n", bias);
        return 3;
    }
    rogue_enemy_collision_profile_set_lod_bias(&p, -9); /* clamp */
    if (rogue_enemy_collision_profile_get_lod_bias(&p) != -8)
    {
        fprintf(stderr, "lod bias clamp low failed\n");
        return 4;
    }
    rogue_enemy_collision_profile_set_lod_bias(&p, 9);
    if (rogue_enemy_collision_profile_get_lod_bias(&p) != 7)
    {
        fprintf(stderr, "lod bias clamp high failed\n");
        return 5;
    }
    if (p.aspect_ratio < 1.9f || p.aspect_ratio > 2.2f)
    {
        fprintf(stderr, "aspect_ratio unexpected %.3f\n", p.aspect_ratio);
        return 6;
    }
    if (p.fill_ratio < 0.24f || p.fill_ratio > 0.26f)
    {
        fprintf(stderr, "fill_ratio unexpected %.3f\n", p.fill_ratio);
        return 7;
    }
    if (p.symmetry_score < 0.f || p.symmetry_score > 1.f)
    {
        fprintf(stderr, "symmetry_score out of range %.3f\n", p.symmetry_score);
        return 8;
    }
    if (p.method_hint > 2)
    {
        fprintf(stderr, "method_hint unexpected %u\n", p.method_hint);
        return 9;
    }
    if (p.cycles_estimate == 0)
    {
        fprintf(stderr, "cycles_estimate not populated\n");
        return 11;
    }
    return 0;
}

static int test_batch_basic(void)
{
    RogueEnemyCollisionBatch b;
    rogue_enemy_collision_batch_reset(&b);
    float xs[4] = {0.f, 3.f, 6.f, 9.f};
    float ys[4] = {0.f, 0.f, 0.f, 0.f};
    float rs[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    for (int i = 0; i < 4; ++i)
    {
        if (rogue_enemy_collision_batch_add(&b, (uint16_t) i, xs[i], ys[i], rs[i]))
            return 10 + i;
    }
    if (b.batch_size != 4)
    {
        fprintf(stderr, "batch_size expected 4 got %u\n", b.batch_size);
        return 20;
    }
    rogue_enemy_collision_batch_finalize(&b, xs, ys, rs);
    if (b.centroid_x < 4.4f || b.centroid_x > 4.6f)
    {
        fprintf(stderr, "centroid_x unexpected %.3f\n", b.centroid_x);
        return 21;
    }
    if (b.batch_radius <= 0.5f)
    {
        fprintf(stderr, "batch_radius not expanded %.3f\n", b.batch_radius);
        return 22;
    }
    return 0;
}

int main(void)
{
    int r = 0;
    if ((r = test_profile_basic()) != 0)
        return r;
    if ((r = test_batch_basic()) != 0)
        return r;
    /* LOD adaptation smoke test */
    RogueEnemyCollisionProfile p2;
    memset(&p2, 0, sizeof(p2));
    rogue_enemy_collision_profile_analyze(&p2, 16.f, 16.f, 200);
    rogue_enemy_collision_profile_adapt_lod(&p2, 5.f, 8000.f, 0.8f, 3.f);
    int bias_close = rogue_enemy_collision_profile_get_lod_bias(&p2);
    rogue_enemy_collision_profile_adapt_lod(&p2, 120.f, 100.f, 0.f, 0.f);
    int bias_far = rogue_enemy_collision_profile_get_lod_bias(&p2);
    if (!(bias_close < bias_far))
    {
        fprintf(stderr, "LOD adapt ordering failed close=%d far=%d\n", bias_close, bias_far);
        return 30;
    }
    return 0; /* success */
}
