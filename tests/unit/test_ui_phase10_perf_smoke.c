#include "ui/core/ui_context.h"
#include "ui/core/ui_test_harness.h"
#include <stdio.h>
#include <assert.h>

int main(){
    RogueUIContext ctx; memset(&ctx,0,sizeof ctx); RogueUIContextConfig cfg={.max_nodes=6000,.seed=9u};
    if(!rogue_ui_init(&ctx,&cfg)){
        fprintf(stderr,"PERF_SMOKE: init failed cfg.max_nodes=%d\n", cfg.max_nodes);
        return 2;
    }
    fprintf(stderr,"PERF_SMOKE: after init cap=%d node_size=%zu ctx=%p nodes_ptr=%p\n", ctx.node_capacity, sizeof(RogueUINode), (void*)&ctx, (void*)ctx.nodes);
    rogue_ui_begin(&ctx,16.6);
    int emitted = rogue_ui_perf_build_many(&ctx,5000);
    fprintf(stderr,"PERF_SMOKE: after build emitted=%d node_count=%d cap=%d\n", emitted, ctx.node_count, ctx.node_capacity);
    rogue_ui_end(&ctx);
    fprintf(stderr,"PERF_SMOKE: after end node_count=%d\n", ctx.node_count);
    if(emitted < 4500){ printf("PERF_SMOKE_FAIL emitted=%d\n",emitted); rogue_ui_shutdown(&ctx); return 1; }
    RogueUIDirtyInfo di = rogue_ui_dirty_info(&ctx); (void)di; // ensure call path
    fprintf(stderr,"PERF_SMOKE: about to shutdown arena_ptr=%p arena_size=%zu\n", (void*)ctx.arena, ctx.arena_size);
    rogue_ui_shutdown(&ctx);
    fprintf(stderr,"PERF_SMOKE: after shutdown nodes_ptr=%p arena_ptr=%p\n", (void*)ctx.nodes, (void*)ctx.arena);
    printf("PERF_SMOKE_OK emitted=%d\n",emitted); return 0; }
