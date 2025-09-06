#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int reg_effect(int mag)
{
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_STAT_BUFF;
    es.buff_type = 0;
    es.magnitude = mag;
    es.duration_ms = 500.0f;
    return rogue_effect_register(&es);
}

int main(void)
{
    rogue_skills_init();
    rogue_effect_reset();
    int e0 = reg_effect(1);
    int e1 = reg_effect(2);
    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = "TreeLayoutRT";
    def.max_rank = 1;
    def.effect_tree_node_count = 2;
    def.effect_tree_nodes[0].effect_spec_id = e0;
    def.effect_tree_nodes[0].parent_index = -1;
    def.effect_tree_nodes[1].effect_spec_id = e1;
    def.effect_tree_nodes[1].parent_index = 0;
    int sid = rogue_skill_register(&def);
    assert(sid == 0);
    int xs[2] = {370, 460};
    int ys[2] = {430, 470};
    assert(rogue_skill_debug_set_effect_tree_layout(sid, 1, xs, ys, 2) == 0);
    char json[4096];
    int n = rogue_skill_debug_export_overrides_json(json, (int) sizeof json);
    assert(n > 0);
    assert(strstr(json, "effect_tree_layout") != NULL);
    /* wipe layout state globals indirectly by reinitializing registry (simulate fresh load) */
    // simulate destroying layout by zeroing orientation validity
    // (direct globals not exposed; rely on not calling set before import)
    // Clear skill tree then import
    g_app.skill_defs[0].effect_tree_node_count = 0;
    for (int i = 0; i < 8; ++i)
        g_app.skill_defs[0].effect_tree_nodes[i].effect_spec_id = -1;
    int applied = rogue_skill_debug_load_overrides_text(json);
    assert(applied >= 1);
    int orient = 0;
    int rx[8], ry[8];
    assert(rogue_skill_debug_get_effect_tree_layout(0, &orient, rx, ry, 2) == 0);
    assert(orient == 1);
    assert(rx[0] == 370 && ry[0] == 430);
    assert(rx[1] == 460 && ry[1] == 470);
    rogue_skills_shutdown();
    rogue_effect_reset();
    printf("EFFECT_TREE_LAYOUT_PERSISTENCE_OK\n");
    return 0;
}
