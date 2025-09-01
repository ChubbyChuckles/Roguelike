/* Items panel - aligned with current loot/inventory APIs. */
#include "../../core/app/app_state.h"
#include "../../core/inventory/inventory.h"
#include "../../core/loot/item_debug.h"
#include "../../core/loot/loot_item_defs.h"
#include "../overlay_core.h"
#include "../overlay_toast.h"
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

    /* Split view: left = inventory list, right = details + create wizard */
    int left_w = 220;
    if (overlay_splitter_begin("items.split", &left_w, 180, 600))
    {
        overlay_label("Inventory Snapshot:");
        const char* headers[] = {"Name", "Qty", "Power"};
        static int s_selected = -1;  /* persist selection across frames */
        static int s_row_offset = 0; /* virtualized row offset (rows, not pixels) */
        int sort_col = 0, sort_dir = 1;
        /* Pre-scan to count visible rows (qty>0) */
        int total_defs = rogue_item_defs_count();
        int total_rows = 0;
        for (int i = 0; i < total_defs; ++i)
            if (rogue_inventory_get_count(i) > 0)
                total_rows++;
        /* Simple virtualization: assume a fixed number of visible rows for now. */
        int visible_rows = 20; /* reasonable default without querying internal UI metrics */
        if (visible_rows > total_rows)
            visible_rows = total_rows;
        int max_first_row = (total_rows > visible_rows) ? (total_rows - visible_rows) : 0;
        if (s_row_offset > max_first_row)
            s_row_offset = max_first_row;

        if (overlay_table_begin("items_inv", headers, 3, &sort_col, &sort_dir, NULL))
        {
            /* Virtualized emit: only draw rows in [first, first+count) among qty>0 entries */
            int first = s_row_offset;
            int visible = visible_rows > 0 ? visible_rows : 1;
            int drawn = 0;
            int kth = 0; /* counts over qty>0 entries */
            for (int i = 0; i < total_defs; ++i)
            {
                int qty = rogue_inventory_get_count(i);
                if (qty <= 0)
                    continue;
                if (kth < first)
                {
                    kth++;
                    continue;
                }
                if (drawn >= visible)
                    break;
                const RogueItemDef* d = rogue_item_def_at(i);
                char qty_s[16];
                char pow_s[16];
                snprintf(qty_s, sizeof qty_s, "%d", qty);
                /* Placeholder power column: base_value for now */
                snprintf(pow_s, sizeof pow_s, "%d", d ? d->base_value : 0);
                const char* cells[] = {d ? d->name : "?", qty_s, pow_s};
                (void) overlay_table_row(cells, 3, i, &s_selected);
                drawn++;
                kth++;
            }
            overlay_table_end();
        }
        /* Simple scroll control for virtualization (row offset) */
        overlay_slider_int("Scroll", &s_row_offset, 0, max_first_row);
        overlay_next_column();
        overlay_label("Create New Item");
        static char new_id[64] = "";
        static char new_name[64] = "";
        static int new_category = (int) ROGUE_ITEM_MISC;
        static int new_level = 1;
        static int new_rarity = 1; /* 1..5 */
        static int new_stack_max = 1;
        static int new_base_value = 0;
        static int new_dmg_min = 0, new_dmg_max = 0;
        static int new_armor = 0;
        static int new_socket_min = 0, new_socket_max = 0;

        const char* cat_items[] = {"Misc", "Consumable", "Weapon", "Armor", "Gem", "Material"};
        int cat_count = (int) (sizeof(cat_items) / sizeof(cat_items[0]));
        overlay_input_text("Id", new_id, sizeof new_id);
        overlay_input_text("Name", new_name, sizeof new_name);
        (void) overlay_combo("Category", &new_category, cat_items, cat_count);
        overlay_slider_int("Level", &new_level, 1, 100);
        overlay_slider_int("Rarity (1..5)", &new_rarity, 1, 5);
        overlay_slider_int("Stack Max", &new_stack_max, 1, 999);
        overlay_slider_int("Base Value", &new_base_value, 0, 100000);
        overlay_slider_int("Base Damage Min", &new_dmg_min, 0, 10000);
        overlay_slider_int("Base Damage Max", &new_dmg_max, 0, 10000);
        overlay_slider_int("Base Armor", &new_armor, 0, 10000);
        overlay_slider_int("Sockets Min", &new_socket_min, 0, 6);
        overlay_slider_int("Sockets Max", &new_socket_max, 0, 6);
        if (new_socket_max < new_socket_min)
            new_socket_max = new_socket_min;

        /* Duplicate-as-template: prefill from selected row */
        if (overlay_button("Duplicate From Selected") && s_selected >= 0)
        {
            const RogueItemDef* sd = rogue_item_def_at(s_selected);
            if (sd)
            {
                /* Prefill fields */
                snprintf(new_name, sizeof new_name, "%s_Copy", sd->name);
                new_category = (int) sd->category;
                new_level = sd->level_req;
                new_rarity = sd->rarity;
                new_stack_max = sd->stack_max;
                new_base_value = sd->base_value;
                new_dmg_min = sd->base_damage_min;
                new_dmg_max = sd->base_damage_max;
                new_armor = sd->base_armor;
                new_socket_min = sd->socket_min;
                new_socket_max = sd->socket_max;
                /* Suggest a unique id by suffixing _copy or _n */
                char base[ROGUE_MAX_ITEM_ID_LEN];
                snprintf(base, sizeof base, "%s_copy", sd->id);
                int attempt = 0;
                char cand[ROGUE_MAX_ITEM_ID_LEN];
                while (1)
                {
                    if (attempt == 0)
                        snprintf(cand, sizeof cand, "%s", base);
                    else
                        snprintf(cand, sizeof cand, "%s_%d", base, attempt);
                    if (rogue_item_def_index(cand) < 0)
                    {
                        snprintf(new_id, sizeof new_id, "%s", cand);
                        break;
                    }
                    attempt++;
                    if (attempt > 999)
                        break;
                }
            }
        }

        int can_create = (new_id[0] != '\0' && new_name[0] != '\0');
        if (!can_create)
            overlay_label("Fill required: Id and Name");
        if (overlay_button("Create") && can_create)
        {
            int idx = rogue_item_debug_create(new_id, new_name, (RogueItemCategory) new_category,
                                              new_level, new_stack_max, new_base_value, new_dmg_min,
                                              new_dmg_max, new_armor, new_rarity, new_socket_min,
                                              new_socket_max);
            if (idx >= 0)
            {
                char msg[192];
                snprintf(msg, sizeof msg, "Created item '%s' (#%d)", new_id, idx);
                overlay_toast_push(OVERLAY_TOAST_INFO, msg, 2000);
                /* Reset id to encourage creating another clean entry */
                new_id[0] = '\0';
            }
            else
            {
                overlay_toast_push(OVERLAY_TOAST_ERROR,
                                   "Create failed (check duplicate id or invalid fields)", 2400);
            }
        }
        overlay_splitter_end();
    }
    overlay_end_panel();
}

void rogue_overlay_register_panel_items(void)
{
    overlay_register_panel("items", "Items", panel_items, NULL);
}

#endif
