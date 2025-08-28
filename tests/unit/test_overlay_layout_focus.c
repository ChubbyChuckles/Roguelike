#include "../../src/debug_overlay/overlay_core.h"
#include "../../src/debug_overlay/overlay_input.h"
#include "../../src/debug_overlay/overlay_widgets.h"
#include <assert.h>
#include <string.h>

int main(void)
{
#if ROGUE_ENABLE_DEBUG_OVERLAY
    overlay_set_enabled(1);
    overlay_input_begin_frame();
    int v = 0;
    int i = 5;
    float f = 0.5f;
    char buf[32];
    strcpy(buf, "ab");
    if (overlay_begin_panel("L", 0, 0, 200))
    {
        /* Two columns, ensure wrapping advances rows */
        int widths[2] = {90, 90};
        overlay_columns_begin(2, widths);
        overlay_button("B1");
        overlay_next_column();
        overlay_button("B2");
        overlay_columns_end();
        /* A row of focusables to tab through */
        overlay_checkbox("C", &v);
        overlay_slider_int("I", &i, 0, 10);
        overlay_slider_float("F", &f, 0.f, 1.f);
        overlay_input_text("T", buf, sizeof(buf));
        overlay_end_panel();
    }
    /* Simulate Shift+Tab then Tab */
    overlay_input_begin_frame();
    overlay_input_simulate_key_tab(1);
    overlay_input_set_capture(1, 1);
    overlay_input_begin_frame();
    overlay_input_simulate_key_tab(0);
    overlay_input_set_capture(1, 1);
    /* Basic sanity */
    assert(i >= 0 && i <= 10);
    assert(f >= 0.f && f <= 1.f);
#else
    overlay_set_enabled(0);
#endif
    return 0;
}
