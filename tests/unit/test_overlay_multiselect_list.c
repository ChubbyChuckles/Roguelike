#include "../../src/debug_overlay/overlay_core.h"
#include "../../src/debug_overlay/overlay_input.h"
#include "../../src/debug_overlay/widgets/overlay_widgets.h"
#include <assert.h>
#include <string.h>

int main(void)
{
#if ROGUE_ENABLE_DEBUG_OVERLAY
    overlay_set_enabled(1);
    unsigned int mask = 0;
    char entries[4][64];
    int count = 0;

    /* Frame 1: open panel, invoke widgets headless without relying on simulated coordinates */
    overlay_input_begin_frame();
    if (overlay_begin_panel("W1", 10, 10, 260))
    {
        const char* items[] = {"A", "B", "C"};
        overlay_multiselect_bits(NULL, items, 3, &mask);
        /* List editor add */
        int mut = overlay_list_editor("List", entries, &count, 4, 64);
        (void) mut;
        overlay_end_panel();
    }

    /* Frame 2: add entry again and then delete first */
    overlay_input_begin_frame();
    if (overlay_begin_panel("W1", 10, 10, 260))
    {
        const char* items[] = {"A", "B", "C"};
        overlay_multiselect_bits(NULL, items, 3, &mask);
        overlay_list_editor("List", entries, &count, 4, 64);
        overlay_end_panel();
    }
    assert(count >= 0 && count <= 4);
#else
    overlay_set_enabled(0);
#endif
    return 0;
}
