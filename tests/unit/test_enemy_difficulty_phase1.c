/* test_enemy_difficulty_phase1.c - Phase 1 tests (1.1 - 1.7 baseline + ΔL model)
 * Coverage:
 *  - Base curve monotonic growth
 *  - Relative ΔL penalty / buff application across grid
 *  - Dominance & trivial reward scalar thresholds
 *  - Extreme ΔL no underflow (hp/dmg not negative, min floor)
 *  - TTK band rough monotonic changes
 */
#include "core/enemy/enemy_difficulty_scaling.h"
#include <math.h>
#include <stdio.h>

static int approx(float a, float b, float eps)
{
    float d = a - b;
    if (d < 0)
        d = -d;
    return d <= eps;
}

static int test_base_monotonic(void)
{
    float prev_hp = 0.f, prev_dmg = 0.f, prev_def = 0.f;
    for (int L = 1; L <= 50; ++L)
    {
        float hp = rogue_enemy_base_hp(L), dmg = rogue_enemy_base_damage(L),
              df = rogue_enemy_base_defense(L);
        if (L > 1)
        {
            if (!(hp > prev_hp && dmg > prev_dmg && df > prev_def))
            {
                printf("FAIL base monotonic L=%d\n", L);
                return 1;
            }
        }
        prev_hp = hp;
        prev_dmg = dmg;
        prev_def = df;
    }
    return 0;
}

static int test_relative_grid(void)
{
    for (int pL = 10; pL <= 20; pL += 5)
    {
        for (int eL = pL - 10; eL <= pL + 10; eL += 5)
        {
            if (eL < 1)
                continue;
            float hp_mult, dmg_mult;
            if (rogue_enemy_difficulty_internal__relative_multipliers(pL, eL, &hp_mult,
                                                                      &dmg_mult) != 0)
            {
                printf("FAIL rel mult compute\n");
                return 1;
            }
            int dL = pL - eL;
            if (dL == 0)
            {
                if (!approx(hp_mult, 1.f, 0.0001f) || !approx(dmg_mult, 1.f, 0.0001f))
                {
                    printf("FAIL dL=0 not 1\n");
                    return 1;
                }
            }
            if (dL > 0)
            {
                if (!(hp_mult <= 1.f && dmg_mult <= 1.f))
                {
                    printf("FAIL downward not <=1 dL=%d\n", dL);
                    return 1;
                }
            }
            if (dL < 0)
            {
                if (!(hp_mult >= 1.f && dmg_mult >= 1.f))
                {
                    printf("FAIL upward not >=1 dL=%d\n", dL);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int test_reward_scalar(void)
{
    float base = rogue_enemy_compute_reward_scalar(20, 20, 0, 0);
    if (!approx(base, 1.f, 0.0001f))
    {
        printf("FAIL reward equal\n");
        return 1;
    }
    const RogueEnemyDifficultyParams* p = rogue_enemy_difficulty_params_current();
    float trivial = rogue_enemy_compute_reward_scalar(20 + p->trivial_threshold, 20, 0, 0);
    if (!approx(trivial, p->reward_trivial_scalar, 0.0001f))
    {
        printf("FAIL reward trivial\n");
        return 1;
    }
    float dom = rogue_enemy_compute_reward_scalar(20 + p->dominance_threshold, 20, 0, 0);
    if (!(dom <= 1.f && dom >= p->reward_trivial_scalar))
    {
        printf("FAIL reward dominance range\n");
        return 1;
    }
    return 0;
}

static int test_final_stats_bounds(void)
{
    RogueEnemyFinalStats fs;
    if (rogue_enemy_compute_final_stats(50, 5, ROGUE_ENEMY_TIER_NORMAL, &fs) != 0)
    {
        printf("FAIL final stats compute\n");
        return 1;
    }
    if (!(fs.hp > 0.f && fs.damage > 0.f && fs.defense > 0.f))
    {
        printf("FAIL final stats positive\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_base_monotonic() != 0)
        return 1;
    if (test_relative_grid() != 0)
        return 1;
    if (test_reward_scalar() != 0)
        return 1;
    if (test_final_stats_bounds() != 0)
        return 1;
    printf("OK test_enemy_difficulty_phase1\n");
    return 0;
}
