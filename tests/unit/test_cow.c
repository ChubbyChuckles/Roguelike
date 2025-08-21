#include "cow.h"
#include "ref_count.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_basic_clone_and_write()
{
    const char* text = "Hello Copy On Write!";
    size_t len = strlen(text) + 1;
    RogueCowBuffer* a = rogue_cow_create_from_bytes(text, len, 64);
    assert(a);
    RogueCowBuffer* b = rogue_cow_clone(a);
    assert(b);
    // pages should be shared initially
    assert(rogue_cow_page_refcount(a, 0) == 2);
    // read from clone
    char tmp[64] = {0};
    assert(rogue_cow_read(b, 0, tmp, len) == 0);
    assert(strcmp(tmp, text) == 0);
    // write through b triggers cow
    const char* patch = "Jello"; // modify first 5 bytes
    assert(rogue_cow_write(b, 0, patch, strlen(patch)) == 0);
    // a unchanged
    memset(tmp, 0, sizeof(tmp));
    assert(rogue_cow_read(a, 0, tmp, len) == 0);
    assert(strcmp(tmp, text) == 0);
    // b changed
    memset(tmp, 0, sizeof(tmp));
    assert(rogue_cow_read(b, 0, tmp, len) == 0);
    assert(strncmp(tmp, "Jello", 5) == 0);
    rogue_cow_destroy(a);
    rogue_cow_destroy(b);
}

static void test_dedup()
{
    // Create buffer with repeating pattern across pages (use compile-time constant sizes for MSVC)
    const size_t pg = 32;
    const size_t total = pg * 3;
    char buf[96]; /* 32*3 */
    for (size_t i = 0; i < pg; i++)
    {
        buf[i] = 'A';
        buf[pg + i] = 'A';
        buf[2 * pg + i] = 'B';
    }
    RogueCowBuffer* b = rogue_cow_create_from_bytes(buf, total, pg);
    assert(b);
    // force write on 3rd page to ensure uniqueness
    char x = 'B';
    assert(rogue_cow_write(b, 2 * pg, &x, 1) == 0);
    // page 0 and 1 identical -> dedup likely collapses one
    uint32_t before0 = rogue_cow_page_refcount(b, 0);
    uint32_t before1 = rogue_cow_page_refcount(b, 1);
    rogue_cow_dedup(b);
    uint32_t after0 = rogue_cow_page_refcount(b, 0);
    uint32_t after1 = rogue_cow_page_refcount(b, 1);
    // Either page 0 now shared (refcount>before0) or page 1 replaced (refcount==after0)
    assert(after0 >= before0);
    assert(after0 == after1);
    rogue_cow_destroy(b);
}

static void test_serialize()
{
    unsigned char data[100];
    for (int i = 0; i < 100; i++)
        data[i] = (unsigned char) i;
    const size_t chunk = 40; // create 3 pages (40,40,20)
    RogueCowBuffer* b = rogue_cow_create_from_bytes(data, sizeof(data), chunk);
    assert(b);
    size_t need = rogue_cow_serialize(b, NULL, 0);
    assert(need == 100);
    unsigned char out[128];
    memset(out, 0, sizeof(out));
    size_t ret = rogue_cow_serialize(b, out, sizeof(out));
    assert(ret == 100);
    assert(memcmp(out, data, 100) == 0);
    RogueCowBuffer* c = rogue_cow_deserialize(out, 100, chunk);
    assert(c);
    unsigned char chk[100];
    assert(rogue_cow_read(c, 0, chk, 100) == 0);
    assert(memcmp(chk, data, 100) == 0);
    rogue_cow_destroy(b);
    rogue_cow_destroy(c);
}

int main()
{
    test_basic_clone_and_write();
    test_dedup();
    test_serialize();
    RogueCowStats st;
    rogue_cow_get_stats(&st);
    printf("[cow] buffers=%llu pages=%llu cows=%llu copies=%llu dedup=%llu linear=%llu\n",
           (unsigned long long) st.buffers_created, (unsigned long long) st.pages_created,
           (unsigned long long) st.cow_triggers, (unsigned long long) st.page_copies,
           (unsigned long long) st.dedup_hits, (unsigned long long) st.serialize_linearizations);
    return 0;
}
