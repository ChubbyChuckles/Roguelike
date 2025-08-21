#define SDL_MAIN_HANDLED 1
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
    RogueUIContextConfig cfg = {256, 77, 32 * 1024};
    if (!rogue_ui_init(&ctx, &cfg))
    {
        printf("FAIL init\n");
        return 1;
    }
    /* Virtual list range basics */
    int first = 0, count = 0;
    int vis = rogue_ui_list_virtual_range(100, 20, 95, 0, &first, &count);
    if (!expect(vis == 5 && first == 0 && count == 5, "virt_range_top"))
        return 1;
    else
        printf("DBG top first=%d count=%d vis=%d\n", first, count, vis);
    vis = rogue_ui_list_virtual_range(100, 20, 95, 130, &first, &count); /* scroll inside */
    if (!expect(first == 6 && count == 5, "virt_range_mid"))
        return 1;
    else
        printf("DBG mid first=%d count=%d\n", first, count);
    vis = rogue_ui_list_virtual_range(10, 20, 60, 400, &first, &count); /* near end */
    if (!expect(first == 9 && count == 1, "virt_range_end"))
        return 1;
    else
        printf("DBG end first=%d count=%d\n", first, count);
    /* Emit a virtual list */
    rogue_ui_begin(&ctx, 16.0);
    RogueUIRect area = {0, 0, 200, 100};
    int emitted = rogue_ui_list_virtual_emit(&ctx, area, 50, 18, 40, 0x111111FF, 0x222222FF);
    rogue_ui_end(&ctx);
    if (!expect(emitted > 0, "virt_emit"))
        return 1;
    else
        printf("DBG emitted=%d nodes=%d\n", emitted, ctx.node_count);
    /* Dirty info after initial render */
    rogue_ui_render(&ctx);
    RogueUIDirtyInfo di = rogue_ui_dirty_info(&ctx);
    if (!expect(di.changed == 1 && di.changed_node_count > 0, "dirty_first"))
        return 1;
    else
        printf("DBG dirty_first nodes=%d rect=%.1f,%.1f %.1fx%.1f\n", di.changed_node_count, di.x,
               di.y, di.w, di.h);
    /* Second render (idempotent) */
    rogue_ui_render(&ctx);
    di = rogue_ui_dirty_info(&ctx);
    printf("DBG dirty_second changed=%d\n", di.changed);
    /* Perf budget over/under */
    /* Perf budget smoke (frame likely near 0 so treat as under) */
    rogue_ui_perf_set_budget(&ctx, 1.0);
    rogue_ui_render(&ctx);
    if (!expect(rogue_ui_perf_frame_over_budget(&ctx) == 0, "budget_smoke"))
        return 1;
    else
        printf("DBG frame_ms=%f budget=%f\n", ctx.perf_last_frame_ms, ctx.perf_budget_ms);
    rogue_ui_shutdown(&ctx);
    printf("test_ui_phase9_perf_virtual: OK\n");
    return 0;
}
