#include "../../src/debug_overlay/overlay_core.h"
#include "../../src/debug_overlay/overlay_input.h"
#include "../../src/debug_overlay/widgets/overlay_widgets.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
#if ROGUE_ENABLE_DEBUG_OVERLAY
    overlay_set_enabled(1);
    /* Build a small table and simulate header clicks and row selection */
    const char* headers[] = {"ColA", "ColB", "ColC"};
    const char* row0[] = {"a0", "b0", "c0"};
    const char* row1[] = {"a1", "b1", "c1"};
    int sort_col = 0, sort_dir = 1;
    int selected = -1;

    /* Frame 1: draw table (no interaction) */
    overlay_input_begin_frame();
    if (overlay_begin_panel("TableT", 10, 10, 240))
    {
        int begun = overlay_table_begin("t1", headers, 3, &sort_col, &sort_dir, NULL);
        assert(begun);
        overlay_table_end();
        overlay_end_panel();
    }

    /* Frame 2: click header 0 -> toggles dir to descending (-1) */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(20, 40, 0, 1);
    if (overlay_begin_panel("TableT", 10, 10, 240))
    {
        (void) overlay_table_begin("t1", headers, 3, &sort_col, &sort_dir, NULL);
        assert(sort_col == 0);
        assert(sort_dir == -1);
        overlay_table_end();
        overlay_end_panel();
    }

    /* Frame 3: click header 1 -> switches sort col and sets dir to ascending (+1)
        Header 1 starts around x ~98 for this panel; click within its bounds. */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(110, 40, 0, 1);
    if (overlay_begin_panel("TableT", 10, 10, 240))
    {
        (void) overlay_table_begin("t1", headers, 3, &sort_col, &sort_dir, NULL);
        assert(sort_col == 1);
        assert(sort_dir == 1);
        overlay_table_end();
        overlay_end_panel();
    }

    /* Frame 4: draw rows and click second row to select it */
    overlay_input_begin_frame();
    /* Click within the y-range of second row for this panel layout */
    overlay_input_simulate_mouse(20, 136, 0, 1);
    if (overlay_begin_panel("TableT", 10, 10, 240))
    {
        (void) overlay_table_begin("t1", headers, 3, &sort_col, &sort_dir, NULL);
        (void) overlay_table_row(row0, 3, 0, &selected);
        int changed = overlay_table_row(row1, 3, 1, &selected);
        assert(changed);
        assert(selected == 1);
        overlay_table_end();
        overlay_end_panel();
    }
    return 0;
#else
    return 0;
#endif
}

/* Optional micro-benchmark (guarded by macro) to sanity-check row style and virtualization
   performance in headless mode. Enabled when ROGUE_OVERLAY_TABLE_BENCH is defined. */
#if defined(ROGUE_ENABLE_DEBUG_OVERLAY) && defined(ROGUE_OVERLAY_TABLE_BENCH)
int bench_overlay_table(void)
{
    overlay_set_enabled(1);
    const int cols = 3;
    const char* headers[] = {"A", "B", "C"};
    int sort_col = 0, sort_dir = 1, selected = -1;
    const int total_rows = 20000;
    /* Simulate N frames and accumulate dt via overlay_last_dt(); overlay core may supply ~16ms */
    double acc_ms = 0.0;
    const int frames = 60;
    for (int f = 0; f < frames; ++f)
    {
        overlay_input_begin_frame();
        if (overlay_begin_panel("Bench", 10, 10, 640))
        {
            (void) overlay_table_begin("bench", headers, cols, &sort_col, &sort_dir, NULL);
            overlay_table_set_row_style(18, 2);
            int vis = 30;
            for (int i = 0; i < vis; ++i)
            {
                char a[16], b[16], c[16];
                snprintf(a, sizeof a, "a%d", i);
                snprintf(b, sizeof b, "b%d", i);
                snprintf(c, sizeof c, "c%d", i);
                const char* row[] = {a, b, c};
                (void) overlay_table_row(row, cols, i, &selected);
            }
            (void) overlay_table_scrollbar(total_rows, vis, &selected);
            overlay_table_end();
            overlay_end_panel();
        }
        /* Simulate a frame duration; in headless this may remain zero, so skip accumulation */
        float dt = overlay_last_dt();
        if (dt > 0.0f)
            acc_ms += (double) dt * 1000.0;
    }
    /* We don't assert timing; this is a smoke benchmark path */
    (void) acc_ms;
    return 0;
}
#endif
