#include "core/damage_calc.h"
#include "core/app_state.h"
#include "core/progression/progression_synergy.h"
#include "core/stat_cache.h"
#include <math.h>

int rogue_damage_fireball(int fireball_skill_id)
{
    const RogueSkillDef* def = rogue_skill_get_def(fireball_skill_id);
    const RogueSkillState* st = rogue_skill_get_state(fireball_skill_id);
    if (!def || !st || st->rank <= 0)
        return 0;
    int base = 3 + st->rank * 2;
    int fire_synergy = rogue_skill_synergy_total(ROGUE_SYNERGY_FIRE_POWER);
    /* Phase 3.6.4: effect scaling with progression stats. For Fireball, scale with INT. */
    int int_bonus = 0;
    /* Ensure stat cache reflects current player totals; caller typically maintains it. */
    int_bonus = (int) (g_player_stat_cache.total_intelligence * 0.25f);
    if (int_bonus < 0)
        int_bonus = 0;
    return base + fire_synergy + int_bonus;
}

float rogue_cooldown_fireball_ms(int fireball_skill_id)
{
    const RogueSkillDef* def = rogue_skill_get_def(fireball_skill_id);
    const RogueSkillState* st = rogue_skill_get_state(fireball_skill_id);
    if (!def || !st || st->rank <= 0)
        return 0.0f;
    float cd = def->base_cooldown_ms - (st->rank - 1) * def->cooldown_reduction_ms_per_rank;
    /* Apply progression-based cooldown reduction soft/hard caps using haste-effective percent. */
    float cdr_eff = rogue_progression_final_cdr((float) g_player_stat_cache.rating_haste_eff_pct);
    if (cdr_eff > 0.0f && cdr_eff < 100.0f)
    {
        float mult = 1.0f - (cdr_eff / 100.0f);
        if (mult < 0.10f)
            mult = 0.10f; /* never below 90% reduction via this path */
        cd *= mult;
    }
    if (cd < 100.0f)
        cd = 100.0f;
    return cd;
}
