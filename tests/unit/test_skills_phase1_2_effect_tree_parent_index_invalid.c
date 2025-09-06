#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_validate.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int reg_eff(void)
{
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_STAT_BUFF;
    es.buff_type = 0;
    es.magnitude = 1;
    return rogue_effect_register(&es);
}

int main(void)
{
    /* Out-of-range parent_index */
    rogue_skills_init();
    rogue_effect_reset();
    int eff = reg_eff();
    assert(eff >= 0);
    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = "TreeBadParentRange";
    def.max_rank = 1;
    def.effect_spec_id = eff;
    def.effect_tree_node_count = 2;
    def.effect_tree_nodes[0].effect_spec_id = eff;
    def.effect_tree_nodes[0].parent_index = -1;
    def.effect_tree_nodes[1].effect_spec_id = eff;
    def.effect_tree_nodes[1].parent_index = 5; /* invalid >=count */
    int sid = rogue_skill_register(&def);
    assert(sid == 0);
    char err[256];
    int r = rogue_skills_validate_all(err, (int) sizeof err);
    assert(r == -1);
    rogue_skills_shutdown();
    rogue_effect_reset();
    printf("EFFECT_TREE_PARENT_INDEX_RANGE_REJECT_OK\n");

    /* Self-parent (cycle) */
    rogue_skills_init();
    rogue_effect_reset();
    eff = reg_eff();
    assert(eff >= 0);
    memset(&def, 0, sizeof def);
    def.name = "TreeBadParentSelf";
    def.max_rank = 1;
    def.effect_spec_id = eff;
    def.effect_tree_node_count = 1;
    def.effect_tree_nodes[0].effect_spec_id = eff;
    def.effect_tree_nodes[0].parent_index = 0; /* self */
    sid = rogue_skill_register(&def);
    assert(sid == 0);
    r = rogue_skills_validate_all(err, (int) sizeof err);
    assert(r == -1); /* cycle */
    rogue_skills_shutdown();
    rogue_effect_reset();
    printf("EFFECT_TREE_PARENT_INDEX_SELF_REJECT_OK\n");
    return 0;
}
