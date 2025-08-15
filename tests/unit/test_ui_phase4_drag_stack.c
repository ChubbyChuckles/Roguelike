#define SDL_MAIN_HANDLED 1
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "ui/core/ui_context.h"

static void frame(RogueUIContext* ui, RogueUIInputState in, int* ids, int* counts){
    fprintf(stderr,"FRAME begin call\n"); fflush(stderr);
    rogue_ui_begin(ui,16.0);
    fprintf(stderr,"After rogue_ui_begin\n"); fflush(stderr);
    rogue_ui_set_input(ui,&in);
    fprintf(stderr,"After set_input (mouse_x=%f mouse_y=%f pressed=%d down=%d rel=%d)\n",in.mouse_x,in.mouse_y,in.mouse_pressed,in.mouse_down,in.mouse_released); fflush(stderr);
    int first=0, vis=0;
    rogue_ui_inventory_grid(ui,(RogueUIRect){10,10,180,100},"inv_test",20,5,ids,counts,28,&first,&vis);
    fprintf(stderr,"After inventory_grid first=%d vis=%d\n",first,vis); fflush(stderr);
    rogue_ui_end(ui);
    fprintf(stderr,"FRAME end call\n"); fflush(stderr);
}

static int pump_event(RogueUIContext* ui, int kind, int* a, int* b, int* c){
    /* Drain queue until we find desired kind (or empty) */
    RogueUIEvent e; int found=0; while(rogue_ui_poll_event(ui,&e)){ if(e.kind==kind){ if(a)*a=e.a; if(b)*b=e.b; if(c)*c=e.c; found=1; } }
    return found;
}
static int any_event(RogueUIContext* ui){ RogueUIEvent e; return rogue_ui_poll_event(ui,&e); }

int main(){
    setvbuf(stderr,NULL,_IONBF,0);
    fprintf(stderr,"TEST start (stderr unbuffered)\n");
    uint64_t guard_pre=0x1111222233334444ULL, guard_post=0x5555666677778888ULL;
    RogueUIContext* ui = (RogueUIContext*)malloc(sizeof(RogueUIContext));
    assert(ui);
    RogueUIContextConfig cfg={0}; cfg.max_nodes=512; cfg.seed=11; cfg.arena_size=16*1024; 
    int init_ok = rogue_ui_init(ui,&cfg);
    if(!init_ok){ fprintf(stderr,"INIT_FAIL drag_stack test\n"); return 1; }
    fprintf(stderr,"POST_INIT node_capacity=%d nodes=%p\n", ui->node_capacity,(void*)ui->nodes);
    int ids[20]={0}; int counts[20]={0};
    ids[2]=101; counts[2]=8; ids[7]=202; counts[7]=3; // two stacks
    /* Execute original test logic */

    // Start drag on slot 2 (mouse over + press)
    /* cell position: x = rect.x + pad + col*(cell_size+spacing) + small offset */
    int pad=2, spacing=2, cell=28; int col2=2; int row0=0;
    RogueUIInputState in={0}; in.mouse_x=10+pad+col2*(cell+spacing)+4; in.mouse_y=10+pad+row0*(cell+spacing)+4; in.mouse_pressed=1; frame(ui,in,ids,counts); fprintf(stderr,"After first press frame\n"); fflush(stderr); in.mouse_pressed=0; in.mouse_down=1; frame(ui,in,ids,counts); fprintf(stderr,"After drag hold frame\n"); fflush(stderr);
    int a=0,b=0,c=0; int drag_ok=pump_event(ui,ROGUE_UI_EVENT_DRAG_BEGIN,&a,&b,&c); int failures=0; if(!drag_ok||a!=2){ printf("FAIL drag begin slot=%d expected 2\n",a); failures++; }
    // Drag release over slot 7
    in.mouse_down=0; in.mouse_released=1; int row1=1; in.mouse_x=10+pad+col2*(cell+spacing)+4; in.mouse_y=10+pad+row1*(cell+spacing)+4; // row1 col2 => slot 7
    frame(ui,in,ids,counts); int drop_from=-1, drop_to=-1; pump_event(ui,ROGUE_UI_EVENT_DRAG_END,&drop_from,&drop_to,&c); if(drop_from!=2||drop_to!=7){ printf("FAIL drag end from=%d to=%d\n",drop_from,drop_to); failures++; }
    // Verify swap
    if(!(ids[7]==101 && counts[7]==8)){ printf("FAIL swapped stack not at target (ids[7]=%d counts[7]=%d)\n",ids[7],counts[7]); failures++; }

    // Open stack split on slot 7 (ctrl+click)
    in=(RogueUIInputState){0}; in.key_ctrl=1; in.mouse_pressed=1; in.mouse_x=10+2*(28+2)+2; in.mouse_y=10+1*(28+2)+2; frame(ui,in,ids,counts); fprintf(stderr,"After split open frame\n"); fflush(stderr);
    int split_slot=-1, total=0, val=0; pump_event(ui,ROGUE_UI_EVENT_STACK_SPLIT_OPEN,&split_slot,&total,&val); if(split_slot!=7||total!=8){ printf("FAIL split open slot=%d total=%d\n",split_slot,total); failures++; }
    // Adjust split via wheel and apply (activate key)
    in=(RogueUIInputState){0}; in.key_activate=1; frame(ui,in,ids,counts); fprintf(stderr,"After split apply frame\n"); fflush(stderr); // apply
    int apply_from=-1, new_slot=-1, moved=0; pump_event(ui,ROGUE_UI_EVENT_STACK_SPLIT_APPLY,&apply_from,&new_slot,&moved);
    if(apply_from!=7||moved<=0){ printf("FAIL split apply from=%d moved=%d\n",apply_from,moved); failures++; }
    /* Ensure source stack reduced */
    if(counts[7] != (8-moved)){ printf("FAIL source stack count after split expected %d got %d\n",8-moved,counts[7]); failures++; }
    /* Ensure new slot received moved count */
    if(new_slot<0 || new_slot>=20 || counts[new_slot]!=moved){ printf("FAIL new stack slot=%d count=%d moved=%d\n",new_slot,new_slot>=0?counts[new_slot]:-1,moved); failures++; }

    /* Stack split cancel path: create artificial stack of 4 in slot 1 and then cancel */
    ids[1]=303; counts[1]=4; // ensure >1 for split
    int col1=1; RogueUIInputState sin={0};
    sin.key_ctrl=1; sin.mouse_pressed=1; sin.mouse_x=10+pad+col1*(cell+spacing)+4; sin.mouse_y=10+pad+row0*(cell+spacing)+4; frame(ui,sin,ids,counts); fprintf(stderr,"After cancel split open frame\n"); fflush(stderr);
    int open_slot=-1, open_total=0, open_val=0; pump_event(ui,ROGUE_UI_EVENT_STACK_SPLIT_OPEN,&open_slot,&open_total,&open_val);
    if(open_slot!=1 || open_total!=4){ printf("FAIL split open (cancel test) slot=%d total=%d\n",open_slot,open_total); failures++; }
    sin=(RogueUIInputState){0}; sin.mouse_released=1; frame(ui,sin,ids,counts); fprintf(stderr,"After cancel release frame\n"); fflush(stderr); int cancel_slot=-1; pump_event(ui,ROGUE_UI_EVENT_STACK_SPLIT_CANCEL,&cancel_slot,NULL,NULL); if(cancel_slot!=1){ printf("FAIL expected cancel event slot=1 got %d\n",cancel_slot); failures++; }

    if(failures==0) printf("test_ui_phase4_drag_stack: OK\n"); else printf("test_ui_phase4_drag_stack: FAIL (%d)\n",failures);
    rogue_ui_shutdown(ui); free(ui); return failures?1:0; }
