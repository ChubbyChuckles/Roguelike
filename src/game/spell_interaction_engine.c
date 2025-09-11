/* spell_interaction_engine.c - Milestone 3.3 baseline implementation */
#include "game/spell_interaction_engine.h"
#include <math.h>
#include <string.h>

static inline float dist2(float ax, float ay, float bx, float by)
{
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

int rogue_spell_interaction_check_proximity(const RogueSpellInteractionRule* rule, float ax,
                                            float ay, float bx, float by)
{
    if (!rule)
        return 0;
    float r = (rule->interaction_radius <= 0.f) ? 0.f : rule->interaction_radius;
    return dist2(ax, ay, bx, by) <= r * r;
}

static int ids_match(uint32_t a, uint32_t b, uint32_t x, uint32_t y)
{
    return (a == x && b == y) || (a == y && b == x);
}

int rogue_spell_interaction_execute(const RogueSpellInteractionRule* rules, uint32_t rule_count,
                                    uint32_t spell_a_id, uint32_t spell_b_id, float ax, float ay,
                                    float bx, float by, float rng01,
                                    RogueSpellInteractionRule* out_matched_rule,
                                    RogueSpellInteractionType* out_type,
                                    uint32_t* out_result_effect_id,
                                    float* out_result_intensity_mult,
                                    bool* out_consumes_original_effects)
{
    if (!rules || rule_count == 0)
        return 0;
    for (uint32_t i = 0; i < rule_count; ++i)
    {
        const RogueSpellInteractionRule* r = &rules[i];
        if (!ids_match(r->spell_a_id, r->spell_b_id, spell_a_id, spell_b_id))
            continue;
        if (!rogue_spell_interaction_check_proximity(r, ax, ay, bx, by))
            continue;
        float p = r->probability;
        if (p < 0.f)
            p = 0.f;
        if (p > 1.f)
            p = 1.f;
        /* Trigger only when rng01 < p (strict). Ensures p=0 never triggers, p=1 triggers for rng<1
         */
        if (!(rng01 < p))
            continue; /* failed probabilistic gate */
        if (out_matched_rule)
            *out_matched_rule = *r;
        if (out_type)
            *out_type = r->type;
        if (out_result_effect_id)
            *out_result_effect_id = r->result_effect_id;
        if (out_result_intensity_mult)
            *out_result_intensity_mult = r->result_intensity_mult;
        if (out_consumes_original_effects)
            *out_consumes_original_effects = r->consumes_original_effects;
        return 1;
    }
    return 0;
}
