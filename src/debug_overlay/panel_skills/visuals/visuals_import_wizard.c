#include "visuals_internal.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* Local static state for import wizard */
struct import_wizard_state
{
    int show;
    char staged[8][128];
    int staged_count;
};
static struct import_wizard_state g_wizard;

void rogue_visuals_draw_import_wizard(RogueSkillVisualParams* vis, int* vchanged)
{
    (void) vis;
    (void) vchanged;
    if (overlay_button("Asset Import Wizard"))
    {
        g_wizard.show = 1;
    }
    if (!g_wizard.show)
        return;

    overlay_begin_panel("Asset Import Wizard", 420, 60, 360);
    overlay_label("Drag & Drop sprite/audio files here or use 'Add Files' (prototype)");
    if (overlay_button("Add Files (scan assets/skills)") && g_wizard.staged_count < 8)
    {
        int remaining = 8 - g_wizard.staged_count;
        while (remaining-- > 0)
        {
            snprintf(g_wizard.staged[g_wizard.staged_count],
                     sizeof g_wizard.staged[g_wizard.staged_count], "demo_asset_%d.png",
                     g_wizard.staged_count);
            ++g_wizard.staged_count;
        }
    }
    for (int i = 0; i < g_wizard.staged_count; ++i)
        overlay_label(g_wizard.staged[i]);
    if (g_wizard.staged_count > 0 && overlay_button("Import (noop prototype)"))
    {
        g_wizard.staged_count = 0; /* future: copy + register */
    }
    if (overlay_button("Close"))
        g_wizard.show = 0;
    overlay_end_panel();
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
