#include "ui/core/ui_context.h"
#include <assert.h>
#include <string.h>

static void set_input(RogueUIContext* ctx, float mx, float my, int pressed, int released)
{
    RogueUIInputState in;
    memset(&in, 0, sizeof in);
    in.mouse_x = mx;
    in.mouse_y = my;
    in.mouse_pressed = pressed;
    in.mouse_released = released;
    in.mouse_down = pressed && !released;
    rogue_ui_set_input(ctx, &in);
}

static int drain_for_kind(RogueUIContext* ctx, int kind, int* a_out)
{
    RogueUIEvent ev;
    int found = 0;
    while (rogue_ui_poll_event(ctx, &ev))
    {
        if (ev.kind == kind)
        {
            if (a_out)
                *a_out = ev.a;
            found = 1;
        }
    }
    return found;
}

int main(void)
{
    RogueUIContext ctx;
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 256;
    cfg.seed = 123;
    cfg.arena_size = 8192;
    assert(rogue_ui_init(&ctx, &cfg));
    int item_ids[16] = {0};
    int counts[16] = {0};
    item_ids[0] = 105;
    counts[0] = 1;
    item_ids[1] = 215;
    counts[1] = 1; /* second item has higher dmg proxy (id%100) */
    int first_visible = 0, vis_count = 0;
    (void) vis_count;

    /* Frame 1: hover slot 0 */
    rogue_ui_begin(&ctx, 16.0);
    set_input(&ctx, 5.0f, 5.0f, 0, 0); /* inside first cell (padding 2 + cell 32) */
    rogue_ui_inventory_grid(&ctx, (RogueUIRect){0, 0, 120, 120}, "inv", 16, 4, item_ids, counts, 32,
                            &first_visible, &vis_count);
    rogue_ui_end(&ctx);
    int a = -1;
    assert(drain_for_kind(&ctx, ROGUE_UI_EVENT_STAT_PREVIEW_SHOW, &a));
    assert(a == 0);

    /* Capture number of nodes, ensure a stat preview text node exists */
    int node_count = 0;
    const RogueUINode* nodes = rogue_ui_nodes(&ctx, &node_count);
    int found = 0;
    for (int i = 0; i < node_count; i++)
    {
        if (nodes[i].kind == 1 && nodes[i].text && strstr(nodes[i].text, "DMG"))
        {
            found = 1;
            break;
        }
    }
    assert(found);

    /* Frame 2: hover slot 1 to trigger new preview show event */
    rogue_ui_begin(&ctx, 16.0);
    set_input(&ctx, 40.0f, 5.0f, 0, 0); /* x ~ cell0 x + cell_size + spacing */
    rogue_ui_inventory_grid(&ctx, (RogueUIRect){0, 0, 120, 120}, "inv", 16, 4, item_ids, counts, 32,
                            &first_visible, &vis_count);
    rogue_ui_end(&ctx);
    a = -1;
    assert(drain_for_kind(&ctx, ROGUE_UI_EVENT_STAT_PREVIEW_SHOW, &a));
    assert(a == 1);

    /* Frame 3: move mouse away to hide */
    rogue_ui_begin(&ctx, 16.0);
    set_input(&ctx, 200.0f, 200.0f, 0, 0); /* outside */
    rogue_ui_inventory_grid(&ctx, (RogueUIRect){0, 0, 120, 120}, "inv", 16, 4, item_ids, counts, 32,
                            &first_visible, &vis_count);
    rogue_ui_end(&ctx);
    a = -1;
    assert(drain_for_kind(&ctx, ROGUE_UI_EVENT_STAT_PREVIEW_HIDE, &a));
    assert(a == 1); /* last preview slot was 1 */

    return 0;
}
