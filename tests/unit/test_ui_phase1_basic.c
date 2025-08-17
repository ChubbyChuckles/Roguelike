#include "ui/core/ui_context.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_init_and_basic_nodes(){
    RogueUIContext ctx; RogueUIContextConfig cfg={.max_nodes=16,.seed=123u};
    printf("STEP init calling\n"); fflush(stdout);
    assert(rogue_ui_init(&ctx,&cfg));
    printf("STEP init ok\n"); fflush(stdout);
    rogue_ui_begin(&ctx,16.6);
    printf("STEP begin ok\n"); fflush(stdout);
    int p = rogue_ui_panel(&ctx,(RogueUIRect){0,0,100,50},0xFF00FFFFu); assert(p==0);
    printf("STEP panel ok idx=%d\n",p); fflush(stdout);
    int t = rogue_ui_text(&ctx,(RogueUIRect){2,4,96,12},"Hello",0xFFFFFFFFu); assert(t==1);
    printf("STEP text ok idx=%d\n",t); fflush(stdout);
    int count=0; const RogueUINode* nodes = rogue_ui_nodes(&ctx,&count); assert(nodes&&count==2);
    printf("STEP nodes retrieved count=%d\n",count); fflush(stdout);
    assert(nodes[0].kind==0 && nodes[1].kind==1);
    printf("STEP kinds ok\n"); fflush(stdout);
    rogue_ui_end(&ctx);
    printf("STEP end ok\n"); fflush(stdout);
    rogue_ui_shutdown(&ctx);
    printf("STEP shutdown ok\n"); fflush(stdout);
}

static void test_rng_stability(){
    RogueUIContext a,b; RogueUIContextConfig cfg={.max_nodes=4,.seed=999u};
    printf("RNG init A\n"); fflush(stdout);
    assert(rogue_ui_init(&a,&cfg)); assert(rogue_ui_init(&b,&cfg));
    printf("RNG init both ok\n"); fflush(stdout);
    unsigned int seq_a[3]; unsigned int seq_b[3];
    for(int i=0;i<3;i++){ seq_a[i]=rogue_ui_rng_next(&a); seq_b[i]=rogue_ui_rng_next(&b); }
    for(int i=0;i<3;i++){ assert(seq_a[i]==seq_b[i]); }
    rogue_ui_shutdown(&a); rogue_ui_shutdown(&b);
    printf("RNG shutdown ok\n"); fflush(stdout);
}

int main(){
    printf("TEST_UI_PHASE1_BASIC_START\n"); fflush(stdout);
    test_init_and_basic_nodes();
    test_rng_stability();
    printf("UI Phase1 basic tests passed\n");
    return 0;
}
