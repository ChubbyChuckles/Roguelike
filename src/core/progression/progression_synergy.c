#include "core/progression/progression_synergy.h"

float rogue_progression_layered_damage(float base_flat, float equipment_pct, float passive_pct,
                                       float mastery_pct, float micro_pct)
{
    /* Each pct param expressed as percentage (e.g. 25 == +25%). Chain multiplicatively in canonical
     * order. */
    float dmg = base_flat;
    if (dmg < 0.f)
        dmg = 0.f;
    float eq = 1.f + equipment_pct / 100.f;
    float pa = 1.f + passive_pct / 100.f;
    float ma = 1.f + mastery_pct / 100.f;
    float mi = 1.f + micro_pct / 100.f;
    dmg *= eq * pa * ma * mi;
    if (dmg < 0.f)
        dmg = 0.f;
    return dmg;
}

int rogue_progression_layered_strength(int base_val, int equipment_bonus, int passive_bonus,
                                       int mastery_bonus, int micro_bonus)
{
    long total = (long) base_val + equipment_bonus + passive_bonus + mastery_bonus + micro_bonus;
    if (total < 0)
        total = 0;
    if (total > 100000)
        total = 100000; /* sanity clamp */
    return (int) total;
}

int rogue_progression_final_crit_chance(int flat_crit_percent)
{
    if (flat_crit_percent < 0)
        flat_crit_percent = 0;
    /* Hard cap 95%, soft cap 60%: apply diminishing beyond soft cap. */
    const int soft = 60;
    const int hard = 95;
    const float softness = 0.55f;
    if (flat_crit_percent <= soft)
        return flat_crit_percent;
    int over = flat_crit_percent - soft;
    float adj = (float) soft + over / (1.f + over / (soft * softness));
    int final = (int) (adj + 0.5f);
    if (final > hard)
        final = hard;
    return final;
}

float rogue_progression_final_cdr(float raw_cdr_percent)
{
    /* Cap total cooldown reduction at 70%; apply soft cap 50% with smooth curve. */
    if (raw_cdr_percent < 0.f)
        raw_cdr_percent = 0.f;
    if (raw_cdr_percent > 200.f)
        raw_cdr_percent = 200.f; /* sanity */
    const float soft = 50.f;
    const float hard = 70.f;
    const float softness = 0.60f;
    if (raw_cdr_percent <= soft)
        return raw_cdr_percent;
    float over = raw_cdr_percent - soft;
    float adj = soft + over / (1.f + over / (soft * softness));
    if (adj > hard)
        adj = hard;
    return adj;
}

int rogue_progression_synergy_tag_mask(const RoguePlayer* p)
{
    if (!p)
        return 0;
    int mask = 0;
    switch (p->weapon_infusion)
    {
    case 1:
        mask |= ROGUE_SKILL_TAG_FIRE;
        break;
    case 2:
        mask |= ROGUE_SKILL_TAG_FROST;
        break;
    case 3:
        mask |= ROGUE_SKILL_TAG_ARCANE;
        break;
    default:
        break;
    }
    return mask;
}

int rogue_progression_synergy_fire_bonus(int tag_mask, int passive_fire_bonus)
{
    if (!(tag_mask & ROGUE_SKILL_TAG_FIRE))
        return 0;
    if (passive_fire_bonus < 0)
        return 0;
    return passive_fire_bonus;
}
