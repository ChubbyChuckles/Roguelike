#ifdef ROGUE_HAVE_SDL
#include "../../src/debug_overlay/overlay_core.h"
#include <assert.h>
int main(void)
{
    overlay_init();
    overlay_set_enabled(0);
    assert(overlay_is_enabled() == 0);
    overlay_set_enabled(1);
    assert(overlay_is_enabled() == 1);
    overlay_new_frame(0.016f, 800, 600);
    overlay_render(); /* should be a no-op without panels */
    overlay_shutdown();
    return 0;
}
#else
int main(void) { return 0; }
#endif
