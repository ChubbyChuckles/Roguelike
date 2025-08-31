#include "../core/app/app_state.h"
#include "overlay_core.h"
#include "overlay_input.h"
#include "overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* A small controller panel that lets users toggle which debug panels are visible. */
static void panel_selector(void* user)
{
    (void) user;
    /* Place in top-right corner with narrow width to minimize overlap */
    int w = 260;
    int x = g_app.viewport_w - w - 10;
    if (x < 10)
        x = 10;
    if (!overlay_begin_panel("Panels", x, 10, w))
        return;

    int count = overlay_get_panel_count();
    for (int i = 0; i < count; ++i)
    {
        const char* id = NULL;
        const char* name = NULL;
        int visible = 0;
        if (overlay_get_panel_info(i, &id, &name, &visible) == 0)
        {
            /* Do not allow hiding the selector itself, to avoid confusion */
            int disable_toggle = 0;
            if (id && name && id[0] && name[0])
            {
                if (id && (id[0] == 'p') && (id[1] == 'a') && (id[2] == 'n') && (id[3] == 'e') &&
                    (id[4] == 'l') && (id[5] == 's'))
                {
                    disable_toggle = 1;
                }
            }
            if (disable_toggle)
            {
                int on = 1;
                overlay_checkbox(name, &on); /* read-only visual */
            }
            else
            {
                int on = visible ? 1 : 0;
                if (overlay_checkbox(name, &on))
                    overlay_set_panel_visible_by_index(i, on);
            }
        }
    }
    overlay_end_panel();
}

void rogue_overlay_register_panel_panelselector(void)
{
    overlay_register_panel("panels", "Panels", panel_selector, NULL);
}

#endif
