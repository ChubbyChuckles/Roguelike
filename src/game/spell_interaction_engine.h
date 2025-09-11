/* spell_interaction_engine.h - Milestone 3.3: Spell Interaction System (baseline)
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RogueSpellInteractionType
    {
        ROGUE_INTERACT_NONE = 0,
        ROGUE_INTERACT_CANCEL = 1,
        ROGUE_INTERACT_COMBINE = 2,
        ROGUE_INTERACT_AMPLIFY = 3,
        ROGUE_INTERACT_TRANSFORM = 4,
        ROGUE_INTERACT_CHAIN = 5
    } RogueSpellInteractionType;

    typedef struct RogueSpellInteractionRule
    {
        uint32_t spell_a_id;
        uint32_t spell_b_id;
        RogueSpellInteractionType type;
        float interaction_radius;       /* max distance for interaction */
        float probability;              /* 0..1 chance */
        uint32_t result_effect_id;      /* new effect created (if any) */
        float result_intensity_mult;    /* multiplier applied to result */
        bool consumes_original_effects; /* whether originals are consumed */
    } RogueSpellInteractionRule;

    /* Return 1 if distance between (ax,ay) and (bx,by) <= interaction_radius. */
    int rogue_spell_interaction_check_proximity(const RogueSpellInteractionRule* rule, float ax,
                                                float ay, float bx, float by);

    /* Execute interaction if a matching rule exists and constraints pass.
     * - rules/rule_count: rule table to scan.
     * - spell_a_id/spell_b_id: participating spells (order-insensitive).
     * - ax,ay,bx,by: effect positions.
     * - rng01: deterministic [0,1] value compared to rule.probability (pass <=prob to trigger).
     * Outputs (optional): out_matched_rule (copy), out_type, out_result_effect_id,
     * out_result_intensity_mult, out_consumes_original_effects.
     * Returns 1 if interaction executed, 0 otherwise. */
    int rogue_spell_interaction_execute(const RogueSpellInteractionRule* rules, uint32_t rule_count,
                                        uint32_t spell_a_id, uint32_t spell_b_id, float ax,
                                        float ay, float bx, float by, float rng01,
                                        RogueSpellInteractionRule* out_matched_rule,
                                        RogueSpellInteractionType* out_type,
                                        uint32_t* out_result_effect_id,
                                        float* out_result_intensity_mult,
                                        bool* out_consumes_original_effects);

#ifdef __cplusplus
}
#endif
