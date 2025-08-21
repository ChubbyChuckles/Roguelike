#define SDL_MAIN_HANDLED 1
#include "ui/core/ui_context.h"
#include <assert.h>
#include <stdio.h>

static void frame(RogueUIContext* ui, RogueUIInputState in, int* ids, int* counts)
{
    rogue_ui_begin(ui, 16.0);
    rogue_ui_set_input(ui, &in);
    int first = 0, vis = 0;
    rogue_ui_inventory_grid(ui, (RogueUIRect){10, 10, 180, 120}, "inv_ctx", 20, 5, ids, counts, 28,
                            &first, &vis);
    rogue_ui_end(ui);
}

static int poll_kind(RogueUIContext* ui, int kind, int* a, int* b)
{
    RogueUIEvent e;
    int got = 0;
    while (rogue_ui_poll_event(ui, &e))
    {
        if (e.kind == kind)
        {
            if (a)
                *a = e.a;
            if (b)
                *b = e.b;
            got = 1;
        }
    }
    return got;
}

int main()
{
    RogueUIContext ui;
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 512;
    cfg.seed = 99;
    cfg.arena_size = 32 * 1024;
    assert(rogue_ui_init(&ui, &cfg));
    int ids[20] = {0};
    int counts[20] = {0};
    ids[3] = 111;
    counts[3] = 1; // single item to trigger menu
    int pad = 2, spacing = 2, cell = 28;
    int col = 3 % 5;
    int row = 3 / 5;
    RogueUIInputState in = {0}; // open context menu
    in.mouse_x = 10 + pad + col * (cell + spacing) + 4;
    in.mouse_y = 10 + pad + row * (cell + spacing) + 4;
    in.mouse2_pressed = 1;
    frame(&ui, in, ids, counts);
    int slot = -1;
    if (!poll_kind(&ui, ROGUE_UI_EVENT_CONTEXT_OPEN, &slot, NULL) || slot != 3)
    {
        printf("FAIL context open slot=%d\n", slot);
        rogue_ui_shutdown(&ui);
        return 1;
    }
    // navigate down twice then activate (select Compare index 2)
    in = (RogueUIInputState){0};
    in.key_down = 1;
    frame(&ui, in, ids, counts);
    in = (RogueUIInputState){0};
    in.key_down = 1;
    frame(&ui, in, ids, counts);
    in = (RogueUIInputState){0};
    in.key_activate = 1;
    frame(&ui, in, ids, counts);
    int sel_slot = -1, sel_index = -1;
    poll_kind(&ui, ROGUE_UI_EVENT_CONTEXT_SELECT, &sel_slot, &sel_index);
    int failures = 0;
    if (sel_slot != 3 || sel_index != 2)
    {
        printf("FAIL context select slot=%d index=%d\n", sel_slot, sel_index);
        failures++;
    }
    // open again then cancel via outside left-click
    in = (RogueUIInputState){0};
    in.mouse_x = 10 + pad + col * (cell + spacing) + 4;
    in.mouse_y = 10 + pad + row * (cell + spacing) + 4;
    in.mouse2_pressed = 1;
    frame(&ui, in, ids, counts);
    poll_kind(&ui, ROGUE_UI_EVENT_CONTEXT_OPEN, NULL, NULL);
    in = (RogueUIInputState){0};
    in.mouse_x = 5;
    in.mouse_y = 5;
    in.mouse_pressed = 1;
    frame(&ui, in, ids, counts);
    int cancel_slot = -1;
    poll_kind(&ui, ROGUE_UI_EVENT_CONTEXT_CANCEL, &cancel_slot, NULL);
    if (cancel_slot != 3)
    {
        printf("FAIL context cancel slot=%d\n", cancel_slot);
        failures++;
    }
    if (!failures)
        printf("test_ui_phase4_context_menu: OK\n");
    else
        printf("test_ui_phase4_context_menu: FAIL (%d)\n", failures);
    rogue_ui_shutdown(&ui);
    return failures ? 1 : 0;
}
