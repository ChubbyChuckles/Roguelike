#include "../overlay_core.h"
#include "../overlay_input.h"
#include "../overlay_theme.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_shortcuts(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("shortcuts", "Shortcuts", 1180, 540, 420))
        return;
    const OverlayTheme* th = overlay_theme_get();
    (void) th;
    overlay_label("Keyboard Cheat Sheet:");
    overlay_label("  F1: Toggle overlay");
    overlay_label("  Ctrl+K: Global Search");
    overlay_label("  Ctrl+Shift+P: Command Palette");
    overlay_label("  Alt+Left/Right: Back/Forward history");
    overlay_label("  ?: Open this Shortcuts panel");
    overlay_label("  Alt+1..9: Show common panels (1 System, 2 Items, 3 Skills, 4 Map, 5 "
                  "Audio/VFX, 6 Entities, 7 Content Graph, 8 Validation, 9 Dialogue)");
    overlay_label("  Esc: Clear focus");
    overlay_label("  Ctrl+S: Save (contextual: Skills overrides, Map JSON)");
    overlay_label("  Ctrl+Z / Ctrl+Y: Undo/Redo (Map editor)");
    overlay_end_panel();
}

void rogue_overlay_register_panel_shortcuts(void)
{
    overlay_register_panel("shortcuts", "Shortcuts", panel_shortcuts, NULL);
}

#endif
