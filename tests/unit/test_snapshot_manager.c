#include "../../src/core/integration/snapshot_manager.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple mock system that increments a counter and stores an array
typedef struct MockSystem
{
    unsigned counter;
    unsigned char buffer[128];
} MockSystem;

static int mock_capture(void* user, void** out_data, size_t* out_size, uint32_t* out_version)
{
    MockSystem* ms = (MockSystem*) user;
    // mutate buffer pattern based on counter
    for (size_t i = 0; i < sizeof(ms->buffer); i++)
        ms->buffer[i] = (unsigned char) (ms->counter + i);
    unsigned char* mem = (unsigned char*) malloc(sizeof(ms->buffer));
    memcpy(mem, ms->buffer, sizeof(ms->buffer));
    *out_data = mem;
    *out_size = sizeof(ms->buffer);
    *out_version = ms->counter;
    return 0;
}

static void test_register_and_capture()
{
    MockSystem ms = {0};
    RogueSnapshotDesc d = {1, "mock", mock_capture, &ms, sizeof(ms.buffer)};
    assert(rogue_snapshot_register(&d) == 0);
    for (int i = 1; i <= 5; i++)
    {
        ms.counter = i;
        int rc = rogue_snapshot_capture(1);
        assert(rc == 0);
        const RogueSystemSnapshot* snap = rogue_snapshot_get(1);
        assert(snap && snap->version == (uint32_t) i);
        assert(snap->size == sizeof(ms.buffer));
        uint64_t h = rogue_snapshot_rehash(snap);
        assert(h == snap->hash);
    }
}

static void test_delta_round_trip()
{
    MockSystem ms = {0};
    RogueSnapshotDesc d = {2, "delta", mock_capture, &ms, sizeof(ms.buffer)};
    assert(rogue_snapshot_register(&d) == 0);
    ms.counter = 1;
    assert(rogue_snapshot_capture(2) == 0);
    const RogueSystemSnapshot* a = rogue_snapshot_get(2);
    // copy base snapshot before next capture overwrites internal storage
    RogueSystemSnapshot base_copy = *a;
    base_copy.data = malloc(a->size);
    memcpy(base_copy.data, a->data, a->size);
    ms.counter = 2;
    assert(rogue_snapshot_capture(2) == 0);
    const RogueSystemSnapshot* b = rogue_snapshot_get(2);
    RogueSnapshotDelta delta;
    assert(rogue_snapshot_delta_build(&base_copy, b, &delta) == 0);
    void* new_data = NULL;
    size_t new_size = 0;
    uint64_t new_hash = 0;
    assert(rogue_snapshot_delta_apply(&base_copy, &delta, &new_data, &new_size, &new_hash) == 0);
    assert(new_size == b->size);
    assert(memcmp(new_data, b->data, b->size) == 0);
    assert(new_hash == b->hash);
    rogue_snapshot_delta_free(&delta);
    free(new_data);
    free(base_copy.data);
}

static void test_version_monotonic()
{
    MockSystem ms = {0};
    RogueSnapshotDesc d = {3, "ver", mock_capture, &ms, sizeof(ms.buffer)};
    assert(rogue_snapshot_register(&d) == 0);
    ms.counter = 5;
    assert(rogue_snapshot_capture(3) == 0);
    const RogueSystemSnapshot* s = rogue_snapshot_get(3);
    assert(s->version == 5);
    // attempt capture with lower version should fail but not replace
    ms.counter = 4;
    int rc = rogue_snapshot_capture(3);
    assert(rc != 0);
    s = rogue_snapshot_get(3);
    assert(s->version == 5);
}

static void test_stats()
{
    RogueSnapshotStats st;
    rogue_snapshot_get_stats(&st);
    assert(st.registered_systems >= 3);
    // ensure we have at least some captures and deltas counted
    assert(st.total_captures > 0);
}

static void test_dependencies()
{
    MockSystem a = {0}, b = {0};
    RogueSnapshotDesc da = {10, "A", mock_capture, &a, sizeof(a.buffer)};
    RogueSnapshotDesc db = {11, "B", mock_capture, &b, sizeof(b.buffer)};
    assert(rogue_snapshot_register(&da) == 0);
    assert(rogue_snapshot_register(&db) == 0);
    assert(rogue_snapshot_dependency_add(11, 10) == 0); // B depends on A
    int order[8];
    size_t n = 8;
    assert(rogue_snapshot_plan_order(order, &n) == 0);
    int foundA = 0, foundB = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (order[i] == 10)
            foundA = i + 1;
        if (order[i] == 11)
            foundB = i + 1;
    }
    assert(foundA && foundB && foundA < foundB); // A before B
}

static void test_reset_and_replay_log()
{
    rogue_snapshot_replay_log_enable(8);
    MockSystem ms = {0};
    RogueSnapshotDesc d = {12, "R", mock_capture, &ms, sizeof(ms.buffer)};
    assert(rogue_snapshot_register(&d) == 0);
    for (int i = 1; i <= 3; i++)
    {
        ms.counter = i;
        assert(rogue_snapshot_capture(12) == 0);
    }
    const RogueSnapshotDeltaRecord* recs = NULL;
    size_t cnt = 0;
    rogue_snapshot_replay_log_get(
        &recs, &cnt); // log currently minimal (delta logging not yet wired fully)
    // ensure reset clears version
    assert(rogue_snapshot_reset(12) == 0);
    const RogueSystemSnapshot* s = rogue_snapshot_get(12);
    assert(s->version == 0);
}

int main()
{
    test_register_and_capture();
    test_delta_round_trip();
    test_version_monotonic();
    test_stats();
    test_dependencies();
    test_reset_and_replay_log();
    printf("snapshot_manager tests passed\n");
    return 0;
}
