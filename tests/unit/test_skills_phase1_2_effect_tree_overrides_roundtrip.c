#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Verifies that effect_tree nodes are preserved across export/import of overrides JSON. */
static int reg_dummy_effect(int magnitude)
{
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_STAT_BUFF;
    es.buff_type = 0;
    es.magnitude = magnitude;
    es.duration_ms = 1000.0f;
    int id = rogue_effect_register(&es);
    assert(id >= 0);
    return id;
}

int main(void)
{
    rogue_skills_init();
    rogue_effect_reset();

    int e0 = reg_dummy_effect(1);
    int e1 = reg_dummy_effect(2);
    int e2 = reg_dummy_effect(3);

    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = "TreeRT";
    def.max_rank = 1;
    def.effect_spec_id = e0;
    def.effect_tree_node_count = 3;
    def.effect_tree_nodes[0].effect_spec_id = e0;
    def.effect_tree_nodes[0].parent_index = -1;
    def.effect_tree_nodes[0].delay_ms = 0.0f;
    def.effect_tree_nodes[0].repeat_count = 1;
    def.effect_tree_nodes[1].effect_spec_id = e1;
    def.effect_tree_nodes[1].parent_index = 0;
    def.effect_tree_nodes[1].delay_ms = 150.0f;
    def.effect_tree_nodes[1].duration_ms = 500.0f;
    def.effect_tree_nodes[1].repeat_count = 0;
    def.effect_tree_nodes[1].repeat_interval_ms = 100.0f;
    def.effect_tree_nodes[2].effect_spec_id = e2;
    def.effect_tree_nodes[2].parent_index = 1;
    def.effect_tree_nodes[2].delay_ms = 50.0f;
    def.effect_tree_nodes[2].repeat_count = 1;
    int sid = rogue_skill_register(&def);
    assert(sid == 0);

    /* Export */
    char json[4096];
    int n = rogue_skill_debug_export_overrides_json(json, (int) sizeof json);
    assert(n > 0);
    assert(strstr(json, "effect_tree") != NULL); /* ensure tree serialized */

    /* Clear tree and primary then reload */
    g_app.skill_defs[0].effect_spec_id = -1;
    g_app.skill_defs[0].effect_tree_node_count = 0;
    for (int i = 0; i < 8; ++i)
        g_app.skill_defs[0].effect_tree_nodes[i].effect_spec_id = -1;

    struct RogueSkillEffectTreeNodeDebug fetched[8];
    int fcnt = 8;
    assert(rogue_skill_debug_get_effect_tree(0, fetched, &fcnt) == 0);
    assert(fcnt == 0);

    int applied = rogue_skill_debug_load_overrides_text(json);
    assert(applied >= 1);

    struct RogueSkillEffectTreeNodeDebug round[8];
    int rcnt = 8;
    assert(rogue_skill_debug_get_effect_tree(0, round, &rcnt) == 0);
    assert(rcnt == 3);
    assert(round[0].effect_spec_id == e0 && round[0].parent_index == -1);
    assert(round[1].effect_spec_id == e1 && round[1].parent_index == 0 &&
           round[1].duration_ms == 500.0f);
    assert(round[2].effect_spec_id == e2 && round[2].parent_index == 1 &&
           round[2].delay_ms == 50.0f);

    rogue_skills_shutdown();
    rogue_effect_reset();
    printf("EFFECT_TREE_OVERRIDES_ROUNDTRIP_OK\n");
    return 0;
}
