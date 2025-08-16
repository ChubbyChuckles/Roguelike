#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/skill_maze.h"
#include "core/skills.h"
#include "core/skill_graph_runtime_internal.h"

static RogueSkillDef make_def(int id,int ring){
    RogueSkillDef d; memset(&d,0,sizeof d);
    d.id=id; d.name="t"; d.icon="i"; d.max_rank=3; d.skill_strength=ring; d.base_cooldown_ms=0; return d;
}

int main(void){
    RogueSkillMaze maze; memset(&maze,0,sizeof maze);
    /* simple manual maze with 3 rings and 6 nodes */
    maze.rings=3; maze.node_count=6;
    maze.nodes = (RogueSkillMazeNode*)malloc(sizeof(RogueSkillMazeNode)*6);
    for(int i=0;i<6;i++){ maze.nodes[i].x= (float)(i*10); maze.nodes[i].y=0; maze.nodes[i].ring = (i/2)+1; }
    int skills=4; /* fewer skills than nodes to exercise reuse */
    RogueSkillDef defs[4];
    defs[0]=make_def(0,1); defs[1]=make_def(1,2); defs[2]=make_def(2,3); defs[3]=make_def(3,0); /* wildcard */
    for(int i=0;i<skills;i++) rogue_skill_register(&defs[i]);
    int* assigned=(int*)malloc(sizeof(int)*maze.node_count);
    int filled = rogue_skillgraph_assign_maze(&maze,assigned,skills);
    if(filled!=maze.node_count){ fprintf(stderr,"FAIL filled=%d expected=%d\n",filled,maze.node_count); return 1; }
    /* Ensure no unassigned */
    for(int i=0;i<maze.node_count;i++){ if(assigned[i]<0){ fprintf(stderr,"FAIL node %d unassigned\n",i); return 2; } }
    /* Ensure each node's skill respects ring constraint if not wildcard */
    for(int i=0;i<maze.node_count;i++){ int sid=assigned[i]; if(sid>=0){ RogueSkillDef* def=&defs[sid]; if(def->skill_strength>0 && def->skill_strength!=maze.nodes[i].ring && !(def->skill_strength>maze.rings && maze.nodes[i].ring==maze.rings)){ fprintf(stderr,"FAIL ring mismatch node=%d ring=%d skill_ring=%d\n",i,maze.nodes[i].ring,def->skill_strength); return 3; } } }
    free(assigned); free(maze.nodes); printf("OK\n"); return 0;
}
