#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_validate.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

static int cb_consume(const RogueSkillDef* d, struct RogueSkillState* st,
                      const struct RogueSkillCtx* ctx)
{
    (void) d;
    (void) st;
    (void) ctx;
    return 1;
}

int main(void)
{
    rogue_skills_init();
    rogue_effect_reset();
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_STAT_BUFF;
    es.buff_type = 0;
    es.magnitude = 1;
    int eff = rogue_effect_register(&es);
    assert(eff >= 0);

    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = "TreeCycle";
    def.max_rank = 1;
    def.base_cooldown_ms = 10;
    def.on_activate = cb_consume;
    def.effect_spec_id = eff;
    def.effect_tree_node_count = 2;
    def.effect_tree_nodes[0].effect_spec_id = eff;
    def.effect_tree_nodes[0].parent_index = 1; /* points to node1 */
    def.effect_tree_nodes[1].effect_spec_id = eff;
    def.effect_tree_nodes[1].parent_index = 0; /* forms cycle */
    int sid = rogue_skill_register(&def);
    assert(sid >= 0);
    char err[256];
    int r = rogue_skills_validate_all(err, (int) sizeof err);
    assert(r == -1); /* should detect cycle */
    printf("EFFECT_TREE_CYCLE_REJECT_OK\n");
    rogue_skills_shutdown();
    rogue_effect_reset();
    return 0;
}
