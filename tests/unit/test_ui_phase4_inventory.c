#define SDL_MAIN_HANDLED 1
#include "../../src/ui/core/ui_context.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Minimal Phase 4 inventory grid test: ensures grid helper emits nodes. */

static void build_frame(RogueUIContext* ctx)
{
    RogueUIInputState in = {0};
    rogue_ui_begin(ctx, 16.0);
    rogue_ui_set_input(ctx, &in);
    int ids[12] = {0};
    int counts[12] = {0};
    for (int i = 0; i < 12; i += 3)
    {
        ids[i] = 100 + i;
        counts[i] = (i % 5) + 1;
    }
    int first = 0, vis = 0;
    rogue_ui_inventory_grid(ctx, (RogueUIRect){10, 10, 180, 100}, "inv_min", 12, 4, ids, counts, 28,
                            &first, &vis);
    rogue_ui_navigation_update(ctx);
    rogue_ui_end(ctx);
}

int main()
{
    RogueUIContext ctx;
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 256;
    cfg.seed = 77;
    cfg.arena_size = 8 * 1024;
    int init_ok = rogue_ui_init(&ctx, &cfg);
    if (!init_ok)
    {
        printf("FAIL init failed\n");
        return 1;
    }
    build_frame(&ctx);
    int failures = 0;
    int count = ctx.node_count;
    if (count <= 0)
    {
        printf("FAIL no nodes emitted\n");
        failures++;
    }
    /* count check already done */
    char buf_a[512];
    size_t len_a = rogue_ui_serialize(&ctx, buf_a, sizeof buf_a);
    build_frame(&ctx); /* second build should be deterministic */
    char buf_b[512];
    size_t len_b = rogue_ui_serialize(&ctx, buf_b, sizeof buf_b);
    if (len_a != len_b || strcmp(buf_a, buf_b) != 0)
    {
        printf("FAIL serialization not deterministic\n");
        failures++;
    }
    if (!failures)
        printf("test_ui_phase4_inventory: OK (nodes=%d)\n", count);
    else
        printf("test_ui_phase4_inventory: FAIL\n");
    rogue_ui_shutdown(&ctx);
    return failures ? 1 : 0;
}
