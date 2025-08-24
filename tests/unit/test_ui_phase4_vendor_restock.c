#include "../../src/core/app/app_state.h"
#include "../../src/core/vendor/vendor_ui.h"
#include <assert.h>

int main(void)
{
    /* Initialize minimal app vendor timing fields */
    g_app.vendor_time_accum_ms = 5000.0;
    g_app.vendor_restock_interval_ms = 20000.0;
    float f = rogue_vendor_restock_fraction();
    assert(f > 0.24f && f < 0.26f);
    g_app.vendor_time_accum_ms = 0.0;
    f = rogue_vendor_restock_fraction();
    assert(f == 0.0f);
    g_app.vendor_time_accum_ms = 20000.0;
    f = rogue_vendor_restock_fraction();
    assert(f == 1.0f);
    g_app.vendor_time_accum_ms = 25000.0;
    f = rogue_vendor_restock_fraction();
    assert(f == 1.0f); /* clamped */
    return 0;
}
