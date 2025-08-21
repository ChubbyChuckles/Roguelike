#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "snapshot_manager.h"
#include "rollback_manager.h"

// Simple fake system state structure
typedef struct FakeState { int value; int version_applied; } FakeState;
static FakeState g_state;

static int capture_cb(void* user, void** out_data, size_t* out_size, uint32_t* out_version){
    FakeState* st = (FakeState*)user;
    *out_size = sizeof(FakeState);
    *out_data = malloc(*out_size);
    if(!*out_data) return -10;
    memcpy(*out_data, st, *out_size);
    *out_version = (uint32_t)st->version_applied;
    return 0;
}
static int restore_cb(void* user, const void* data, size_t size, uint32_t version){
    if(size != sizeof(FakeState)) return -20;
    const FakeState* in = (const FakeState*)data;
    FakeState* st = (FakeState*)user;
    st->value = in->value;
    st->version_applied = (int)version;
    return 0;
}

int main(){
    setvbuf(stdout,NULL,_IONBF,0);
    memset(&g_state,0,sizeof(g_state));
    RogueSnapshotDesc desc = {0};
    desc.system_id = 1;
    desc.name = "fake";
    desc.capture = capture_cb;
    desc.restore = restore_cb;
    desc.user = &g_state;
    assert(rogue_snapshot_register(&desc) == 0);
    assert(rogue_rollback_configure(1,4)==0);

    // capture baseline
    g_state.value = 10; g_state.version_applied = 1; assert(rogue_rollback_capture(1)==0);
    g_state.value = 20; g_state.version_applied = 2; assert(rogue_rollback_capture(1)==0);
    g_state.value = 30; g_state.version_applied = 3; assert(rogue_rollback_capture(1)==0);

    // mutate forward without capturing
    g_state.value = 99; g_state.version_applied = 999;
    assert(rogue_rollback_step_back(1,0)==0); // rollback to latest captured (version 3)
    assert(g_state.value==30 && g_state.version_applied==3);

    // step back further
    g_state.value = 777; g_state.version_applied = 777; // scribble first
    assert(rogue_rollback_step_back(1,1)==0); // version 2
    assert(g_state.value==20 && g_state.version_applied==2);

    // negative test: too many steps
    assert(rogue_rollback_step_back(1,42)<0);

    // purge
    assert(rogue_rollback_purge(1)==0);
    assert(rogue_rollback_step_back(1,0)<0); // no history

    printf("test_rollback_manager OK\n");
    return 0;
}
