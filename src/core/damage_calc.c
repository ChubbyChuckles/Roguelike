#include "core/damage_calc.h"
#include "core/app_state.h"
#include <math.h>

int rogue_damage_fireball(int fireball_skill_id)
{
    const RogueSkillDef* def = rogue_skill_get_def(fireball_skill_id);
    const RogueSkillState* st = rogue_skill_get_state(fireball_skill_id);
    if (!def || !st || st->rank <= 0)
        return 0;
    int base = 3 + st->rank * 2;
    int fire_synergy = rogue_skill_synergy_total(ROGUE_SYNERGY_FIRE_POWER);
    return base + fire_synergy;
}

float rogue_cooldown_fireball_ms(int fireball_skill_id)
{
    const RogueSkillDef* def = rogue_skill_get_def(fireball_skill_id);
    const RogueSkillState* st = rogue_skill_get_state(fireball_skill_id);
    if (!def || !st || st->rank <= 0)
        return 0.0f;
    float cd = def->base_cooldown_ms - (st->rank - 1) * def->cooldown_reduction_ms_per_rank;
    if (cd < 100.0f)
        cd = 100.0f;
    return cd;
}
