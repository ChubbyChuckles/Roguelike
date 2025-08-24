#include "../../src/core/skills/skill_graph_runtime_internal.h"
#include "../../src/core/skills/skill_maze.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This test currently just ensures repeated assignment with same inputs is deterministic. */

int main(void)
{
    RogueSkillMaze maze;
    memset(&maze, 0, sizeof maze);
    maze.rings = 2;
    maze.node_count = 4;
    maze.nodes = (RogueSkillMazeNode*) malloc(sizeof(RogueSkillMazeNode) * 4);
    for (int i = 0; i < 4; i++)
    {
        maze.nodes[i].x = (float) (i * 5);
        maze.nodes[i].y = 0;
        maze.nodes[i].ring = (i < 2) ? 1 : 2;
    }
    int skill_count = 3;
    int* assignedA = (int*) malloc(sizeof(int) * maze.node_count);
    int* assignedB = (int*) malloc(sizeof(int) * maze.node_count);
    int filledA = rogue_skillgraph_assign_maze(&maze, assignedA, skill_count);
    int filledB = rogue_skillgraph_assign_maze(&maze, assignedB, skill_count);
    if (filledA != filledB)
    {
        fprintf(stderr, "FAIL filled mismatch %d %d\n", filledA, filledB);
        return 1;
    }
    for (int i = 0; i < maze.node_count; i++)
    {
        if (assignedA[i] != assignedB[i])
        {
            fprintf(stderr, "FAIL nondeterministic node %d %d %d\n", i, assignedA[i], assignedB[i]);
            return 2;
        }
    }
    free(assignedA);
    free(assignedB);
    free(maze.nodes);
    printf("OK\n");
    return 0;
}
