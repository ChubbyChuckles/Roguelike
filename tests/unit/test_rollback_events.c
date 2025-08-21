// Test rollback event log & auto rollback API
#include "rollback_manager.h"
#include "snapshot_manager.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int v;
    int ver;
} S;
static S st;

static int cap(void* user, void** out_data, size_t* out_size, uint32_t* out_version)
{
    S* s = (S*) user;
    *out_size = sizeof(S);
    *out_data = malloc(*out_size);
    if (!*out_data)
        return -1;
    memcpy(*out_data, s, *out_size);
    *out_version = (uint32_t) s->ver;
    return 0;
}
static int rest(void* user, const void* data, size_t size, uint32_t ver)
{
    if (size != sizeof(S))
        return -2;
    memcpy(user, data, size);
    ((S*) user)->ver = (int) ver;
    return 0;
}

int main()
{
    RogueSnapshotDesc d = {0};
    d.system_id = 5;
    d.name = "evt";
    d.capture = cap;
    d.restore = rest;
    d.user = &st;
    assert(rogue_snapshot_register(&d) == 0);
    assert(rogue_rollback_configure(5, 4) == 0);
    // create baseline snapshots
    st.v = 10;
    st.ver = 1;
    assert(rogue_rollback_capture(5) == 0);
    st.v = 11;
    st.ver = 2;
    assert(rogue_rollback_capture(5) == 0);
    st.v = 12;
    st.ver = 3;
    assert(rogue_rollback_capture(5) == 0);
    // map participant and invoke auto rollback (should restore latest version 3)
    assert(rogue_rollback_map_participant(42, 5) == 0);
    // mutate without capture then auto rollback
    st.v = 999;
    st.ver = 999;
    assert(rogue_rollback_auto_for_participant(42) == 0);
    assert(st.v == 12 && st.ver == 3);
    // event log should have at least one entry
    const RogueRollbackEvent* evs = NULL;
    size_t count = 0;
    assert(rogue_rollback_events_get(&evs, &count) == 0);
    assert(count > 0);
    int found_auto = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (evs[i].system_id == 5 && evs[i].auto_triggered)
        {
            found_auto = 1;
            break;
        }
    }
    assert(found_auto);
    fprintf(stderr, "test_rollback_events OK\n");
    return 0;
}
