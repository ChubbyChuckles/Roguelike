#include "../../src/core/integration/ref_count.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple test object structure
typedef struct TestObj
{
    int value;
    struct TestObj* child;
} TestObj;

static void testobj_dtor(void* p)
{
    TestObj* o = (TestObj*) p;
    o->value = -999;
}

// Edge enumerator: expose child pointer
static size_t enum_edges(void* obj, void** out_children, size_t max)
{
    if (max == 0)
        return 0;
    TestObj* o = (TestObj*) obj;
    if (o->child)
    {
        out_children[0] = o->child;
        return 1;
    }
    return 0;
}

static void test_basic()
{
    TestObj* a = (TestObj*) rogue_rc_alloc(sizeof(TestObj), 1, testobj_dtor);
    a->value = 42;
    a->child = NULL;
    assert(rogue_rc_get_strong(a) == 1);
    rogue_rc_retain(a);
    assert(rogue_rc_get_strong(a) == 2);
    rogue_rc_release(a);
    assert(rogue_rc_get_strong(a) == 1);
    RogueWeakRef w = rogue_rc_weak_from(a);
    assert(rogue_rc_get_weak(a) >= 1);
    TestObj* acq = (TestObj*) rogue_rc_weak_acquire(w);
    assert(acq == a);
    rogue_rc_release(acq); // release acquire
    rogue_rc_release(a);   // invoke dtor
    // after release strong=0; weak ref still alive
    rogue_rc_weak_release(&w); // frees memory
}

static void test_upgrade_fail()
{
    TestObj* a = (TestObj*) rogue_rc_alloc(sizeof(TestObj), 2, testobj_dtor);
    RogueWeakRef w = rogue_rc_weak_from(a);
    rogue_rc_release(a); // destroy
    void* again = rogue_rc_weak_acquire(w);
    assert(again == NULL);
    rogue_rc_weak_release(&w);
}

static void test_graph_and_snapshot()
{
    TestObj* parent = (TestObj*) rogue_rc_alloc(sizeof(TestObj), 3, testobj_dtor);
    TestObj* child = (TestObj*) rogue_rc_alloc(sizeof(TestObj), 3, testobj_dtor);
    parent->child = child;
    rogue_rc_register_edge_enumerator(3, enum_edges);
    char buf[512];
    size_t dot_sz = rogue_rc_generate_dot(NULL, 0);
    assert(dot_sz > 0);
    size_t w = rogue_rc_generate_dot(buf, sizeof(buf));
    assert(w < sizeof(buf));
    // snapshot
    size_t snap_needed = rogue_rc_snapshot(NULL, 0);
    assert(snap_needed > 0);
    char snap[256];
    rogue_rc_snapshot(snap, sizeof(snap));
    // validate contains both ids
    uint64_t pid = rogue_rc_get_id(parent);
    uint64_t cid = rogue_rc_get_id(child);
    char needle1[32], needle2[32];
    snprintf(needle1, sizeof(needle1), "%llu", (unsigned long long) pid);
    snprintf(needle2, sizeof(needle2), "%llu", (unsigned long long) cid);
    assert(strstr(buf, needle1));
    assert(strstr(buf, needle2));
    assert(strstr(snap, needle1));
    assert(strstr(snap, needle2));
    // release order
    rogue_rc_release(child);
    rogue_rc_release(parent);
}

static void test_validation_and_leaks()
{
    TestObj* a = (TestObj*) rogue_rc_alloc(sizeof(TestObj), 4, testobj_dtor);
    assert(rogue_rc_validate());
    RogueRcStats st;
    rogue_rc_get_stats(&st);
    assert(st.live_objects >= 1);
    rogue_rc_dump_leaks(NULL); // smoke
    rogue_rc_release(a);
    assert(rogue_rc_validate());
}

int main()
{
    test_basic();
    test_upgrade_fail();
    test_graph_and_snapshot();
    test_validation_and_leaks();
    RogueRcStats st;
    rogue_rc_get_stats(&st);
    printf("[ref_count] total_allocs=%llu total_frees=%llu live=%llu peak=%llu\n",
           (unsigned long long) st.total_allocs, (unsigned long long) st.total_frees,
           (unsigned long long) st.live_objects, (unsigned long long) st.peak_live);
    return 0;
}
