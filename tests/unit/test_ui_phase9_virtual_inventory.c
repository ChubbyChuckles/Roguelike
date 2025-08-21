#include "ui/core/ui_context.h"
#include <stdio.h>

static int expect(int cond, const char* msg)
{
    if (!cond)
    {
        printf("FAIL %s\n", msg);
        return 0;
    }
    return 1;
}

int main()
{
    RogueUIContext ctx;
    RogueUIContextConfig cfg = {512, 42, 64 * 1024};
    if (!rogue_ui_init(&ctx, &cfg))
    {
        printf("FAIL init\n");
        return 1;
    }
    int slots = 200;
    int columns = 10;
    int ids[200] = {0};
    int counts[200] = {0};
    for (int i = 0; i < slots; i++)
    {
        ids[i] = 100 + i;
        counts[i] = (i % 5) + 1;
    }
    int first = 0, vis = 0;
    RogueUIRect area = {0, 0, 400, 160};
    /* Build first frame, expect limited visible subset */
    RogueUIInputState in = {0};
    in.mouse_x = 0;
    in.mouse_y = 0;
    rogue_ui_set_input(&ctx, &in);
    rogue_ui_begin(&ctx, 16.0);
    int root =
        rogue_ui_inventory_grid(&ctx, area, "inv", slots, columns, ids, counts, 24, &first, &vis);
    rogue_ui_end(&ctx);
    rogue_ui_render(&ctx);
    if (!expect(root >= 0, "grid_root"))
        return 1;
    /* Initial subset should be less than total; scrolling will advance rows */
    if (!expect(vis > 0 && vis < slots, "subset_visible_initial"))
        return 1;
    int nodes_initial = ctx.node_count;
    /* Scroll down (simulate wheel) several times and ensure first advances and node count stays
     * roughly bounded */
    for (int step = 0; step < 5; ++step)
    {
        in.wheel_delta = -1.0f;
        rogue_ui_set_input(&ctx, &in);
        rogue_ui_begin(&ctx, 16.0);
        rogue_ui_inventory_grid(&ctx, area, "inv", slots, columns, ids, counts, 24, &first, &vis);
        rogue_ui_end(&ctx);
        rogue_ui_render(&ctx);
        in.wheel_delta = 0;
    }
    if (!expect(first > 0, "scroll_advanced"))
        return 1;
    if (!expect(ctx.node_count <= nodes_initial + 64, "node_count_bounded"))
        return 1; /* allow minor variance */
    rogue_ui_shutdown(&ctx);
    printf("test_ui_phase9_virtual_inventory: OK\n");
    return 0;
}
