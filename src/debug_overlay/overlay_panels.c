#include "../core/app/app_state.h"
#include "overlay_core.h"
#include "overlay_input.h"
#include "overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_system(void* user)
{
    (void) user;
    if (!overlay_begin_panel("System", 10, 10, 320))
        return;
    char buf[128];
    snprintf(buf, sizeof(buf), "FPS: %.1f  (%.3f ms)", g_app.fps, g_app.frame_ms);
    overlay_label(buf);
    snprintf(buf, sizeof(buf), "Draw calls: %d", g_app.frame_draw_calls);
    overlay_label(buf);
    snprintf(buf, sizeof(buf), "Tile quads: %d", g_app.frame_tile_quads);
    overlay_label(buf);
    int flags = g_app.show_metrics_overlay ? 1 : 0;
    if (overlay_checkbox("Show metrics overlay (F1)", &flags))
    {
        g_app.show_metrics_overlay = flags;
        overlay_set_enabled(flags);
    }
    overlay_end_panel();
}

void rogue_overlay_register_default_panels(void)
{
    overlay_register_panel("system", "System", panel_system, NULL);
}

#else
void rogue_overlay_register_default_panels(void) {}
#endif
