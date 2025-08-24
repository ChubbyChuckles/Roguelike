#include "../../src/core/progression/progression_maze.h"
#include "../../src/core/progression/progression_passives.h"
#include "../../src/core/progression/progression_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Phase 7.3 anti-stack safeguards test: ensures diminishing contribution of multiple keystones per
 * category */
static const char* dsl =
    "0 STR+10 CRITC+5\n"
    "1 RES_PHY+10\n"
    "2 STR+10 CRITC+5\n"; /* duplicate offense keystone effects intentionally */

int main(void)
{
    printf("P7A_START\n");
    fflush(stdout);
    /* Build minimal synthetic maze with three nodes flagged as keystones in two offense & one
     * defense categories */
    RogueProgressionMaze maze;
    memset(&maze, 0, sizeof maze);
    maze.base.node_count = 3;
    maze.base.rings = 6;
    maze.base.nodes = (RogueSkillMazeNode*) malloc(sizeof(RogueSkillMazeNode) * 3);
    memset(maze.base.nodes, 0, sizeof(RogueSkillMazeNode) * 3);
    maze.base.edges = NULL;
    maze.base.edge_count = 0;
    maze.meta = (RogueProgressionMazeNodeMeta*) malloc(sizeof(RogueProgressionMazeNodeMeta) * 3);
    memset(maze.meta, 0, sizeof(RogueProgressionMazeNodeMeta) * 3);
    for (int i = 0; i < 3; i++)
    {
        maze.meta[i].node_id = i;
        maze.meta[i].ring = (i == 1) ? (maze.base.rings - 2) : (maze.base.rings - 1);
        if (maze.meta[i].ring < 1)
            maze.meta[i].ring = 1;
        maze.meta[i].level_req = 10;
        maze.meta[i].cost_points = 1;
        maze.meta[i].flags = ROGUE_MAZE_FLAG_KEYSTONE;
        maze.meta[i].adj_start = 0;
        maze.meta[i].adj_count = 0;
    }
    printf("MAZE_SYNTH\n");
    fflush(stdout);
    if (rogue_progression_passives_init(&maze) != 0)
    {
        printf("init_fail\n");
        fflush(stdout);
        return 1;
    }
    printf("PASSIVES_INIT_OK\n");
    fflush(stdout);
    if (rogue_progression_passives_load_dsl(dsl) != 0)
    {
        printf("dsl_fail\n");
        fflush(stdout);
        return 2;
    }
    printf("DSL_OK\n");
    fflush(stdout);
    /* Unlock node 0 (offense) */
    rogue_progression_passive_unlock(0, 10, 60, 50, 50, 50, 50);
    printf("UNLOCK0\n");
    fflush(stdout);
    int str_after1 = rogue_progression_passives_stat_total(0);
    /* Unlock node 2 (another offense) -> diminished incremental gain */
    rogue_progression_passive_unlock(2, 20, 60, 50, 50, 50, 50);
    printf("UNLOCK2\n");
    fflush(stdout);
    int str_after2 = rogue_progression_passives_stat_total(0);
    int diff = str_after2 - str_after1;
    if (diff >= 10)
    {
        printf("no_diminish diff=%d\n", diff);
        return 3;
    }
    /* Unlock node 1 (defense) -> independent category no diminishing vs first of its type */
    rogue_progression_passive_unlock(1, 30, 60, 50, 50, 50, 50);
    printf("UNLOCK1\n");
    fflush(stdout);
    int res_phy = rogue_progression_passives_stat_total(120);
    if (res_phy < 10)
    {
        printf("def_keystone_scaled res=%d\n", res_phy);
        return 4;
    }
    if (rogue_progression_passives_keystone_count_offense() != 2 ||
        rogue_progression_passives_keystone_count_defense() != 1)
    {
        printf("keystone_counts %d %d\n", rogue_progression_passives_keystone_count_offense(),
               rogue_progression_passives_keystone_count_defense());
        return 5;
    }
    printf("progression_phase7_antistack: OK str1=%d str2=%d inc=%d res=%d\n", str_after1,
           str_after2, diff, res_phy);
    fflush(stdout);
    rogue_progression_passives_shutdown();
    rogue_progression_maze_free(&maze);
    return 0;
}
