#include "core/damage_calc.h"
#include "core/app_state.h"
#include "core/progression/progression_mastery.h"
#include "core/progression/progression_specialization.h"
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
    int total = base + fire_synergy + int_bonus;
    /* Phase 3.6.5: integrate mastery bonuses into active skill output. */
    {
        float ms = 1.0f;
        /* mastery scalar is >= 1.0f; multiply and round to nearest int for stability */
        ms = rogue_mastery_bonus_scalar(fireball_skill_id);
        if (ms > 1.0f)
        {
            float scaled = (float) total * ms;
            if (scaled < 0.0f)
                scaled = 0.0f;
            total = (int) (scaled + 0.5f);
        }
    }
    /* Phase 3.6.6: apply specialization path (POWER) scalar. */
    {
        float ds = rogue_specialization_damage_scalar(fireball_skill_id);
        if (ds != 1.0f)
        {
            float scaled = (float) total * ds;
            if (scaled < 0.0f)
                scaled = 0.0f;
            total = (int) (scaled + 0.5f);
        }
    }
    return total;
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
    /* Phase 3.6.6: specialization CONTROL reduces cooldown multiplicatively. */
    {
        float cms = rogue_specialization_cooldown_scalar(fireball_skill_id);
        if (cms > 0.0f && cms != 1.0f)
            cd *= cms;
    }
    if (cd < 100.0f)
        cd = 100.0f;
    return cd;
}
