#include "../../src/debug_overlay/overlay_core.h"
#include "../../src/debug_overlay/overlay_input.h"
#include "../../src/debug_overlay/overlay_widgets.h"
#include <assert.h>
#include <string.h>

int main(void)
{
#if ROGUE_ENABLE_DEBUG_OVERLAY
    overlay_set_enabled(1);
    /* Simulate frame and interactions headless. */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(50, 50, 0, 1);
    overlay_input_set_capture(1, 1);

    int changed = 0;
    char buf[32];
    strcpy(buf, "hi");

    if (overlay_begin_panel("Test", 10, 10, 200))
    {
        overlay_label("Hello");
        changed |= overlay_button("Click");
        int v = 0;
        changed |= overlay_checkbox("Box", &v);
        int iv = 3;
        changed |= overlay_slider_int("I", &iv, 0, 10);
        float fv = 0.5f;
        changed |= overlay_slider_float("F", &fv, 0.f, 1.f);
        changed |= overlay_input_text("T", buf, sizeof(buf));
        overlay_end_panel();
    }
    /* We don't assert specific interactions, just ensure no crash and API returns sane values. */
    assert(buf[0] != '\0');
#else
    /* When disabled, APIs are stubs. Ensure no crash. */
    overlay_set_enabled(0);
#endif
    return 0;
}
