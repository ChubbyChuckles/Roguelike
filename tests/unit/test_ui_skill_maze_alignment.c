#include "../../src/core/skills/skill_graph_runtime_internal.h"
#include "../../src/core/skills/skill_maze.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Alignment/overlap heuristic test. */
int main(void)
{
    RogueSkillMaze maze;
    memset(&maze, 0, sizeof maze);
    maze.rings = 3;
    maze.node_count = 6;
    maze.nodes = (RogueSkillMazeNode*) malloc(sizeof(RogueSkillMazeNode) * 6);
    for (int i = 0; i < 6; i++)
    {
        maze.nodes[i].x = (float) ((i % 3) * 10);
        maze.nodes[i].y = (float) ((i / 3) * 10);
        maze.nodes[i].ring = (i / 2) + 1;
    }
    int skills = 3;
    int* first = (int*) malloc(sizeof(int) * maze.node_count);
    int* second = (int*) malloc(sizeof(int) * maze.node_count);
    rogue_skillgraph_assign_maze(&maze, first, skills);
    rogue_skillgraph_assign_maze(&maze, second, skills);
    for (int i = 0; i < maze.node_count; i++)
    {
        if (first[i] != second[i])
        {
            fprintf(stderr, "FAIL assignment drift node=%d %d %d\n", i, first[i], second[i]);
            return 1;
        }
    }
    for (int i = 0; i < maze.node_count; i++)
        for (int j = i + 1; j < maze.node_count; j++)
        {
            float dx = maze.nodes[i].x - maze.nodes[j].x;
            float dy = maze.nodes[i].y - maze.nodes[j].y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < 4.0f)
            {
                fprintf(stderr, "FAIL nodes too close %d %d d=%f\n", i, j, d);
                return 2;
            }
        }
    free(first);
    free(second);
    free(maze.nodes);
    printf("OK\n");
    return 0;
}
