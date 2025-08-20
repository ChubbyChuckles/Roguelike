#include "core/progression_maze.h"
#include <assert.h>
#include <stdio.h>

static int test_build(void){ RogueProgressionMaze m; if(!rogue_progression_maze_build("assets/skill_maze_config.json", &m)){ fprintf(stderr,"build_failed\n"); return -1; } assert(m.base.node_count>0); assert(m.base.edge_count>0); /* orphan audit */ int orphans=rogue_progression_maze_orphan_count(&m); if(orphans<0){ fprintf(stderr,"orphan_audit_fail\n"); return -1; } /* gating sanity: root node always unlockable at lvl 1 */ if(!rogue_progression_maze_node_unlockable(&m,0,1,0,0,0,0)){ fprintf(stderr,"root_locked\n"); return -1; } /* pick a high ring node and expect gating fails at low level */ int high_id=-1; int high_ring=0; for(int i=0;i<m.base.node_count;i++){ if(m.meta[i].ring>high_ring){ high_ring=m.meta[i].ring; high_id=i; } } if(high_id>=0){ int need_lvl=m.meta[high_id].level_req; if(need_lvl>1){ if(rogue_progression_maze_node_unlockable(&m,high_id,1,0,0,0,0)){ fprintf(stderr,"gating_low_level_fail\n"); return -1; } if(!rogue_progression_maze_node_unlockable(&m,high_id,need_lvl,999,999,999,999)){ fprintf(stderr,"gating_high_level_fail\n"); return -1; } }
    }
    /* path cost monotonicity: cost from node 0 to any neighbor should be >= neighbor cost */
    if(m.base.node_count>1){ for(int i=1;i<m.base.node_count && i<25;i++){ int c=rogue_progression_maze_shortest_cost(&m,0,i); if(c<0){ /* skip unreachable */ continue; } if(c < m.meta[i].cost_points){ fprintf(stderr,"path_cost_less_than_node_cost id=%d c=%d node_cost=%d\n",i,c,m.meta[i].cost_points); return -1; } } }
    rogue_progression_maze_free(&m); return 0; }

int main(void){ if(test_build()<0) return 1; printf("progression_phase4_maze: OK\n"); return 0; }
