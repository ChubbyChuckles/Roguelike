/* Unit test: Phase 5.2/5.3 skill graph layering & animations */
#define SDL_MAIN_HANDLED 1
#include <stdio.h>
#include <assert.h>
#include "ui/core/ui_context.h"

static void build_graph(RogueUIContext* ui){
    rogue_ui_skillgraph_begin(ui,0,0,200,150,1.0f);
    for(int i=0;i<8;i++){ rogue_ui_skillgraph_add(ui,(float)(i*22),(float)((i%4)*30), i, i%3, 3, (i%5)==0, 0); }
}

int main(){
    RogueUIContext ui; RogueUIContextConfig cfg={0}; cfg.max_nodes=1024; cfg.seed=7; cfg.arena_size=16*1024; if(!rogue_ui_init(&ui,&cfg)){ printf("FAIL init\n"); return 1; }
    /* Frame 1: build graph, trigger pulse+spend for icon 2 */
    printf("FRAME0 begin\n"); fflush(stdout);
    rogue_ui_begin(&ui,16.0);
    build_graph(&ui);
    rogue_ui_skillgraph_pulse(&ui,2);
    rogue_ui_skillgraph_spend_flyout(&ui,2,1);
    int emitted = rogue_ui_skillgraph_build(&ui);
    rogue_ui_end(&ui);
    printf("FRAME0 emitted=%d pulses=%d spends=%d\n", emitted, ui.skillgraph_pulse_count, ui.skillgraph_spend_count); fflush(stdout);
    int count1=0; const RogueUINode* nodes1 = rogue_ui_nodes(&ui,&count1); (void)nodes1; if(emitted<=0||count1<=0){ printf("FAIL no nodes emitted\n"); return 1; }
    /* Advance several frames to ensure animations decay */
    for(int f=0; f<20; ++f){ rogue_ui_begin(&ui,40.0); build_graph(&ui); emitted=rogue_ui_skillgraph_build(&ui); rogue_ui_end(&ui); }
    printf("AFTER frames pulses=%d spends=%d\n", ui.skillgraph_pulse_count, ui.skillgraph_spend_count); fflush(stdout);
    /* After ~600ms pulses should be gone */
    if(ui.skillgraph_pulse_count!=0){ printf("FAIL pulses not cleared (%d)\n", ui.skillgraph_pulse_count); return 1; }
    if(ui.skillgraph_spend_count!=0){ printf("FAIL spend flyouts not cleared (%d)\n", ui.skillgraph_spend_count); return 1; }
    printf("test_ui_phase5_skillgraph_anim: OK emitted_last=%d total_nodes=%d\n", emitted, ui.node_count);
    rogue_ui_shutdown(&ui); return 0; }
