#include "ui/core/ui_context.h"
#include <assert.h>
#include <string.h>

static void frame(RogueUIContext* ctx, RogueUIInputState in)
{
    rogue_ui_set_input(ctx, &in);
    rogue_ui_begin(ctx, 16.0);
}

int main()
{
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 256;
    cfg.arena_size = 8192;
    cfg.seed = 1234;
    RogueUIContext ui;
    assert(rogue_ui_init(&ui, &cfg));

    // Frame 1: create scroll container and children beyond its height
    frame(&ui, (RogueUIInputState){0});
    int scroll = rogue_ui_scroll_begin(&ui, (RogueUIRect){0, 0, 100, 80}, 200.0f);
    assert(scroll >= 0);
    float y = 0;
    for (int i = 0; i < 5; i++)
    {
        RogueUIRect r = {0, y, 100, 30};
        RogueUIRect ar = rogue_ui_scroll_apply(&ui, scroll, r);
        rogue_ui_button(&ui, ar, "Row", 0, 0);
        y += 32.0f;
    }
    float off0 = rogue_ui_scroll_offset(&ui, scroll);
    assert(off0 == 0.0f);
    rogue_ui_end(&ui);

    // Frame 2: simulate wheel scroll down
    frame(&ui, (RogueUIInputState){.wheel_delta = -1.0f});
    scroll = rogue_ui_scroll_begin(&ui, (RogueUIRect){0, 0, 100, 80}, 200.0f);
    y = 0;
    for (int i = 0; i < 5; i++)
    {
        RogueUIRect r = {0, y, 100, 30};
        RogueUIRect ar = rogue_ui_scroll_apply(&ui, scroll, r);
        rogue_ui_button(&ui, ar, "Row", 0, 0);
        y += 32.0f;
    }
    float off1 = rogue_ui_scroll_offset(&ui, scroll);
    assert(off1 > 0.0f);
    rogue_ui_end(&ui);

    // Tooltip test: hover hold
    frame(&ui, (RogueUIInputState){.mouse_x = 10, .mouse_y = 10});
    int btn = rogue_ui_button(&ui, (RogueUIRect){0, 0, 40, 20}, "Tip", 0x111111u, 0xffffffu);
    ui.hot_index = btn; // simulate hover
    rogue_ui_tooltip(&ui, btn, "Hello", 0x222222u, 0xffffffu, 200);
    rogue_ui_end(&ui);

    // Advance time beyond delay
    frame(&ui, (RogueUIInputState){.mouse_x = 10, .mouse_y = 10});
    ui.time_ms += 250.0; // manually force time
    btn = rogue_ui_button(&ui, (RogueUIRect){0, 0, 40, 20}, "Tip", 0, 0);
    ui.hot_index = btn;
    int tip_panel = rogue_ui_tooltip(&ui, btn, "Hello", 0, 0, 200);
    assert(tip_panel >= 0);
    rogue_ui_end(&ui);

    // Navigation test: create interactive widgets and simulate Tab
    frame(&ui, (RogueUIInputState){0});
    int b1 = rogue_ui_button(&ui, (RogueUIRect){0, 0, 30, 20}, "A", 0, 0);
    int b2 = rogue_ui_button(&ui, (RogueUIRect){40, 0, 30, 20}, "B", 0, 0);
    (void) b1;
    (void) b2;
    ui.focus_index = -1;
    ui.input.key_tab = 1;
    rogue_ui_navigation_update(&ui);
    assert(ui.focus_index == b1 || ui.focus_index == b2);
    ui.input.key_tab = 1;
    int prev = ui.focus_index;
    rogue_ui_navigation_update(&ui);
    assert(ui.focus_index != prev); /* moved to next focusable */
    rogue_ui_end(&ui);

    return 0;
}
