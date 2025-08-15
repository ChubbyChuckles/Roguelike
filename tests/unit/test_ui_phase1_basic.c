#include "ui/core/ui_context.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_init_and_basic_nodes(){
    RogueUIContext ctx; RogueUIContextConfig cfg={.max_nodes=16,.seed=123u};
    assert(rogue_ui_init(&ctx,&cfg));
    rogue_ui_begin(&ctx,16.6);
    int p = rogue_ui_panel(&ctx,(RogueUIRect){0,0,100,50},0xFF00FFFFu); assert(p==0);
    int t = rogue_ui_text(&ctx,(RogueUIRect){2,4,96,12},"Hello",0xFFFFFFFFu); assert(t==1);
    int count=0; const RogueUINode* nodes = rogue_ui_nodes(&ctx,&count); assert(nodes&&count==2);
    assert(nodes[0].kind==0 && nodes[1].kind==1);
    rogue_ui_end(&ctx);
    rogue_ui_shutdown(&ctx);
}

static void test_rng_stability(){
    RogueUIContext a,b; RogueUIContextConfig cfg={.max_nodes=4,.seed=999u};
    assert(rogue_ui_init(&a,&cfg)); assert(rogue_ui_init(&b,&cfg));
    unsigned int seq_a[3]; unsigned int seq_b[3];
    for(int i=0;i<3;i++){ seq_a[i]=rogue_ui_rng_next(&a); seq_b[i]=rogue_ui_rng_next(&b); }
    for(int i=0;i<3;i++){ assert(seq_a[i]==seq_b[i]); }
    rogue_ui_shutdown(&a); rogue_ui_shutdown(&b);
}

int main(){
    test_init_and_basic_nodes();
    test_rng_stability();
    printf("UI Phase1 basic tests passed\n");
    return 0;
}
