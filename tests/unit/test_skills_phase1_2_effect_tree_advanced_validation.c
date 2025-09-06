#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_coeffs.h"
#include "../../src/core/skills/skills_validate.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int reg_basic(void)
{
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_STAT_BUFF;
    es.magnitude = 1;
    return rogue_effect_register(&es);
}

int main(void)
{
    /* Case 1: duration < repeat_count * repeat_interval (should fail) */
    rogue_skills_init();
    rogue_effect_reset();
    int eff = reg_basic();
    assert(eff >= 0);
    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = "TreeBadWindow";
    def.max_rank = 1;
    def.effect_spec_id = eff;
    def.effect_tree_node_count = 1;
    def.effect_tree_nodes[0].effect_spec_id = eff;
    def.effect_tree_nodes[0].repeat_count = 3;            /* 3 repeats after first */
    def.effect_tree_nodes[0].repeat_interval_ms = 100.0f; /* needs at least 300ms window */
    def.effect_tree_nodes[0].duration_ms = 250.0f;        /* insufficient */
    def.effect_tree_nodes[0].parent_index = -1;
    int sid = rogue_skill_register(&def);
    assert(sid == 0);
    char err[256];
    int r = rogue_skills_validate_all(err, (int) sizeof err);
    assert(r == -1);
    rogue_skills_shutdown();
    rogue_effect_reset();
    printf("EFFECT_TREE_WINDOW_INSUFFICIENT_REJECT_OK\n");

    /* Case 2: excessive span > 60000ms (delay chain + repeats) */
    rogue_skills_init();
    rogue_effect_reset();
    eff = reg_basic();
    assert(eff >= 0);
    memset(&def, 0, sizeof def);
    def.name = "TreeTooLong";
    def.max_rank = 1;
    def.effect_spec_id = eff;
    def.effect_tree_node_count = 2;
    def.effect_tree_nodes[0].effect_spec_id = eff;
    def.effect_tree_nodes[0].parent_index = -1;
    def.effect_tree_nodes[0].delay_ms = 1000.0f;
    def.effect_tree_nodes[1].effect_spec_id = eff;
    def.effect_tree_nodes[1].parent_index = 0;
    def.effect_tree_nodes[1].delay_ms = 59000.0f;
    def.effect_tree_nodes[1].repeat_count = 1;
    def.effect_tree_nodes[1].repeat_interval_ms =
        2000.0f; /* start(1000+59000)=60000, span=2000 -> end=62000 > 60000 */
    sid = rogue_skill_register(&def);
    assert(sid == 0);
    r = rogue_skills_validate_all(err, (int) sizeof err);
    assert(r == -1);
    rogue_skills_shutdown();
    rogue_effect_reset();
    printf("EFFECT_TREE_SPAN_LIMIT_REJECT_OK\n");

    /* Case 3: valid tree passes */
    rogue_skills_init();
    rogue_effect_reset();
    eff = reg_basic();
    assert(eff >= 0);
    memset(&def, 0, sizeof def);
    def.name = "TreeGood";
    def.max_rank = 1;
    def.effect_spec_id = eff;
    def.effect_tree_node_count = 2;
    def.effect_tree_nodes[0].effect_spec_id = eff;
    def.effect_tree_nodes[0].parent_index = -1;
    def.effect_tree_nodes[0].delay_ms = 0.0f;
    def.effect_tree_nodes[0].repeat_count = 2;
    def.effect_tree_nodes[0].repeat_interval_ms = 100.0f;
    def.effect_tree_nodes[0].duration_ms = 220.0f; /* >=200 */
    def.skill_type = ROGUE_SKTYPE_BUFF;
    def.effect_tree_nodes[1].effect_spec_id = eff;
    def.effect_tree_nodes[1].parent_index = 0;
    def.effect_tree_nodes[1].delay_ms = 50.0f;
    def.effect_tree_nodes[1].repeat_count = 0;
    def.effect_tree_nodes[1].duration_ms = 0.0f; /* single trigger */
    sid = rogue_skill_register(&def);
    assert(sid == 0);
    /* Provide coeff for offensive skill */
    RogueSkillCoeffParams cp;
    memset(&cp, 0, sizeof cp);
    cp.base_scalar = 1.0f;
    cp.per_rank_scalar = 0.0f;
    assert(rogue_skill_coeff_register(sid, &cp) == 0);
    r = rogue_skills_validate_all(err, (int) sizeof err);
    assert(r == 0);
    rogue_skills_shutdown();
    rogue_effect_reset();
    printf("EFFECT_TREE_ADVANCED_VALIDATION_PASS_OK\n");
    return 0;
}
