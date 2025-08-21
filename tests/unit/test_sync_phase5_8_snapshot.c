#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/core/integration/snapshot_manager.h"

typedef struct { int a; int b; } DummyState;
static DummyState g_state = {1,2};
static uint32_t g_ver = 0;

static int cap(void* user, void** out_data, size_t* out_size, uint32_t* out_version){
    (void)user; *out_size = sizeof(DummyState); *out_data = malloc(*out_size); if(!*out_data) return -1; memcpy(*out_data,&g_state,sizeof(DummyState)); *out_version = ++g_ver; return 0; }
static int restore(void* user, const void* data, size_t size, uint32_t version){ (void)user; (void)version; if(size!=sizeof(DummyState)) return -1; memcpy(&g_state,data,sizeof(DummyState)); return 0; }

int main(void){
    RogueSnapshotDesc d = { .system_id=101, .name="dummy", .capture=cap, .user=NULL, .max_size=sizeof(DummyState), .restore=restore };
    if(rogue_snapshot_register(&d)!=0){ fprintf(stderr,"register failed\n"); return 2; }
    if(rogue_snapshot_capture(101)!=0){ fprintf(stderr,"capture0 failed\n"); return 2; }
    const RogueSystemSnapshot* s0 = rogue_snapshot_get(101);
    if(!s0 || s0->version==0 || s0->size!=sizeof(DummyState)) { fprintf(stderr,"snapshot0 invalid\n"); return 2; }
    uint64_t h0 = s0->hash;
    // mutate and capture again
    g_state.a = 42; g_state.b = 7;
    if(rogue_snapshot_capture(101)!=0){ fprintf(stderr,"capture1 failed\n"); return 2; }
    const RogueSystemSnapshot* s1 = rogue_snapshot_get(101);
    if(!s1 || s1->version<=s0->version || s1->hash==h0) { fprintf(stderr,"snapshot1 invalid/version/hash not changed\n"); return 2; }
    // restore prior snapshot
    if(rogue_snapshot_restore(101, s0)!=0){ fprintf(stderr,"restore failed\n"); return 2; }
    if(g_state.a!=1 || g_state.b!=2){ fprintf(stderr,"restore content mismatch\n"); return 1; }
    printf("SYNC_5_8_SNAPSHOT_OK\n");
    return 0;
}
