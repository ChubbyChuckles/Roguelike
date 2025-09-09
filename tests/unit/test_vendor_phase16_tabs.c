#include "../../src/core/app/app_state.h"
#include "../../src/core/vendor/vendor_ui.h"
#include <assert.h>
#include <stdio.h>

/* Minimal test: verify tab getters/setters and wrap via input-like logic.
   We don't simulate SDL; just state transitions. */

int main(void)
{
    rogue_app_state_maybe_init();
    g_app.show_vendor_panel = 1;
    rogue_vendor_tab_set(0);
    assert(rogue_vendor_tab_get() == 0);
    rogue_vendor_tab_set(2);
    assert(rogue_vendor_tab_get() == 2);
    rogue_vendor_tab_set(99);
    assert(rogue_vendor_tab_get() == 3);
    rogue_vendor_tab_set(-5);
    assert(rogue_vendor_tab_get() == 0);
    printf("VENDOR_PHASE16_TABS_OK\n");
    return 0;
}
