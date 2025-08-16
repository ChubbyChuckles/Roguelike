#include "core/skill_graph_runtime_internal.h"
#include <stdlib.h>

/* Deterministic multi-pass ring-aware assignment used by tests (mirrors runtime build logic). */
int rogue_skillgraph_assign_maze(const RogueSkillMaze* maze, int* out_ids, int skill_count){
    if(!maze||!out_ids||maze->node_count<=0) return 0;
    int count_safe = skill_count>0?skill_count:1;
    for(int i=0;i<maze->node_count;i++) out_ids[i]=-1;
    int filled=0; int cursor=0;
    for(int pass=0; pass<3 && filled<maze->node_count; ++pass){
        for(int n=0;n<maze->node_count && filled<maze->node_count; ++n){
            if(out_ids[n]>=0) continue; int ring=maze->nodes[n].ring;
            for(int tries=0; tries<skill_count; ++tries){
                int sid=(cursor+tries)%count_safe; const RogueSkillDef* def=rogue_skill_get_def(sid); if(!def) continue;
                int trg=def->skill_strength; int ok=0;
                if(trg==0) ok=1; else if(trg==ring) ok=1; else if(trg>maze->rings && ring==maze->rings) ok=1; else if(pass>=1){ if(pass==1 && (trg==ring-1 || trg==ring+1)) ok=1; else if(pass==2) ok=1; }
                if(ok){ out_ids[n]=sid; filled++; cursor=(sid+1)%count_safe; break; }
            }
            if(out_ids[n]<0 && skill_count>0){ out_ids[n]=(n+cursor)%count_safe; filled++; }
        }
    }
    for(int n=0;n<maze->node_count;n++){ if(out_ids[n]<0) out_ids[n]= n % count_safe; }
    return filled; }
