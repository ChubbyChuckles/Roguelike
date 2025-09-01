/* Items panel - aligned with current loot/inventory APIs. */
#include "../../core/inventory/inventory.h"
#include "../../core/loot/item_debug.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_items(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("items", "Items", 820, 10, 360))
        return;

    static int filter_rarity = -1; // -1 all
    static int level_min = 1, level_max = 50;
    static int give_count = 1;
    static int equip_slot = 0; // 0..N
    static char search[64] = {0};

    const char* rarities[] = {"All", "Common", "Uncommon", "Rare", "Epic", "Legendary"};
    (void) overlay_combo("Rarity", &filter_rarity, rarities, 6);
    overlay_input_text("Search", search, sizeof search);
    overlay_slider_int("Level Min", &level_min, 1, 100);
    overlay_slider_int("Level Max", &level_max, 1, 100);
    overlay_slider_int("Give Count", &give_count, 1, 99);
    overlay_slider_int("Equip Slot", &equip_slot, 0, 12);

    if (overlay_columns_begin(2, NULL))
    {
        /* Minimal placeholder actions using inventory APIs; original items_debug helpers
           were moved to loot/item_debug.* and inventory.* */
        if (overlay_button("Give Matching"))
        {
            /* Simple demo: add N of the first item def if available */
            if (rogue_item_defs_count() > 0)
            {
                (void) rogue_inventory_add(0, give_count);
            }
        }
        overlay_next_column();
        if (overlay_button("Equip Best"))
        {
            (void) equip_slot; /* TODO: wire equipment API */
        }
        overlay_columns_end();
    }

    overlay_label("Inventory Snapshot:");
    {
        const char* headers[] = {"Name", "Qty", "Power"};
        int sort_col = 0, sort_dir = 1, selected = -1;
        if (overlay_table_begin("items_inv", headers, 3, &sort_col, &sort_dir, NULL))
        {
            /* Snapshot: iterate defs and display counts > 0 */
            int n = rogue_item_defs_count();
            for (int i = 0; i < n; ++i)
            {
                int qty = rogue_inventory_get_count(i);
                if (qty <= 0)
                    continue;
                const RogueItemDef* d = rogue_item_def_at(i);
                char qty_s[16];
                char pow_s[16];
                snprintf(qty_s, sizeof qty_s, "%d", qty);
                /* Placeholder power column: base_value for now */
                snprintf(pow_s, sizeof pow_s, "%d", d ? d->base_value : 0);
                const char* cells[] = {d ? d->name : "?", qty_s, pow_s};
                (void) overlay_table_row(cells, 3, i, &selected);
            }
            overlay_table_end();
        }
    }
    overlay_end_panel();
}

void rogue_overlay_register_panel_items(void)
{
    overlay_register_panel("items", "Items", panel_items, NULL);
}

#endif
