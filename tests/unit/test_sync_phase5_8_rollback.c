#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/core/integration/snapshot_manager.h"
#include "../../src/core/integration/rollback_manager.h"

typedef struct { int v; } RBState;
static RBState g_rb = {10};
static uint32_t g_rb_ver=0;
static int cap(void* u, void** out_data, size_t* out_size, uint32_t* out_version){ (void)u; *out_size=sizeof(RBState); *out_data=malloc(*out_size); if(!*out_data) return -1; memcpy(*out_data,&g_rb,*out_size); *out_version=++g_rb_ver; return 0; }
static int restore(void* u, const void* data, size_t size, uint32_t v){ (void)u;(void)v; if(size!=sizeof(RBState)) return -1; memcpy(&g_rb,data,size); return 0; }

int main(void){
    RogueSnapshotDesc d={.system_id=103,.name="rb",.capture=cap,.user=NULL,.max_size=sizeof(RBState),.restore=restore};
    if(rogue_snapshot_register(&d)!=0){ fprintf(stderr,"reg failed\n"); return 2; }
    if(rogue_rollback_configure(103, 8)!=0){ fprintf(stderr,"cfg failed\n"); return 2; }
    // capture baseline and a mutated state
    if(rogue_rollback_capture(103)!=0){ fprintf(stderr,"cap0 failed\n"); return 2; }
    int v0 = g_rb.v; g_rb.v = 99; if(rogue_rollback_capture(103)!=0){ fprintf(stderr,"cap1 failed\n"); return 2; }
    // mutate to wrong value, then step back 1
    g_rb.v = -1234; if(rogue_rollback_step_back(103,1)!=0){ fprintf(stderr,"step_back failed\n"); return 2; }
    if(g_rb.v != 99){ fprintf(stderr,"rollback not to last\n"); return 1; }
    // step back again to baseline
    if(rogue_rollback_step_back(103,1)!=0){ fprintf(stderr,"step_back2 failed\n"); return 2; }
    if(g_rb.v != v0){ fprintf(stderr,"rollback baseline mismatch\n"); return 1; }
    printf("SYNC_5_8_ROLLBACK_OK\n");
    return 0;
}
