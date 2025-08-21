#include "../../src/core/integration/snapshot_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    unsigned char buf[256];
} BufState;
static BufState g_buf;
static uint32_t g_ver = 0;

static int cap(void* u, void** out_data, size_t* out_size, uint32_t* out_version)
{
    (void) u;
    *out_size = sizeof(BufState);
    *out_data = malloc(*out_size);
    if (!*out_data)
        return -1;
    memcpy(*out_data, &g_buf, sizeof(g_buf));
    *out_version = ++g_ver;
    return 0;
}
static int restore(void* u, const void* data, size_t size, uint32_t v)
{
    (void) u;
    (void) v;
    if (size != sizeof(BufState))
        return -1;
    memcpy(&g_buf, data, size);
    return 0;
}

int main(void)
{
    memset(&g_buf, 0, sizeof(g_buf));
    for (int i = 0; i < 256; i++)
        g_buf.buf[i] = (unsigned char) i;
    RogueSnapshotDesc desc = {.system_id = 102,
                              .name = "buf",
                              .capture = cap,
                              .user = NULL,
                              .max_size = sizeof(BufState),
                              .restore = restore};
    if (rogue_snapshot_register(&desc) != 0)
    {
        fprintf(stderr, "register failed\n");
        return 2;
    }
    if (rogue_snapshot_capture(102) != 0)
    {
        fprintf(stderr, "cap0 failed\n");
        return 2;
    }
    const RogueSystemSnapshot* s0 = rogue_snapshot_get(102);
    // mutate a middle range
    for (int i = 100; i < 140; i++)
        g_buf.buf[i] = (unsigned char) (255 - i);
    if (rogue_snapshot_capture(102) != 0)
    {
        fprintf(stderr, "cap1 failed\n");
        return 2;
    }
    const RogueSystemSnapshot* s1 = rogue_snapshot_get(102);
    RogueSnapshotDelta d = {0};
    if (rogue_snapshot_delta_build(s0, s1, &d) != 0)
    {
        fprintf(stderr, "delta build failed\n");
        return 2;
    }
    void* out = NULL;
    size_t out_sz = 0;
    uint64_t out_hash = 0;
    if (rogue_snapshot_delta_apply(s0, &d, &out, &out_sz, &out_hash) != 0)
    {
        fprintf(stderr, "delta apply failed\n");
        return 2;
    }
    if (out_sz != s1->size || out_hash != s1->hash || memcmp(out, s1->data, out_sz) != 0)
    {
        fprintf(stderr, "delta mismatch\n");
        return 1;
    }
    if (d.data_size >= s1->size)
    {
        fprintf(stderr, "delta not smaller than full\n"); /* not fatal, but warn */
    }
    rogue_snapshot_delta_free(&d);
    free(out);
    printf("SYNC_5_8_DELTA_OK\n");
    return 0;
}
