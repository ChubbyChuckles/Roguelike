/* Unit test: Talents open-allocation ANY threshold gating and cost-aware unlocks
   - Tiny 4-node line: 0-1-2-3, cost_points: [1,2,1,1]
   - Set any_threshold=2 so after 2 total unlocked, non-adjacent may unlock
   - Unlock 0 (cost 1), try unlock 2 before adjacency -> should fail until threshold met
   - Unlock 1 (cost 2) consumes 2 points; total unlocked=2
   - Now unlock 3 via ANY threshold despite no direct adjacency to unlocked neighbors
*/
#include "../../src/core/app/app_state.h"
#include "../../src/core/progression/progression_maze.h"
#include "../../src/core/skills/skill_talents.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void build_maze_4(RogueProgressionMaze* mz)
{
    memset(mz, 0, sizeof *mz);
    mz->base.node_count = 4;
    mz->base.rings = 1;
    mz->base.nodes = (RogueSkillMazeNode*) calloc(4, sizeof(RogueSkillMazeNode));
    for (int i = 0; i < 4; ++i)
        mz->base.nodes[i].ring = 0;
    mz->meta = (RogueProgressionMazeNodeMeta*) calloc(4, sizeof(RogueProgressionMazeNodeMeta));
    for (int i = 0; i < 4; ++i)
    {
        mz->meta[i].node_id = i;
        mz->meta[i].ring = 0;
        mz->meta[i].level_req = 1;
        mz->meta[i].str_req = mz->meta[i].dex_req = mz->meta[i].int_req = mz->meta[i].vit_req = 0;
        mz->meta[i].cost_points = (i == 1) ? 2 : 1;
        mz->meta[i].adj_start = i;
        mz->meta[i].adj_count = (i == 1 || i == 2) ? 2 : 1;
    }
    mz->adjacency = (int*) calloc(4, sizeof(int));
    /* linear chain 0-1-2-3 encoded minimally for test adjacency checks */
    mz->adjacency[0] = 1; /* neighbor of 0 is 1 */
    mz->adjacency[1] = 0; /* neighbor of 1: 0 (we rely on adj_count=2 for 1 and 2 check loops) */
    mz->adjacency[2] =
        3; /* neighbor of 2: 3 (the second neighbor indices aren't used by our test logic) */
    mz->adjacency[3] = 2; /* neighbor of 3: 2 */
    mz->total_adjacency = 4;
}

int main(void)
{
    RogueProgressionMaze mz;
    build_maze_4(&mz);
    assert(rogue_talents_init(&mz) == 0);
    g_app.talent_points = 5;

    /* Configure open allocation ANY threshold */
    rogue_talents_set_any_threshold(2);

    /* Unlock 0 (cost 1) */
    assert(rogue_talents_can_unlock(0, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_unlock(0, 0, 1, 0, 0, 0, 0) == 1);
    assert(g_app.talent_points == 4);

    /* Can't unlock 2 yet (not adjacent and threshold not met) */
    assert(rogue_talents_can_unlock(2, 1, 0, 0, 0, 0) == 0);

    /* Unlock 1 (cost 2) */
    assert(rogue_talents_can_unlock(1, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_unlock(1, 0, 1, 0, 0, 0, 0) == 1);
    assert(g_app.talent_points == 2);

    /* Threshold met: now allow 3 even though not adjacent to an unlocked neighbor */
    assert(rogue_talents_can_unlock(3, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_unlock(3, 0, 1, 0, 0, 0, 0) == 1);
    assert(g_app.talent_points == 1);

    rogue_talents_shutdown();
    rogue_progression_maze_free(&mz);
    printf("test_talents_phase1b_open_allocation: OK\n");
    return 0;
}
