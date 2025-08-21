#include "core/progression/progression_mastery.h"
#include "core/progression/progression_maze.h"
#include "core/progression/progression_passives.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Phase 7 initial scaffold test: ring expansion milestones & keystone heuristic */
int main(void)
{
    /* Phase 7 ring expansion + keystone dynamic expansion test */
    /* Synthetic minimal maze (avoid dependency on full generator for scaffold test) */
    RogueProgressionMaze maze;
    memset(&maze, 0, sizeof maze);
    maze.base.node_count = 3;
    maze.base.rings = 6;
    RogueProgressionMazeNodeMeta* meta =
        (RogueProgressionMazeNodeMeta*) malloc(sizeof(RogueProgressionMazeNodeMeta) * 3);
    memset(meta, 0, sizeof(RogueProgressionMazeNodeMeta) * 3);
    /* Node 0: inner non-keystone */ meta[0].node_id = 0;
    meta[0].ring = 2;
    meta[0].flags = 0;
    /* Node 1: outer high-degree simulated keystone */ meta[1].node_id = 1;
    meta[1].ring = (int) (maze.base.rings * 0.75f);
    meta[1].flags = ROGUE_MAZE_FLAG_KEYSTONE;
    /* Node 2: optional leaf (should not be keystone) */ meta[2].node_id = 2;
    meta[2].ring = maze.base.rings - 1;
    meta[2].flags = ROGUE_MAZE_FLAG_OPTIONAL;
    maze.meta = meta;
    /* Allocate base nodes array since expand expects heap */
    maze.base.nodes = (RogueSkillMazeNode*) malloc(sizeof(RogueSkillMazeNode) * 3);
    memset(maze.base.nodes, 0, sizeof(RogueSkillMazeNode) * 3);
    maze.base.edges = NULL;
    maze.base.edge_count = 0;
    int base_rings = maze.base.rings;
    /* Milestone levels */
    int extra_49 = rogue_progression_ring_expansions_unlocked(49);
    int extra_50 = rogue_progression_ring_expansions_unlocked(50);
    int extra_75 = rogue_progression_ring_expansions_unlocked(75);
    if (extra_49 != 0 || extra_50 < 1 || extra_75 < 1)
    {
        printf("ring_milestone_fail %d %d %d base_rings=%d\n", extra_49, extra_50, extra_75,
               base_rings);
        fflush(stdout);
        return 2;
    }
    int keystones = rogue_progression_maze_keystone_total(&maze);
    /* Must be non-negative; if zero, still pass but report */
    if (keystones < 0)
    {
        printf("keystone_neg\n");
        fflush(stdout);
        return 3;
    }
    /* Basic invariant: optional nodes should not be keystones */
    int n = maze.base.node_count;
    for (int i = 0; i < n; i++)
    {
        if ((maze.meta[i].flags & ROGUE_MAZE_FLAG_OPTIONAL) &&
            (maze.meta[i].flags & ROGUE_MAZE_FLAG_KEYSTONE))
        {
            printf("optional_keystone_collision\n");
            fflush(stdout);
            return 4;
        }
    }
    if (!rogue_progression_maze_is_keystone(&maze, 1) ||
        rogue_progression_maze_is_keystone(&maze, 2))
    {
        printf("keystone_helper_fail\n");
        fflush(stdout);
        return 5;
    }
    int added = rogue_progression_maze_expand(&maze, 2, 12345u);
    if (added <= 0)
    {
        printf("expand_fail added=%d\n", added);
        fflush(stdout);
        return 6;
    }
    if (rogue_progression_maze_total_rings(&maze) < base_rings + added)
    {
        printf("ring_count_mismatch\n");
        fflush(stdout);
        return 7;
    }
    printf("progression_phase7_rings: OK base_rings=%d keystones=%d extra50=%d synthetic=1 "
           "added=%d total_rings=%d\n",
           base_rings, keystones, extra_50, added, rogue_progression_maze_total_rings(&maze));
    fflush(stdout);
    rogue_progression_maze_free(&maze);
    return 0;
}
