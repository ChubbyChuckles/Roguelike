/* Verifies that overlay_core API is available and feature flag is propagated. */
#include "../../src/debug_overlay/overlay_core.h"
#include <stdio.h>

int main(void)
{
#if ROGUE_ENABLE_DEBUG_OVERLAY
    overlay_set_enabled(1);
    if (!overlay_is_enabled())
    {
        fprintf(stderr, "overlay should be enabled when flag on\n");
        return 1;
    }
#else
    /* When disabled, stubs should always report disabled and be no-ops */
    overlay_set_enabled(1);
    if (overlay_is_enabled() != 0)
    {
        fprintf(stderr, "overlay should be disabled when flag off\n");
        return 1;
    }
#endif
    return 0;
}
