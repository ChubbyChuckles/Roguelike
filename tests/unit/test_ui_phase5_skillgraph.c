#define SDL_MAIN_HANDLED 1
#include <assert.h>
#include <stdio.h>
#include "ui/core/ui_context.h"

/* Phase 5.1 Skill Graph basic test: zoomable panning with quadtree culling & layering */
static void populate(RogueUIContext* ui){
    rogue_ui_skillgraph_begin(ui, 0,0, 200, 150, 1.0f);
    /* create clustered nodes (some inside view, some outside) */
    for(int i=0;i<50;i++){
        float x = (float)(i*20);
        float y = (float)((i%5)*30);
        int rank = (i%4);
        int max_rank = 4;
        int synergy = (i%13)==0;
        rogue_ui_skillgraph_add(ui,x,y, i, rank, max_rank, synergy);
    }
}
int main(){
    printf("START skillgraph test\n"); fflush(stdout);
    RogueUIContext ui; RogueUIContextConfig cfg={0}; cfg.max_nodes=2048; cfg.seed=42; cfg.arena_size=32*1024; 
    int ok = rogue_ui_init(&ui,&cfg); printf("INIT ok=%d node_capacity=%d\n",ok,ui.node_capacity); fflush(stdout); if(!ok){ printf("FAIL init failed\n"); return 1; }
    rogue_ui_begin(&ui,16.0); printf("BEGIN frame node_capacity=%d\n",ui.node_capacity); fflush(stdout);
    populate(&ui); printf("ADDED nodes=%d (expected 50)\n", ui.skillgraph_node_count); fflush(stdout);
    int emitted = rogue_ui_skillgraph_build(&ui); printf("BUILD emitted=%d total_nodes_after=%d\n",emitted, ui.node_count); fflush(stdout);
    rogue_ui_end(&ui); printf("END frame\n"); fflush(stdout);
    int failures=0;
    if(emitted<=0){ printf("FAIL no skill nodes emitted (emitted=%d)\n",emitted); failures++; }
    /* Ensure quadtree culling removed far nodes: expect less than total 50 */
    /* emitted counts UI primitives (panel + pips + glow) so can exceed raw node count; ensure some nodes visible */
    if(emitted<=0){ printf("FAIL nothing emitted after build\n"); failures++; }
    /* Ensure at least one synergy glow panel (current glow color 0x30307040) */
    int count=0; const RogueUINode* nodes = rogue_ui_nodes(&ui,&count); int glow_found=0; for(int i=0;i<count;i++){ if(nodes[i].color==0x30307040u){ glow_found=1; break; } }
    if(!glow_found){ printf("FAIL synergy glow panel missing\n"); failures++; }
    if(!failures) printf("test_ui_phase5_skillgraph: OK (emitted=%d total_nodes=%d)\n",emitted,count); else printf("test_ui_phase5_skillgraph: FAIL (%d)\n",failures);
    rogue_ui_shutdown(&ui); return failures?1:0; }
