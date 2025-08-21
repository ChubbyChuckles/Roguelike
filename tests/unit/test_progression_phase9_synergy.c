#include "core/progression/progression_synergy.h"
#include <stdio.h>
#include <string.h>

static int test_layered_damage(void)
{
    float base = 100.f;
    float dmg = rogue_progression_layered_damage(base, 25.f, 10.f, 5.f,
                                                 2.f); /* (1.25*1.10*1.05*1.02)=~1.4786 */
    if (dmg < 147.0f || dmg > 148.5f)
    {
        printf("fail_layered_damage %.3f\n", dmg);
        return 1;
    }
    return 0;
}

static int test_strength(void)
{
    int s = rogue_progression_layered_strength(10, 5, 4, 3, 2);
    if (s != 24)
    {
        printf("fail_strength %d\n", s);
        return 1;
    }
    return 0;
}

static int test_caps(void)
{
    int crit_a = rogue_progression_final_crit_chance(55);
    if (crit_a != 55)
    {
        printf("fail_crit_pre %d\n", crit_a);
        return 1;
    }
    int crit_b = rogue_progression_final_crit_chance(120);
    if (crit_b > 95 || crit_b < 80)
    {
        printf("fail_crit_post %d\n", crit_b);
        return 1;
    }
    float cdr_a = rogue_progression_final_cdr(40.f);
    if (cdr_a < 39.f || cdr_a > 41.f)
    {
        printf("fail_cdr_pre %.2f\n", cdr_a);
        return 1;
    }
    float cdr_b = rogue_progression_final_cdr(120.f);
    if (cdr_b > 70.5f || cdr_b < 60.f)
    {
        printf("fail_cdr_post %.2f\n", cdr_b);
        return 1;
    }
    return 0;
}

static int test_tag_synergy(void)
{
    RoguePlayer p;
    memset(&p, 0, sizeof p);
    p.weapon_infusion = 1;
    int mask = rogue_progression_synergy_tag_mask(&p);
    if (!(mask & ROGUE_SKILL_TAG_FIRE))
    {
        printf("fail_mask_fire %d\n", mask);
        return 1;
    }
    int bonus = rogue_progression_synergy_fire_bonus(mask, 15);
    if (bonus != 15)
    {
        printf("fail_fire_bonus %d\n", bonus);
        return 1;
    }
    p.weapon_infusion = 0;
    mask = rogue_progression_synergy_tag_mask(&p);
    bonus = rogue_progression_synergy_fire_bonus(mask, 15);
    if (bonus != 0)
    {
        printf("fail_fire_bonus_absent %d\n", bonus);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_layered_damage())
        return 1;
    if (test_strength())
        return 1;
    if (test_caps())
        return 1;
    if (test_tag_synergy())
        return 1;
    printf("progression_phase9_synergy: OK\n");
    return 0;
}
