/* Debug overlay panels aggregator: declares and calls per-panel registrars. */
#include "overlay_core.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* Declarations of per-panel registrars */
void rogue_overlay_register_panel_system(void);
void rogue_overlay_register_panel_player(void);
void rogue_overlay_register_panel_skills(void);
void rogue_overlay_register_panel_entities(void);
void rogue_overlay_register_panel_map(void);
void rogue_overlay_register_panel_audiovfx(void);
void rogue_overlay_register_panel_items(void);
void rogue_overlay_register_panel_validation(void);
void rogue_overlay_register_panel_content_graph(void);

void rogue_overlay_register_default_panels(void)
{
    rogue_overlay_register_panel_system();
    rogue_overlay_register_panel_player();
    rogue_overlay_register_panel_skills();
    rogue_overlay_register_panel_entities();
    rogue_overlay_register_panel_map();
    rogue_overlay_register_panel_audiovfx();
    rogue_overlay_register_panel_items();
    rogue_overlay_register_panel_validation();
    rogue_overlay_register_panel_content_graph();
}

#else
void rogue_overlay_register_default_panels(void) {}
#endif
