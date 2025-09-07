#include "panel_skills_visuals.h"
#include "../../../core/skills/skill_debug.h"
#include "../../../core/skills/skill_sprite_loader.h"
#include "../../overlay_core.h"
#include "../../widgets/overlay_widgets.h"
#include "../../widgets/overlay_widgets_internal.h" /* g_ui for positioning */
#include "../shared/panel_skills_shared.h"
#include <ctype.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY
void panel_skills_draw_visuals(int sel)
{
    overlay_label("Visuals");
    RogueSkillVisualParams vis;
    memset(&vis, 0, sizeof vis);
    if (rogue_skill_debug_get_visuals(sel, &vis) != 0)
        return; /* skill not found */
    int stype = 0;
    (void) rogue_skill_debug_get_type(sel, &stype);
    int vchanged = 0;
    /* Order of sections */
    rogue_visuals_draw_import_wizard(&vis, &vchanged);
    rogue_visuals_draw_core_fields(&vis, stype, &vchanged);
    rogue_visuals_draw_prototypes(&vis);
    rogue_visuals_draw_sprite_browser(&vis, &vchanged);
    rogue_visuals_draw_asset_picker(&vis, &vchanged);
    rogue_visuals_draw_sheet_editor(&vis, &vchanged);
    rogue_visuals_preview_section(&vis, &vchanged);
    rogue_visuals_dependency_viewer(&vis);
    if (vchanged)
    {
        (void) rogue_skill_debug_set_visuals(sel, &vis);
        panel_skills_save_overrides_and_refresh();
    }
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
