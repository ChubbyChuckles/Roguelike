/* Unit test: Phase 1.B DAG model + modifiers + skill unlock
   - Build a small maze 0-1-2 (line)
   - Define node types and explicit prerequisites (node 2 requires [0,1])
   - Register a modifier on node 1 for skill 0
   - Mark node 2 as SKILL_UNLOCK for skill 0
   - Verify cannot unlock 2 with only 0 unlocked (AND prereqs)
   - Unlock 1 and then 2; verify skill unlock query
   - Verify effective skill def includes modifier from node 1
*/
#include "../../src/core/app/app_state.h"
#include "../../src/core/progression/progression_maze.h"
#include "../../src/core/skills/skill_talents.h"
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void build_line_maze3(RogueProgressionMaze* mz)
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
        mz->meta[i].str_req = mz->meta[i].dex_req = mz->meta[i].int_req = mz->meta[i].vit_req = 0;
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
    /* Prepare a simple skill */
    RogueSkillDef skill = {0};
    skill.id = 0;
    skill.name = "DAGTest";
    skill.icon = "none";
    skill.max_rank = 1;
    skill.base_cooldown_ms = 1000.0f;
    skill.action_point_cost = 5;
    rogue_skills_init();
    int sid = rogue_skill_register(&skill);
    assert(sid == 0);

    RogueProgressionMaze mz;
    build_line_maze3(&mz);
    assert(rogue_talents_init(&mz) == 0);

    g_app.talent_points = 5;

    /* Node 1 is a modifier for skill 0 */
    rogue_talents_set_node_type(1, ROGUE_TALENT_NODE_MODIFIER);
    RogueTalentModifier mod = {0};
    mod.node_id = 1;
    mod.skill_id = sid;
    mod.ap_delta = -1;
    mod.cd_scalar = 0.8f;
    assert(rogue_talents_register_modifier(&mod) == 1);

    /* Node 2 requires both 0 and 1, and unlocks the skill id (for query) */
    int prereqs[2] = {0, 1};
    assert(rogue_talents_set_prerequisites(2, prereqs, 2) == 1);
    rogue_talents_set_node_type(2, ROGUE_TALENT_NODE_SKILL_UNLOCK);
    assert(rogue_talents_set_skill_unlock(2, sid) == 1);

    /* Unlock 0 */
    assert(rogue_talents_unlock(0, 0, 1, 0, 0, 0, 0) == 1);
    /* Can't unlock 2 yet (needs 1 as well) */
    assert(rogue_talents_can_unlock(2, 1, 0, 0, 0, 0) == 0);
    /* Unlock 1 */
    assert(rogue_talents_unlock(1, 0, 1, 0, 0, 0, 0) == 1);
    /* Now 2 can unlock */
    assert(rogue_talents_can_unlock(2, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_unlock(2, 0, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_is_skill_unlocked(sid) == 1);

    /* Effective skill def should include modifier from node 1 */
    RogueSkillDef eff;
    assert(rogue_skill_get_effective_def(sid, &eff) == 1);
    assert(eff.action_point_cost == 4);
    assert(eff.base_cooldown_ms <= 800.5f);

    rogue_talents_shutdown();
    rogue_skills_shutdown();
    rogue_progression_maze_free(&mz);
    printf("test_talents_phase1b_dag_and_modifiers: OK\n");
    return 0;
}
