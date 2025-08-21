#include "ui/core/ui_context.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Phase 10.3 Automated navigation traversal test
   Goals:
   - Ensure directional navigation (Right/Left, Down/Up) can visit every focusable widget exactly
   once before wrap.
   - Detect focus traps (missing wrap or skipping a widget) in the directional heuristic.
   Strategy:
   - Build a stable 2x3 grid of buttons each frame so node indices are deterministic.
   - Perform one frame with no movement to establish initial focus (first focusable).
   - Simulate edge key presses (right arrow) per frame; record visited focus indices until we wrap.
   - Assert unique visit count equals total focusable count (6) and wrap returns to start after 6
   moves.
   - Vertical traversal sanity: ensure DOWN moves reach the second row for each column and wrap
   back. */

static void build_buttons(RogueUIContext* ui)
{
    for (int row = 0; row < 2; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            float x = (float) col * 40.0f;
            float y = (float) row * 30.0f;
            char label[8];
            snprintf(label, sizeof label, "B%d", row * 3 + col);
            rogue_ui_button(ui, (RogueUIRect){x, y, 32, 20}, label, 0x202020FFu, 0xFFFFFFFFu);
        }
    }
}

static int set_contains(int* arr, int count, int v)
{
    for (int i = 0; i < count; i++)
        if (arr[i] == v)
            return 1;
    return 0;
}

int main()
{
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 128;
    cfg.seed = 99u;
    cfg.arena_size = 4096;
    RogueUIContext ui;
    assert(rogue_ui_init(&ui, &cfg));

    /* Initial frame (no movement) to build buttons and establish first focus */
    rogue_ui_begin(&ui, 16.0);
    build_buttons(&ui);
    ui.focus_index = -1; /* force automatic selection of first focusable */
    rogue_ui_navigation_update(&ui);
    int initial = ui.focus_index;
    assert(initial >= 0);
    rogue_ui_end(&ui);

    /* Horizontal cycle (RIGHT key) */
    int visited[16];
    int visit_count = 0;
    memset(visited, -1, sizeof visited);
    for (int step = 0; step < 6; ++step)
    {
        rogue_ui_begin(&ui, 16.0);
        build_buttons(&ui);
        RogueUIInputState in = {0};
        in.key_right = 1;
        ui.input = in; /* simulate edge press */
        rogue_ui_navigation_update(&ui);
        int idx = ui.focus_index;
        assert(idx >= 0);
        if (step < 5)
        {
            assert(!set_contains(visited, visit_count, idx));
            visited[visit_count++] = idx;
        }
        else
        {
            /* On the 6th move we should wrap to initial */
            assert(idx == initial);
        }
        rogue_ui_end(&ui);
    }
    assert(visit_count == 5); /* plus initial makes 6 unique */

    /* Vertical traversal test: For each top-row button force focus and move DOWN once; expect
     * matching bottom-row index (top+3) */
    int vertical_matches = 0;
    for (int col = 0; col < 3; ++col)
    {
        int top_index = col; /* row-major order */
        ui.focus_index = top_index;
        rogue_ui_begin(&ui, 16.0);
        build_buttons(&ui);
        RogueUIInputState inDown = {0};
        inDown.key_down = 1;
        ui.input = inDown;
        rogue_ui_navigation_update(&ui);
        int bottom_idx = ui.focus_index;
        rogue_ui_end(&ui);
        assert(bottom_idx == top_index + 3); /* direct column counterpart */
        vertical_matches++;
    }
    assert(vertical_matches == 3);

    rogue_ui_shutdown(&ui);
    printf("PHASE10_NAV_TRAVERSAL_OK buttons=6 horizontal_unique=%d vertical=%d\n", visit_count + 1,
           vertical_matches);
    return 0;
}
