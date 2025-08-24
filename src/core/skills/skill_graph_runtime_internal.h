#ifndef ROGUE_CORE_SKILL_GRAPH_RUNTIME_INTERNAL_H
#define ROGUE_CORE_SKILL_GRAPH_RUNTIME_INTERNAL_H
#include "skill_maze.h"
#include "skills.h"
#ifdef __cplusplus
extern "C"
{
#endif
    /* Internal helper for tests (returns number of nodes filled) */
    int rogue_skillgraph_assign_maze(const RogueSkillMaze* maze, int* out_ids, int skill_count);
#ifdef __cplusplus
}
#endif
#endif
