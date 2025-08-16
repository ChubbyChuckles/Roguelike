#ifndef ROGUE_CORE_SKILL_MAZE_H
#define ROGUE_CORE_SKILL_MAZE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" { 
#endif

typedef struct RogueSkillMazeNode { float x,y; int ring; int a,b; } RogueSkillMazeNode; /* a,b optional indices for adjacency building */

typedef struct RogueSkillMazeEdge { int from; int to; } RogueSkillMazeEdge;

typedef struct RogueSkillMaze { RogueSkillMazeNode* nodes; int node_count; RogueSkillMazeEdge* edges; int edge_count; int rings; } RogueSkillMaze;

/* Load config JSON with fields: rings, approx_intersections, seed. Uses fallback search. Returns 1 on success. */
int rogue_skill_maze_generate(const char* config_path, RogueSkillMaze* out_maze);
void rogue_skill_maze_free(RogueSkillMaze* m);

#ifdef __cplusplus
}
#endif
#endif
