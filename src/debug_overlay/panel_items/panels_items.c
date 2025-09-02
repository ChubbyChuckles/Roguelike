/* Items panel - aligned with current loot/inventory APIs. */
#include "../../core/app/app_state.h"
#include "../../core/inventory/inventory.h"
#include "../../core/loot/item_debug.h"
#include "../../core/loot/loot_item_defs.h"
#include "../overlay_commands.h"
#include "../overlay_core.h"
#include "../overlay_input.h"
#include "../overlay_toast.h"
#include "../widgets/overlay_widgets.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* File-scope state used by qsort comparator */
static int s_items_sort_col = 0; /* 0 Name, 1 Id, 2 Rarity, 3 Category, 4 Qty */
static int s_items_sort_dir = 1; /* +1 asc, -1 desc */
/* External navigation hook: pending selection request (item index in defs) */
static int s_items_select_request = -1;

static int ci_cmp(const char* a, const char* b)
{
    if (!a)
        a = "";
    if (!b)
        b = "";
    while (*a && *b)
    {
        int ca = tolower((unsigned char) *a);
        int cb = tolower((unsigned char) *b);
        if (ca != cb)
            return (ca < cb) ? -1 : 1;
        ++a;
        ++b;
    }
    if (*a == *b)
        return 0;
    return (*a) ? 1 : -1;
}

static int contains_ci(const char* hay, const char* needle)
{
    if (!needle || !*needle)
        return 1;
    if (!hay)
        return 0;
    size_t nlen = strlen(needle);
    for (const char* p = hay; *p; ++p)
    {
        size_t i = 0;
        while (i < nlen && p[i] &&
               tolower((unsigned char) p[i]) == tolower((unsigned char) needle[i]))
            ++i;
        if (i == nlen)
            return 1;
    }
    return 0;
}

static int items_idx_cmp(const void* ap, const void* bp)
{
    int ia = *(const int*) ap;
    int ib = *(const int*) bp;
    const RogueItemDef* da = rogue_item_def_at(ia);
    const RogueItemDef* db = rogue_item_def_at(ib);
    if (!da || !db)
        return 0;
    int v = 0;
    switch (s_items_sort_col)
    {
    default:
    case 0:
        v = ci_cmp(da->name, db->name);
        break;
    case 1:
        v = ci_cmp(da->id, db->id);
        break;
    case 2:
        v = (da->rarity - db->rarity);
        break;
    case 3:
        v = ((int) da->category - (int) db->category);
        break;
    case 4:
        v = (rogue_inventory_get_count(ia) - rogue_inventory_get_count(ib));
        break;
    }
    if (s_items_sort_dir < 0)
        v = -v;
    return (v < 0) ? -1 : (v > 0 ? 1 : 0);
}

/* Command callback: open the Items panel */
static void items_cmd_open(void* user)
{
    (void) user;
    overlay_set_panel_visible("items", 1);
}

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
        /* Headers include fields we can sort by; clicking a header toggles direction. */
        const char* headers[] = {"Name", "Id", "Rarity", "Category", "Qty"};
        static int s_selected = -1;  /* persist selection across frames */
        static int s_row_offset = 0; /* virtualized row offset (rows, not pixels) */
        static int s_sort_col = 0;
        static int s_sort_dir = 1; /* +1 asc, -1 desc */
        /* Build filtered list of item indices based on qty>0 and UI filters */
        int total_defs = rogue_item_defs_count();
        int* idx = (int*) malloc(sizeof(int) * (total_defs > 0 ? total_defs : 1));
        int nidx = 0;
        for (int i = 0; i < total_defs; ++i)
        {
            int qty = rogue_inventory_get_count(i);
            if (qty <= 0)
                continue;
            const RogueItemDef* d = rogue_item_def_at(i);
            if (!d)
                continue;
            if (filter_rarity >= 0 && d->rarity != filter_rarity)
                continue;
            if (d->level_req < level_min || d->level_req > level_max)
                continue;
            if (search[0] != '\0')
            {
                int match = contains_ci(d->name, search) || contains_ci(d->id, search);
                if (!match)
                    continue;
            }
            idx[nidx++] = i;
        }
        /* Sorting comparator according to selected column */
        if (nidx > 1)
        {
            s_items_sort_col = s_sort_col;
            s_items_sort_dir = s_sort_dir;
            qsort(idx, (size_t) nidx, sizeof(int), items_idx_cmp);
        }
        /* Handle external selection request: set selection and scroll into view */
        if (s_items_select_request >= 0)
        {
            s_selected = s_items_select_request;
            /* Find its row position in the current filtered+sorted index list */
            int pos = -1;
            for (int k = 0; k < nidx; ++k)
            {
                if (idx[k] == s_items_select_request)
                {
                    pos = k;
                    break;
                }
            }
            if (pos >= 0)
            {
                /* Ensure pos is visible; center it if possible */
                int target_first = pos - 2;
                if (target_first < 0)
                    target_first = 0;
                /* visible_rows computed below; guard against zero */
                int vis_rows_guess = overlay_prefs_get_int("items.visible_rows", 20);
                if (vis_rows_guess < 1)
                    vis_rows_guess = 1;
                int max_first_guess = (nidx > vis_rows_guess) ? (nidx - vis_rows_guess) : 0;
                if (target_first > max_first_guess)
                    target_first = max_first_guess;
                s_row_offset = target_first;
            }
            s_items_select_request = -1;
        }
        /* Virtualization sizing */
        int total_rows = nidx;
        /* Tuning knobs for virtualization: visible rows and row height, persisted */
        static int s_visible_rows = -1; /* cached pref */
        static int s_row_height = -1;   /* cached pref */
        if (s_visible_rows < 0)
        {
            s_visible_rows = overlay_prefs_get_int("items.visible_rows", 20);
            if (s_visible_rows < 5)
                s_visible_rows = 5;
            if (s_visible_rows > 100)
                s_visible_rows = 100;
        }
        if (s_row_height < 0)
        {
            /* Tuned default row height for denser lists (perf-tested) */
            s_row_height = overlay_prefs_get_int("items.row_height", 16);
            if (s_row_height < 12)
                s_row_height = 12;
            if (s_row_height > 48)
                s_row_height = 48;
        }
        int visible_rows = s_visible_rows;
        if (visible_rows > total_rows)
            visible_rows = total_rows;
        int max_first_row = (total_rows > visible_rows) ? (total_rows - visible_rows) : 0;
        if (s_row_offset > max_first_row)
            s_row_offset = max_first_row;
        /* Keyboard & mouse scroll: adjust row offset */
        {
            const OverlayInputState* in = overlay_input_get();
            int step = (in && in->key_shift_down) ? 5 : 1;
            if (in && in->key_down_pressed)
                s_row_offset += step;
            if (in && in->key_up_pressed)
                s_row_offset -= step;
            if (in && in->key_home_pressed)
                s_row_offset = 0;
            if (in && in->key_end_pressed)
                s_row_offset = max_first_row;
            /* Mouse wheel over the table */
            int wheel = 0;
            if (overlay_table_hover_wheel(&wheel) && wheel != 0)
            {
                /* SDL: positive wheel = up => decrease offset */
                s_row_offset -= wheel;
            }
            if (s_row_offset < 0)
                s_row_offset = 0;
            if (s_row_offset > max_first_row)
                s_row_offset = max_first_row;
        }
        if (overlay_table_begin("items_inv", headers, 5, &s_sort_col, &s_sort_dir, search))
        {
            /* Apply row style according to tuning */
            overlay_table_set_row_style(s_row_height, 1);
            int first = s_row_offset;
            int visible = visible_rows > 0 ? visible_rows : 1;
            for (int k = 0; k < visible && (first + k) < nidx; ++k)
            {
                int i = idx[first + k];
                int qty = rogue_inventory_get_count(i);
                const RogueItemDef* d = rogue_item_def_at(i);
                char qty_s[16];
                snprintf(qty_s, sizeof qty_s, "%d", qty);
                char rar_s[16];
                snprintf(rar_s, sizeof rar_s, "%d", d ? d->rarity : 0);
                char cat_s[16];
                snprintf(cat_s, sizeof cat_s, "%d", d ? (int) d->category : 0);
                const char* cells[] = {d ? d->name : "?", d ? d->id : "?", rar_s, cat_s, qty_s};
                (void) overlay_table_row(cells, 5, i, &s_selected);
            }
            /* Draw and handle a vertical scrollbar aligned with the table */
            if (overlay_table_scrollbar(total_rows, visible_rows, &s_row_offset))
            {
                if (s_row_offset < 0)
                    s_row_offset = 0;
                if (s_row_offset > max_first_row)
                    s_row_offset = max_first_row;
            }
            overlay_table_end();
        }
        overlay_slider_int("Scroll", &s_row_offset, 0, max_first_row);
        if (overlay_slider_int("Visible Rows", &s_visible_rows, 5, 100))
        {
            overlay_prefs_set_int("items.visible_rows", s_visible_rows);
        }
        if (overlay_slider_int("Row Height", &s_row_height, 12, 48))
        {
            overlay_prefs_set_int("items.row_height", s_row_height);
        }
        if (idx)
            free(idx);
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

/* Optional: allow external navigation to select a row (used by global search). */
void rogue_overlay_items_set_selected_index(int index)
{
    /* Defer actual UI selection to next panel frame to avoid reentrancy */
    s_items_select_request = index;
}

void rogue_overlay_register_panel_items(void)
{
    overlay_register_panel("items", "Items", panel_items, NULL);
    /* Expose command to open Create Item wizard */
    overlay_command_register("Items: Create New", items_cmd_open, NULL);
}

#endif
