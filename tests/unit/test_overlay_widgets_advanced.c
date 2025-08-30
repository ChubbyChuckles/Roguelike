#include "../../src/debug_overlay/overlay_core.h"
#include "../../src/debug_overlay/overlay_input.h"
#include "../../src/debug_overlay/overlay_widgets.h"
#include <assert.h>
#include <string.h>

int main(void)
{
#if ROGUE_ENABLE_DEBUG_OVERLAY
    overlay_set_enabled(1);

    /* Frame 1: Combo click cycles selection and sets focus */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(20, 40, 0, 1); /* inside first widget at (10,10) panel */
    int idx = 0;
    const char* items[] = {"A", "B", "C"};
    if (overlay_begin_panel("Adv", 10, 10, 300))
    {
        int changed = overlay_combo("Mode", &idx, items, 3);
        overlay_end_panel();
        assert(changed == 1);
        assert(idx == 1);
    }

    /* Frame 2: Combo keyboard right advances selection while focused */
    overlay_input_begin_frame();
    overlay_input_simulate_key_right();
    overlay_input_set_capture(1, 1);
    if (overlay_begin_panel("Adv", 10, 10, 300))
    {
        int changed = overlay_combo("Mode", &idx, items, 3);
        overlay_end_panel();
        assert(changed == 1);
        assert(idx == 2);
    }

    /* Frame 3: Tree click toggles open */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(20, 40, 0, 1);
    int open = 0;
    if (overlay_begin_panel("Adv", 10, 10, 300))
    {
        int is_open = overlay_tree_node("Advanced", &open);
        overlay_tree_pop();
        overlay_end_panel();
        assert(open == 1);
        assert(is_open == 1);
    }

    /* Frame 4: Tree keyboard Enter toggles closed while focused */
    overlay_input_begin_frame();
    overlay_input_simulate_key_enter();
    overlay_input_set_capture(1, 1);
    if (overlay_begin_panel("Adv", 10, 10, 300))
    {
        int is_open = overlay_tree_node("Advanced", &open);
        overlay_tree_pop();
        overlay_end_panel();
        assert(open == 0);
        assert(is_open == 0);
    }

    /* Frame 5: ColorEdit – click on the R slider changes value */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(20, 40, 0, 1);
    unsigned char rgba[4] = {10, 20, 30, 40};
    if (overlay_begin_panel("Adv", 10, 10, 300))
    {
        int changed = overlay_color_edit_rgba("Tint", rgba);
        overlay_end_panel();
        assert(changed == 1);
        assert(rgba[0] != 10);
    }
#else
    /* When overlay is compiled out, ensure stubs don't crash. */
    overlay_set_enabled(0);
#endif
    return 0;
}
