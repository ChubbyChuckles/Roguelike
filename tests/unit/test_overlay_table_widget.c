#include "../../src/debug_overlay/overlay_core.h"
#include "../../src/debug_overlay/overlay_input.h"
#include "../../src/debug_overlay/overlay_widgets.h"
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
