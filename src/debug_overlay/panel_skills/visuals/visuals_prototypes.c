#include "visuals_internal.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

void rogue_visuals_draw_prototypes(RogueSkillVisualParams* vis)
{
    (void) vis;
    /* Status Tags multiselect (prototype) */
    static unsigned int status_mask = 0;
    const char* status_items[] = {"Burn", "Freeze", "Shock", "Bleed", "Poison", "Slow"};
    overlay_multiselect_bits("Status Tags", status_items,
                             (int) (sizeof status_items / sizeof status_items[0]), &status_mask);

    /* Combo Chain editor prototype */
    static int combo_count = 0;
    static char combo_entries[8][64];
    overlay_list_editor("Combo Chain (prototype)", combo_entries, &combo_count, 8, 64);
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
