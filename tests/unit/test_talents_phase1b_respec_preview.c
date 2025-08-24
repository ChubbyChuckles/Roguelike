/* Unit test: Phase 1.B talents – respec and preview flows
   - Build a tiny 3-node line maze
   - Unlock node 0 and 1; verify points spent
   - Respec last 1; verify node 1 locked and point refunded
   - Preview begin -> unlock node 1 -> cancel; verify no state change / no point spend
   - Preview begin -> unlock node 1 -> commit; verify state change / point spent
   - Full respec returns to zero unlocks and refunds remaining points */
#include "../../src/core/app/app_state.h"
#include "../../src/core/progression/progression_maze.h"
#include "../../src/core/skills/skill_talents.h"
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
    RogueProgressionMaze mz;
    build_tiny_maze(&mz);
    assert(rogue_talents_init(&mz) == 0);
    g_app.talent_points = 5;

    /* Unlock node 0 then 1 */
    assert(rogue_talents_unlock(0, 0, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_unlock(1, 0, 1, 0, 0, 0, 0) == 1);
    assert(g_app.talent_points == 3);
    assert(rogue_talents_is_unlocked(1) == 1);

    /* Respec last 1 */
    assert(rogue_talents_respec_last(1) == 1);
    assert(rogue_talents_is_unlocked(1) == 0);
    assert(g_app.talent_points == 4);

    /* Preview unlock then cancel */
    assert(rogue_talents_preview_begin() == 1);
    assert(rogue_talents_preview_unlock(1, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_preview_cancel() == 1);
    assert(rogue_talents_is_unlocked(1) == 0);
    assert(g_app.talent_points == 4);

    /* Preview unlock then commit */
    assert(rogue_talents_preview_begin() == 1);
    assert(rogue_talents_preview_unlock(1, 1, 0, 0, 0, 0) == 1);
    int committed = rogue_talents_preview_commit(0);
    assert(committed == 1);
    assert(rogue_talents_is_unlocked(1) == 1);
    assert(g_app.talent_points == 3);

    /* Full respec */
    int refunded = rogue_talents_full_respec();
    assert(refunded >= 1);
    assert(rogue_talents_is_unlocked(0) == 0);
    assert(rogue_talents_is_unlocked(1) == 0);
    assert(g_app.talent_points >= 4);

    rogue_talents_shutdown();
    rogue_progression_maze_free(&mz);
    printf("test_talents_phase1b_respec_preview: OK\n");
    return 0;
}
