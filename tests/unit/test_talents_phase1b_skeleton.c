/* Unit test: Phase 1.B talent graph skeleton
   - Build a tiny progression maze (3 nodes in a line)
   - Init talents, register a modifier for skill 0 at node 1
   - Unlock node 0 and then node 1, check talent points spent
   - Serialize/deserialize and compare hash
   - Verify effective skill def reflects modifier when node 1 unlocked
*/
#include "../../src/core/app/app_state.h"
#include "../../src/core/progression/progression_maze.h"
#include "../../src/core/skills/skill_talents.h"
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void build_tiny_maze(RogueProgressionMaze* mz)
{
    memset(mz, 0, sizeof *mz);
    mz->base.node_count = 3;
    mz->base.rings = 1;
    mz->base.nodes = (RogueSkillMazeNode*) calloc(3, sizeof(RogueSkillMazeNode));
    for (int i = 0; i < 3; ++i)
        mz->base.nodes[i].ring = 0;
    mz->meta = (RogueProgressionMazeNodeMeta*) calloc(3, sizeof(RogueProgressionMazeNodeMeta));
    for (int i = 0; i < 3; ++i)
    {
        mz->meta[i].node_id = i;
        mz->meta[i].ring = 0;
        mz->meta[i].level_req = 1;
        mz->meta[i].cost_points = 1;
        mz->meta[i].adj_start = (i == 0) ? 0 : (i == 1 ? 1 : 2);
        mz->meta[i].adj_count = (i == 1) ? 2 : 1;
    }
    mz->adjacency = (int*) calloc(3, sizeof(int));
    mz->adjacency[0] = 1; /* 0<->1 */
    mz->adjacency[1] = 0;
    mz->adjacency[2] = 1; /* 2 connected to 1 */
    mz->total_adjacency = 3;
}

int main(void)
{
    RogueSkillDef skill = {0};
    skill.id = 0;
    skill.name = "TestSkill";
    skill.icon = "none";
    skill.max_rank = 3;
    skill.skill_strength = 0;
    skill.base_cooldown_ms = 1000.0f;
    skill.cooldown_reduction_ms_per_rank = 0;
    skill.on_activate = NULL;
    skill.is_passive = 0;
    skill.tags = 0;
    skill.synergy_id = -1;
    skill.synergy_value_per_rank = 0;
    skill.resource_cost_mana = 0;
    skill.action_point_cost = 5;
    skill.max_charges = 0;
    skill.charge_recharge_ms = 0;
    skill.cast_time_ms = 0;
    skill.input_buffer_ms = 0;
    skill.min_weave_ms = 0;
    skill.early_cancel_min_pct = 0;
    skill.cast_type = 0;
    skill.combo_builder = 0;
    skill.combo_spender = 0;
    skill.reserved_u8 = 0;
    skill.effect_spec_id = 0;
    rogue_skills_init();
    int sid = rogue_skill_register(&skill);
    assert(sid == 0);
    g_app.talent_points = 3;

    RogueProgressionMaze mz;
    build_tiny_maze(&mz);
    assert(rogue_talents_init(&mz) == 0);

    RogueTalentModifier mod = {0};
    mod.node_id = 1; /* unlock at node 1 */
    mod.skill_id = sid;
    mod.cd_scalar = 0.5f;
    mod.ap_delta = -2;
    mod.add_tags = ROGUE_SKILL_TAG_FIRE;
    mod.charges_delta = 1;
    mod.add_effect_spec_id = 42;
    assert(rogue_talents_register_modifier(&mod) == 1);

    /* Cannot unlock node 1 before node 0 */
    assert(rogue_talents_can_unlock(1, 1, 0, 0, 0, 0) == 0);
    assert(rogue_talents_unlock(0, 0, 1, 0, 0, 0, 0) == 1);
    assert(g_app.talent_points == 2);
    assert(rogue_talents_can_unlock(1, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_unlock(1, 0, 1, 0, 0, 0, 0) == 1);
    assert(g_app.talent_points == 1);

    RogueSkillDef eff;
    assert(rogue_skill_get_effective_def(sid, &eff) == 1);
    if (!(eff.tags & ROGUE_SKILL_TAG_FIRE) || eff.action_point_cost != 3 || eff.max_charges != 1 ||
        eff.effect_spec_id != 42)
    {
        printf("modifier propagation failed: tags=0x%X ap=%d charges=%d eff=%d\n", eff.tags,
               eff.action_point_cost, eff.max_charges, eff.effect_spec_id);
        return 2;
    }
    if (eff.base_cooldown_ms > 501.0f)
    {
        printf("cooldown scalar not applied: %f\n", eff.base_cooldown_ms);
        return 3;
    }

    unsigned char buf[128];
    int wrote = rogue_talents_serialize(buf, sizeof buf);
    assert(wrote > 0);
    unsigned long long h1 = rogue_talents_hash();
    /* Reset and reload to test round-trip */
    rogue_talents_shutdown();
    assert(rogue_talents_init(&mz) == 0);
    int read = rogue_talents_deserialize(buf, (size_t) wrote);
    assert(read == wrote);
    unsigned long long h2 = rogue_talents_hash();
    if (h1 != h2)
    {
        printf("hash mismatch %llu vs %llu\n", (unsigned long long) h1, (unsigned long long) h2);
        return 4;
    }

    rogue_talents_shutdown();
    rogue_skills_shutdown();
    rogue_progression_maze_free(&mz);
    printf("test_talents_phase1b_skeleton: OK\n");
    return 0;
}
