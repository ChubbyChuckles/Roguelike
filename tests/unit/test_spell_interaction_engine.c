/* test_spell_interaction_engine.c - Milestone 3.3 baseline tests */
#include "game/spell_interaction_engine.h"
#include <stdio.h>

static int fail(const char* m)
{
    fprintf(stderr, "%s\n", m);
    return 1;
}

int main(void)
{
    RogueSpellInteractionRule rules[3] = {0};
    rules[0].spell_a_id = 1;
    rules[0].spell_b_id = 2;
    rules[0].type = ROGUE_INTERACT_COMBINE;
    rules[0].interaction_radius = 5.f;
    rules[0].probability = 1.f;
    rules[0].result_effect_id = 100;
    rules[0].result_intensity_mult = 1.5f;
    rules[0].consumes_original_effects = true;
    rules[1].spell_a_id = 2;
    rules[1].spell_b_id = 3;
    rules[1].type = ROGUE_INTERACT_AMPLIFY;
    rules[1].interaction_radius = 3.f;
    rules[1].probability = 0.0f;
    rules[1].result_effect_id = 200;
    rules[1].result_intensity_mult = 2.0f;
    rules[1].consumes_original_effects = false;
    rules[2].spell_a_id = 3;
    rules[2].spell_b_id = 1;
    rules[2].type = ROGUE_INTERACT_CANCEL;
    rules[2].interaction_radius = 10.f;
    rules[2].probability = 0.5f;
    rules[2].result_effect_id = 0;
    rules[2].result_intensity_mult = 0.0f;
    rules[2].consumes_original_effects = true;

    /* Proximity check */
    if (!rogue_spell_interaction_check_proximity(&rules[0], 0, 0, 3, 4))
        return fail("prox fail (3-4 within 5)");
    if (rogue_spell_interaction_check_proximity(&rules[1], 0, 0, 4, 0))
        return fail("prox false positive");

    /* Deterministic execution: rule[0] should match regardless of id order, prob=1 */
    RogueSpellInteractionRule matched = {0};
    RogueSpellInteractionType type = ROGUE_INTERACT_NONE;
    uint32_t rid = 0;
    float mult = 0.f;
    bool consume = false;
    if (!rogue_spell_interaction_execute(rules, 3, 1, 2, 0, 0, 3, 4, 0.3f, &matched, &type, &rid,
                                         &mult, &consume))
        return fail("exec should match rule0");
    if (type != ROGUE_INTERACT_COMBINE || rid != 100 || mult < 1.49f || mult > 1.51f || !consume)
        return fail("exec output mismatch rule0");

    /* Order-insensitive */
    if (!rogue_spell_interaction_execute(rules, 3, 2, 1, 0, 0, 3, 4, 0.9f, NULL, &type, &rid, &mult,
                                         &consume))
        return fail("exec order-insensitive fail");

    /* Probability gate: rule[1] prob=0 -> never triggers */
    if (rogue_spell_interaction_execute(rules, 3, 2, 3, 0, 0, 0, 0, 0.0f, NULL, &type, &rid, &mult,
                                        &consume))
        return fail("prob zero should not trigger");

    /* 50% gate: with rng01=0.6 should NOT trigger; with 0.4 should trigger (proximity satisfied) */
    if (rogue_spell_interaction_execute(rules, 3, 3, 1, 0, 0, 6, 8, 0.6f, NULL, &type, &rid, &mult,
                                        &consume))
        return fail("prob 0.5, rng 0.6 should not");
    if (!rogue_spell_interaction_execute(rules, 3, 3, 1, 0, 0, 6, 8, 0.4f, NULL, &type, &rid, &mult,
                                         &consume))
        return fail("prob 0.5, rng 0.4 should trigger");

    return 0;
}
