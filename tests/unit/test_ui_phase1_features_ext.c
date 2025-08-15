/* Tests for UI Phase 1.4 - 1.7 (RNG channel existed; now snapshot, diff, arena) */
#include "ui/core/ui_context.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void test_arena_and_text_dup(){
    RogueUIContext ctx; RogueUIContextConfig cfg={.max_nodes=8,.seed=42u,.arena_size=1024};
    assert(rogue_ui_init(&ctx,&cfg));
    rogue_ui_begin(&ctx,16.0);
    int id = rogue_ui_text_dup(&ctx,(RogueUIRect){0,0,50,10},"ArenaString",0xFFFFFFFFu);
    assert(id==0);
    int count=0; const RogueUINode* nodes = rogue_ui_nodes(&ctx,&count); assert(count==1);
    assert(nodes[0].text && strcmp(nodes[0].text,"ArenaString")==0);
    rogue_ui_end(&ctx);
    rogue_ui_shutdown(&ctx);
}

static void test_snapshot_and_diff(){
    RogueUIContext ctx; RogueUIContextConfig cfg={.max_nodes=4,.seed=99u,.arena_size=2048};
    assert(rogue_ui_init(&ctx,&cfg));
    struct SimDummy { int hp; int xp; } sim={100,250};
    rogue_ui_set_simulation_snapshot(&ctx,&sim,sizeof sim);
    size_t snap_size=0; const void* snap = rogue_ui_simulation_snapshot(&ctx,&snap_size);
    assert(snap==&sim && snap_size==sizeof sim);
    rogue_ui_begin(&ctx,16.0);
    assert(rogue_ui_diff_changed(&ctx)==1);
    assert(rogue_ui_diff_changed(&ctx)==0);
    rogue_ui_panel(&ctx,(RogueUIRect){0,0,10,10},0xFF00FF00u);
    assert(rogue_ui_diff_changed(&ctx)==1);
    assert(rogue_ui_diff_changed(&ctx)==0);
    rogue_ui_end(&ctx);
    rogue_ui_shutdown(&ctx);
}

static void test_capacity_and_arena_exhaust(){
    RogueUIContext ctx; RogueUIContextConfig cfg={.max_nodes=2,.seed=1u,.arena_size=32};
    assert(rogue_ui_init(&ctx,&cfg));
    rogue_ui_begin(&ctx,0);
    assert(rogue_ui_panel(&ctx,(RogueUIRect){0,0,1,1},0x0)!=-1);
    assert(rogue_ui_text_dup(&ctx,(RogueUIRect){0,0,1,1},"A",0x0)!=-1);
    assert(rogue_ui_panel(&ctx,(RogueUIRect){0,0,1,1},0x0)==-1);
    void* big = rogue_ui_arena_alloc(&ctx, 1000, 1);
    assert(big==NULL);
    rogue_ui_end(&ctx);
    rogue_ui_shutdown(&ctx);
}

int main(){
    test_arena_and_text_dup();
    test_snapshot_and_diff();
    test_capacity_and_arena_exhaust();
    printf("UI Phase1 extended feature tests passed\n");
    return 0;
}
