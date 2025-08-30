#include "../../src/debug_overlay/overlay_core.h"
#include "../../src/debug_overlay/overlay_input.h"
#include "../../src/debug_overlay/overlay_widgets.h"
#include <assert.h>
#include <string.h>

int main(void)
{
#if ROGUE_ENABLE_DEBUG_OVERLAY
    overlay_set_enabled(1);

    char buf[16];
    strcpy(buf, "ab");

    /* Frame 1: Click to focus (no text yet) */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(10, 32, 0, 1); /* inside default panel/input rect */
    if (overlay_begin_panel("T", 0, 0, 200))
    {
        int ch = overlay_input_text("T", buf, sizeof(buf));
        overlay_end_panel();
    }
    assert(strcmp(buf, "ab") == 0);

    /* Frame 2: Type 'C' at end (focused from prior frame) */
    overlay_input_begin_frame();
    overlay_input_simulate_text("C");
    if (overlay_begin_panel("T", 0, 0, 200))
    {
        int ch = overlay_input_text("T", buf, sizeof(buf));
        assert(ch == 1);
        overlay_end_panel();
    }
    assert(strcmp(buf, "abC") == 0);

    /* Frame 3: Move caret left, then backspace to remove 'C' */
    overlay_input_begin_frame();
    overlay_input_simulate_key_left();
    overlay_input_simulate_key_backspace();
    overlay_input_simulate_mouse(10, 32, 0, 1);
    if (overlay_begin_panel("T", 0, 0, 200))
    {
        (void) overlay_input_text("T", buf, sizeof(buf));
        overlay_end_panel();
    }
    assert(strcmp(buf, "ab") == 0);

    /* Frame 4: Move caret to start, then insert at front */
    overlay_input_begin_frame();
    overlay_input_simulate_key_home();
    overlay_input_simulate_text("Z");
    if (overlay_begin_panel("T", 0, 0, 200))
    {
        (void) overlay_input_text("T", buf, sizeof(buf));
        overlay_end_panel();
    }
    assert(strcmp(buf, "Zab") == 0);
#else
    overlay_set_enabled(0);
#endif
    return 0;
}
