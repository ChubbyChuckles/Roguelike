#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "core/progression/progression_maze.h"

/* Phase 7.4 visualization layering test */
int main(void){
    RogueProgressionMaze maze; memset(&maze,0,sizeof maze);
    /* Build tiny synthetic base with 2 rings + expansion to exercise functions */
    maze.base.rings=2; maze.base.node_count=4;
    maze.base.nodes=(RogueSkillMazeNode*)malloc(sizeof(RogueSkillMazeNode)*4);
    for(int i=0;i<4;i++){ maze.base.nodes[i].x = (float)( (i<2)? 40*cos(i*3.14159f): 80*cos(i*3.14159f*0.5f)); maze.base.nodes[i].y=(float)( (i<2)? 40*sin(i*3.14159f): 80*sin(i*3.14159f*0.5f)); maze.base.nodes[i].ring = (i<2)?0:1; }
    maze.base.edge_count=0; maze.base.edges=NULL;
    maze.meta=(RogueProgressionMazeNodeMeta*)calloc(4,sizeof(RogueProgressionMazeNodeMeta));
    for(int i=0;i<4;i++){ maze.meta[i].node_id=i; maze.meta[i].ring=maze.base.nodes[i].ring; maze.meta[i].level_req=1; maze.meta[i].cost_points=1; }
    /* Expand by 1 ring to ensure dynamic support */
    rogue_progression_maze_expand(&maze,1,123u);

    float layers[16]; int lc = rogue_progression_maze_layers(&maze,layers,16);
    if(lc != rogue_progression_maze_total_rings(&maze)){ printf("layer_count_mismatch %d %d\n", lc, rogue_progression_maze_total_rings(&maze)); return 2; }
    for(int i=1;i<lc;i++){ if(!(layers[i] > layers[i-1])){ printf("non_monotonic\n"); return 3; } }
    /* Project nodes */
    for(int i=0;i<maze.base.node_count;i++){ float r,t; if(!rogue_progression_maze_project(&maze,i,&r,&t)){ printf("proj_fail\n"); return 4; } if(r<0){ printf("neg_radius\n"); return 5; } }
    char buf[2048]; int w = rogue_progression_maze_ascii_overview(&maze,buf,sizeof buf,48,16); if(w<0){ printf("ascii_fail\n"); return 6; }
    if(strchr(buf,'\0')==NULL){ printf("no_terminator\n"); return 7; }
    /* Basic heuristic: expect at least one newline and non-dot character after plotting */
    if(strchr(buf,'\n')==NULL){ printf("no_newline\n"); return 8; }
    int has_plot=0; for(const char* p=buf; *p; ++p){ if(*p!='.' && *p!='\n'){ has_plot=1; break; } }
    if(!has_plot){ printf("empty_plot\n"); return 9; }
    printf("progression_phase7_visualization: OK rings=%d ascii_len=%d\n", lc, w); fflush(stdout);
    rogue_progression_maze_free(&maze); return 0; }
