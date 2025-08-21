#include "core/skill_graph_runtime_internal.h"
#include "core/skill_maze.h"
#include "core/skills.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RogueSkillDef make_def(int id, int strength)
{
    RogueSkillDef d;
    memset(&d, 0, sizeof d);
    d.id = id;
    d.name = "s";
    d.icon = "i";
    d.max_rank = 3;
    d.skill_strength = strength;
    return d;
}

int main(void)
{
    RogueSkillMaze maze;
    memset(&maze, 0, sizeof maze);
    if (!rogue_skill_maze_generate("assets/skill_maze_config.json", &maze))
    {
        fprintf(stderr, "FAIL generate\n");
        return 1;
    }
    /* Register a set of skills: some ring-specific, some wildcard, include over-outer ring strength
     */
    int rings = maze.rings;
    int skill_count = rings * 2 + 4;
    if (skill_count < 8)
        skill_count = 8;
    RogueSkillDef* defs = (RogueSkillDef*) malloc(sizeof(RogueSkillDef) * skill_count);
    int idx = 0; /* one per ring */
    for (int r = 1; r <= rings && idx < skill_count; r++)
    {
        defs[idx] = make_def(idx, r);
        rogue_skill_register(&defs[idx]);
        idx++;
    }
    /* wildcard */
    if (idx < skill_count)
    {
        defs[idx] = make_def(idx, 0);
        rogue_skill_register(&defs[idx]);
        idx++;
    }
    /* outer ring+1 strength */
    if (idx < skill_count)
    {
        defs[idx] = make_def(idx, rings + 1);
        rogue_skill_register(&defs[idx]);
        idx++;
    }
    /* fill remainder wildcards */
    while (idx < skill_count)
    {
        defs[idx] = make_def(idx, 0);
        rogue_skill_register(&defs[idx]);
        idx++;
    }
    int* assigned = (int*) malloc(sizeof(int) * maze.node_count);
    int filled = rogue_skillgraph_assign_maze(&maze, assigned, skill_count);
    if (filled != maze.node_count)
    {
        fprintf(stderr, "FAIL filled=%d expected=%d\n", filled, maze.node_count);
        return 2;
    }
    for (int i = 0; i < maze.node_count; i++)
    {
        if (assigned[i] < 0)
        {
            fprintf(stderr, "FAIL unassigned node %d\n", i);
            return 3;
        }
    }
    printf("OK nodes=%d skills=%d rings=%d\n", maze.node_count, skill_count, rings);
    free(assigned);
    rogue_skill_maze_free(&maze);
    free(defs);
    return 0;
}
