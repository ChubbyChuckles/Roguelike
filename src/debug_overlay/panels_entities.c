#include "../core/app/app_state.h"
#include "../core/entities/entity_debug.h"
#include "overlay_core.h"
#include "overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_entities(void* user)
{
    (void) user;
    if (!overlay_begin_panel("Entities", 820, 10, 360))
        return;
    static int selected_slot = -1;
    int total = rogue_entity_debug_count();
    char hdr[64];
    snprintf(hdr, sizeof hdr, "Alive: %d", total);
    overlay_label(hdr);
    int idxs[64];
    int n = rogue_entity_debug_list(idxs, (int) (sizeof idxs / sizeof idxs[0]));
    if (n <= 0)
    {
        overlay_label("No enemies alive");
        if (overlay_button("Spawn @ Player+2,0"))
        {
            int si = rogue_entity_debug_spawn_at_player(2.0f, 0.0f);
            if (si >= 0)
                selected_slot = si;
        }
        overlay_end_panel();
        return;
    }
    static int sel_i = 0;
    if (sel_i < 0)
        sel_i = 0;
    if (sel_i >= n)
        sel_i = n - 1;
    if (overlay_slider_int("Select", &sel_i, 0, n - 1))
        selected_slot = idxs[sel_i];
    if (selected_slot < 0 || selected_slot >= ROGUE_MAX_ENEMIES)
        selected_slot = idxs[sel_i];
    RogueEntityDebugInfo info;
    if (rogue_entity_debug_get_info(selected_slot, &info) == 0 && info.alive)
    {
        char line[128];
        snprintf(line, sizeof line, "Slot %d  Type %d  HP %d/%d", info.slot_index, info.type_index,
                 info.health, info.max_health);
        overlay_label(line);
        snprintf(line, sizeof line, "Pos: %.2f, %.2f", info.x, info.y);
        overlay_label(line);
        if (overlay_columns_begin(2, NULL))
        {
            if (overlay_button("Kill"))
                (void) rogue_entity_debug_kill(info.slot_index);
            overlay_next_column();
            if (overlay_button("Teleport -> Player"))
                (void) rogue_entity_debug_teleport(info.slot_index, g_app.player.base.pos.x,
                                                   g_app.player.base.pos.y);
            overlay_columns_end();
        }
    }
    else
        overlay_label("Selection not alive");

    if (overlay_button("Spawn @ Player+2,0"))
    {
        int si = rogue_entity_debug_spawn_at_player(2.0f, 0.0f);
        if (si >= 0)
        {
            selected_slot = si;
            sel_i = 0;
        }
    }

    overlay_end_panel();
}

void rogue_overlay_register_panel_entities(void)
{
    overlay_register_panel("entities", "Entities", panel_entities, NULL);
}

#endif
