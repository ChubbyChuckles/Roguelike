/* Items panel - aligned with current loot/inventory APIs. */
#include "../../content/json_io.h"
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* File-scope state used by qsort comparator */
static int s_items_sort_col = 0; /* 0 Name, 1 Id, 2 Rarity, 3 Category, 4 Qty */
static int s_items_sort_dir = 1; /* +1 asc, -1 desc */
/* External navigation hook: pending selection request (item index in defs) */
static int s_items_select_request = -1;

/* ---------------- Templates & Presets (Milestone 2.2) ---------------- */

/* Directory for storing item templates (JSON fragments we generate/consume) */
static const char* items_templates_dir(void) { return "build/templates/items"; }

static void ensure_dir_recursive(const char* path)
{
    /* Create nested directories best-effort; ignore EEXIST. */
    if (!path || !*path)
        return;
    char tmp[512];
    size_t n = strlen(path);
    if (n >= sizeof tmp)
        return;
    memcpy(tmp, path, n + 1);
    for (size_t i = 1; i < n; ++i)
    {
        if (tmp[i] == '/' || tmp[i] == '\\')
        {
            char c = tmp[i];
            tmp[i] = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            tmp[i] = c;
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

/* Sanitize a template name into a safe filename (lowercase, [a-z0-9_]) */
static void items_template_sanitize_filename(const char* name, char* out, size_t cap)
{
    size_t j = 0;
    for (size_t i = 0; name && name[i] && j + 1 < cap; ++i)
    {
        unsigned char ch = (unsigned char) name[i];
        if (ch >= 'A' && ch <= 'Z')
            ch = (unsigned char) (ch - 'A' + 'a');
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))
        {
            out[j++] = (char) ch;
        }
        else if (ch == ' ' || ch == '-' || ch == '.')
        {
            out[j++] = '_';
        }
        /* skip other characters */
    }
    out[j] = '\0';
}

/* Compose full path for a template name (adds .json) */
static void items_template_path(const char* name, char* out, size_t cap)
{
    char fname[128];
    items_template_sanitize_filename(name, fname, sizeof fname);
    if (fname[0] == '\0')
        snprintf(fname, sizeof fname, "template");
    snprintf(out, cap, "%s/%s.json", items_templates_dir(), fname);
}

/* Write current wizard fields to a JSON fragment on disk */
static int items_template_save_json(const char* name, const char* disp_name, int category,
                                    int level, int rarity, int stack_max, int base_value,
                                    int dmg_min, int dmg_max, int armor, int socket_min,
                                    int socket_max)
{
    ensure_dir_recursive(items_templates_dir());
    char path[512];
    items_template_path(name, path, sizeof path);
    char buf[1024];
    int n = snprintf(buf, sizeof buf,
                     "{\n"
                     "  \"name\": \"%s\",\n"
                     "  \"category\": %d,\n"
                     "  \"level\": %d,\n"
                     "  \"rarity\": %d,\n"
                     "  \"stack_max\": %d,\n"
                     "  \"base_value\": %d,\n"
                     "  \"dmg_min\": %d,\n"
                     "  \"dmg_max\": %d,\n"
                     "  \"armor\": %d,\n"
                     "  \"socket_min\": %d,\n"
                     "  \"socket_max\": %d\n"
                     "}\n",
                     disp_name ? disp_name : "", category, level, rarity, stack_max, base_value,
                     dmg_min, dmg_max, armor, socket_min, socket_max);
    if (n <= 0)
        return -1;
    char err[128];
    return json_io_write_atomic(path, buf, (size_t) n, err, (int) sizeof err);
}

/* Very small helper to parse an integer field like "key": 123 from our own JSON */
static int parse_json_int_field(const char* json, const char* key, int* out)
{
    if (!json || !key || !out)
        return -1;
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p)
        return -2;
    p = strchr(p, ':');
    if (!p)
        return -3;
    ++p; /* after colon */
    while (*p == ' ' || *p == '\t')
        ++p;
    char* endp = NULL;
    long v = strtol(p, &endp, 10);
    if (endp == p)
        return -4;
    *out = (int) v;
    return 0;
}

/* Parse a string field like "name": "..." into out buffer */
static int parse_json_string_field(const char* json, const char* key, char* out, size_t cap)
{
    if (!json || !key || !out || cap == 0)
        return -1;
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p)
        return -2;
    p = strchr(p, ':');
    if (!p)
        return -3;
    ++p; /* after colon */
    while (*p && (*p == ' ' || *p == '\t'))
        ++p;
    if (*p != '"')
        return -4;
    ++p;
    size_t j = 0;
    while (*p && *p != '"' && j + 1 < cap)
    {
        out[j++] = *p++;
    }
    out[j] = '\0';
    return 0;
}

/* Load template JSON and apply to wizard fields */
static int items_template_load_apply(const char* name, char* out_disp_name, size_t disp_cap,
                                     int* category, int* level, int* rarity, int* stack_max,
                                     int* base_value, int* dmg_min, int* dmg_max, int* armor,
                                     int* socket_min, int* socket_max)
{
    char path[512];
    items_template_path(name, path, sizeof path);
    char* data = NULL;
    size_t len = 0;
    char err[128];
    if (json_io_read_file(path, &data, &len, err, (int) sizeof err) != 0 || !data)
        return -1;
    (void) parse_json_string_field(data, "name", out_disp_name, disp_cap);
    (void) parse_json_int_field(data, "category", category);
    (void) parse_json_int_field(data, "level", level);
    (void) parse_json_int_field(data, "rarity", rarity);
    (void) parse_json_int_field(data, "stack_max", stack_max);
    (void) parse_json_int_field(data, "base_value", base_value);
    (void) parse_json_int_field(data, "dmg_min", dmg_min);
    (void) parse_json_int_field(data, "dmg_max", dmg_max);
    (void) parse_json_int_field(data, "armor", armor);
    (void) parse_json_int_field(data, "socket_min", socket_min);
    (void) parse_json_int_field(data, "socket_max", socket_max);
    free(data);
    return 0;
}

/* Apply a curated preset quickly (built-in defaults) */
static void items_apply_preset(int preset_id, int* category, int* level, int* rarity,
                               int* stack_max, int* base_value, int* dmg_min, int* dmg_max,
                               int* armor, int* socket_min, int* socket_max)
{
    /* 0 Weapon, 1 Armor, 2 Consumable, 3 Gem, 4 Material */
    switch (preset_id)
    {
    default:
    case 0: /* Weapon Basic */
        *category = (int) ROGUE_ITEM_WEAPON;
        *level = 1;
        *rarity = 1;
        *stack_max = 1;
        *base_value = 10;
        *dmg_min = 1;
        *dmg_max = 3;
        *armor = 0;
        *socket_min = 0;
        *socket_max = 2;
        break;
    case 1: /* Armor Basic */
        *category = (int) ROGUE_ITEM_ARMOR;
        *level = 1;
        *rarity = 1;
        *stack_max = 1;
        *base_value = 12;
        *dmg_min = 0;
        *dmg_max = 0;
        *armor = 2;
        *socket_min = 0;
        *socket_max = 3;
        break;
    case 2: /* Consumable Basic */
        *category = (int) ROGUE_ITEM_CONSUMABLE;
        *level = 1;
        *rarity = 1;
        *stack_max = 5;
        *base_value = 5;
        *dmg_min = 0;
        *dmg_max = 0;
        *armor = 0;
        *socket_min = 0;
        *socket_max = 0;
        break;
    case 3: /* Gem Basic */
        *category = (int) ROGUE_ITEM_GEM;
        *level = 1;
        *rarity = 2;
        *stack_max = 1;
        *base_value = 15;
        *dmg_min = 0;
        *dmg_max = 0;
        *armor = 0;
        *socket_min = 0;
        *socket_max = 0;
        break;
    case 4: /* Material Basic */
        *category = (int) ROGUE_ITEM_MATERIAL;
        *level = 1;
        *rarity = 1;
        *stack_max = 20;
        *base_value = 2;
        *dmg_min = 0;
        *dmg_max = 0;
        *armor = 0;
        *socket_min = 0;
        *socket_max = 0;
        break;
    }
}

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

    /* Render breadcrumb for current selection if any */
    {
        static OverlayNavState s_bc; /* keep buffers stable */
        s_bc.panel_id = "items";
        s_bc.crumb_a[0] = '\0';
        s_bc.crumb_b[0] = '\0';
        s_bc.crumb_c[0] = '\0';
        overlay_nav_render_breadcrumb(&s_bc);
    }

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
                if (overlay_table_row(cells, 5, i, &s_selected))
                {
                    /* Update breadcrumb + history on selection change */
                    OverlayNavState st;
                    st.panel_id = "items";
                    st.sel_index = s_selected;
                    st.crumb_a[0] = '\0';
                    st.crumb_b[0] = '\0';
                    st.crumb_c[0] = '\0';
                    if (d)
                    {
                        strncpy(st.crumb_a, "Items", sizeof st.crumb_a - 1);
                        strncpy(st.crumb_b, d->name ? d->name : d->id, sizeof st.crumb_b - 1);
                        st.crumb_a[sizeof st.crumb_a - 1] = '\0';
                        st.crumb_b[sizeof st.crumb_b - 1] = '\0';
                    }
                    overlay_nav_set_current(&st);
                }
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

        /* Templates & Presets (curated + file-backed) */
        overlay_label("Templates & Presets");
        static int preset_idx = 0; /* 0 Weapon,1 Armor,2 Consumable,3 Gem,4 Material */
        const char* preset_names[] = {"Weapon Basic", "Armor Basic", "Consumable Basic",
                                      "Gem Basic", "Material Basic"};
        (void) overlay_combo("Preset", &preset_idx, preset_names,
                             (int) (sizeof preset_names / sizeof preset_names[0]));
        if (overlay_button("Apply Preset"))
        {
            items_apply_preset(preset_idx, &new_category, &new_level, &new_rarity, &new_stack_max,
                               &new_base_value, &new_dmg_min, &new_dmg_max, &new_armor,
                               &new_socket_min, &new_socket_max);
        }
        overlay_label("Custom Template (name)");
        static char templ_name[64] = "";
        overlay_input_text("Template Name", templ_name, sizeof templ_name);
        if (overlay_button("Save as Template"))
        {
            int rc =
                items_template_save_json(templ_name, new_name, new_category, new_level, new_rarity,
                                         new_stack_max, new_base_value, new_dmg_min, new_dmg_max,
                                         new_armor, new_socket_min, new_socket_max);
            if (rc == 0)
            {
                overlay_toast_push(OVERLAY_TOAST_INFO, "Template saved", 1800);
            }
            else
            {
                overlay_toast_push(OVERLAY_TOAST_ERROR, "Template save failed", 2200);
            }
        }
        if (overlay_button("Load Template"))
        {
            char loaded_name[64] = "";
            int rc = items_template_load_apply(
                templ_name, loaded_name, sizeof loaded_name, &new_category, &new_level, &new_rarity,
                &new_stack_max, &new_base_value, &new_dmg_min, &new_dmg_max, &new_armor,
                &new_socket_min, &new_socket_max);
            if (rc == 0)
            {
                if (loaded_name[0] != '\0')
                    snprintf(new_name, sizeof new_name, "%s", loaded_name);
                overlay_toast_push(OVERLAY_TOAST_INFO, "Template applied", 1800);
            }
            else
            {
                overlay_toast_push(OVERLAY_TOAST_ERROR, "Template load failed", 2200);
            }
        }
        if (overlay_button("Create from Preset"))
        {
            /* Apply selected preset, then create immediately using current id/name */
            items_apply_preset(preset_idx, &new_category, &new_level, &new_rarity, &new_stack_max,
                               &new_base_value, &new_dmg_min, &new_dmg_max, &new_armor,
                               &new_socket_min, &new_socket_max);
            /* If id is empty, suggest from name or preset */
            if (new_id[0] == '\0')
            {
                char base[64];
                if (new_name[0] != '\0')
                    snprintf(base, sizeof base, "%s", new_name);
                else
                    snprintf(base, sizeof base, "%s",
                             (preset_idx == 0)   ? "weapon_basic"
                             : (preset_idx == 1) ? "armor_basic"
                             : (preset_idx == 2) ? "consumable_basic"
                             : (preset_idx == 3) ? "gem_basic"
                                                 : "material_basic");
                /* sanitize base to id-like */
                char cand[ROGUE_MAX_ITEM_ID_LEN];
                size_t j = 0;
                for (size_t i = 0; base[i] && j + 1 < sizeof cand; ++i)
                {
                    char ch = base[i];
                    if (ch >= 'A' && ch <= 'Z')
                        ch = (char) (ch - 'A' + 'a');
                    if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))
                        cand[j++] = ch;
                    else if (ch == ' ' || ch == '-' || ch == '_')
                        cand[j++] = '_';
                }
                cand[j] = '\0';
                int attempt = 0;
                while (1)
                {
                    char try_id[ROGUE_MAX_ITEM_ID_LEN];
                    if (attempt == 0)
                        snprintf(try_id, sizeof try_id, "%s", cand);
                    else
                        snprintf(try_id, sizeof try_id, "%s_%d", cand, attempt);
                    if (rogue_item_def_index(try_id) < 0)
                    {
                        snprintf(new_id, sizeof new_id, "%s", try_id);
                        break;
                    }
                    if (++attempt > 999)
                        break;
                }
            }
            int idx = rogue_item_debug_create(new_id, new_name, (RogueItemCategory) new_category,
                                              new_level, new_stack_max, new_base_value, new_dmg_min,
                                              new_dmg_max, new_armor, new_rarity, new_socket_min,
                                              new_socket_max);
            if (idx >= 0)
            {
                char msg[192];
                snprintf(msg, sizeof msg, "Created item '%s' (#%d) from preset", new_id, idx);
                overlay_toast_push(OVERLAY_TOAST_INFO, msg, 2000);
                new_id[0] = '\0';
            }
            else
            {
                overlay_toast_push(OVERLAY_TOAST_ERROR, "Create from preset failed", 2400);
            }
        }

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

        /* M4.1: Live Preview (headless-safe, text only) */
        overlay_label("Preview");
        {
            /* Derive simple, deterministic stats preview from current inputs. */
            const char* cat_name = "?";
            /* cat_items is declared just below; but it may not yet be in scope in some builds.
               Safeguard by lazily mapping if out of range. */
            switch ((RogueItemCategory) new_category)
            {
            case ROGUE_ITEM_MISC:
                cat_name = "Misc";
                break;
            case ROGUE_ITEM_CONSUMABLE:
                cat_name = "Consumable";
                break;
            case ROGUE_ITEM_WEAPON:
                cat_name = "Weapon";
                break;
            case ROGUE_ITEM_ARMOR:
                cat_name = "Armor";
                break;
            case ROGUE_ITEM_GEM:
                cat_name = "Gem";
                break;
            case ROGUE_ITEM_MATERIAL:
                cat_name = "Material";
                break;
            default:
                break;
            }
            char stats_line[64];
            if (new_dmg_min > 0 || new_dmg_max > 0)
            {
                int dmin = new_dmg_min;
                int dmax = new_dmg_max;
                if (dmax < dmin)
                {
                    int t = dmin;
                    dmin = dmax;
                    dmax = t;
                }
                snprintf(stats_line, sizeof stats_line, "Damage %d-%d", dmin, dmax);
            }
            else if (new_armor > 0)
            {
                snprintf(stats_line, sizeof stats_line, "Armor %d", new_armor);
            }
            else
            {
                snprintf(stats_line, sizeof stats_line, "None");
            }
            char sockets_line[32];
            snprintf(sockets_line, sizeof sockets_line, "%d..%d", new_socket_min, new_socket_max);
            /* Simple, bounded value estimator (purely informational). */
            int avg_dmg = (new_dmg_min + new_dmg_max) / 2;
            if (avg_dmg < 0)
                avg_dmg = 0;
            int est_val = new_base_value + avg_dmg * 2 + (new_armor * 3) + (new_level * 5) +
                          (new_rarity * 25) + (new_socket_max * 15);
            if (est_val < 0)
                est_val = 0;

            const char* headers[] = {"Property", "Value"};
            int sc = 0, sd = 0;
            if (overlay_table_begin("item_preview", headers, 2, &sc, &sd, NULL))
            {
                const char* r0[] = {"Category", cat_name};
                (void) overlay_table_row(r0, 2, 0, NULL);
                char rar_buf[16];
                snprintf(rar_buf, sizeof rar_buf, "%d", new_rarity);
                const char* r1[] = {"Rarity", rar_buf};
                (void) overlay_table_row(r1, 2, 1, NULL);
                char lvl_buf[16];
                snprintf(lvl_buf, sizeof lvl_buf, "%d", new_level);
                const char* r2[] = {"Level", lvl_buf};
                (void) overlay_table_row(r2, 2, 2, NULL);
                const char* r3[] = {"Base Stats", stats_line};
                (void) overlay_table_row(r3, 2, 3, NULL);
                const char* r4[] = {"Sockets", sockets_line};
                (void) overlay_table_row(r4, 2, 4, NULL);
                char val_buf[32];
                snprintf(val_buf, sizeof val_buf, "%d", est_val);
                const char* r5[] = {"Est. Value", val_buf};
                (void) overlay_table_row(r5, 2, 5, NULL);
                overlay_table_end();
            }
        }

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

        /* M2.4: Batch Creator for Items (pattern expansion + preview/validation) */
        overlay_label("Batch Create Items (pattern)");
        static char batch_id_fmt[64] = "item_%02d";
        static char batch_name_fmt[64] = "Item %02d";
        static int batch_start = 1;
        static int batch_count = 3; /* cap small for preview clarity */
        overlay_input_text("Id Format (printf)", batch_id_fmt, sizeof batch_id_fmt);
        overlay_input_text("Name Format (printf)", batch_name_fmt, sizeof batch_name_fmt);
        overlay_slider_int("Start", &batch_start, -999, 999);
        overlay_slider_int("Count", &batch_count, 1, 32);
        /* Live preview */
        const char* p_headers[] = {"Id", "Name", "Status"};
        int p_sort_col = 0, p_sort_dir = 0;
        if (overlay_table_begin("items_batch_preview", p_headers, 3, &p_sort_col, &p_sort_dir,
                                NULL))
        {
            int shown = 0;
            for (int i = 0; i < batch_count; ++i)
            {
                int n = batch_start + i;
                char id[ROGUE_MAX_ITEM_ID_LEN];
                char nm[64];
                /* Guard against malformed formats */
                if (snprintf(id, sizeof id, batch_id_fmt, n) <= 0)
                    id[0] = '\0';
                if (snprintf(nm, sizeof nm, batch_name_fmt, n) <= 0)
                    nm[0] = '\0';
                const char* status = "OK";
                if (id[0] == '\0')
                    status = "Invalid id";
                else if ((int) strlen(id) >= (int) sizeof(id) - 1)
                    status = "Id too long";
                else if (rogue_item_def_index(id) >= 0)
                    status = "Duplicate id";
                const char* cells[] = {id[0] ? id : "<err>", nm[0] ? nm : "<err>", status};
                (void) overlay_table_row(cells, 3, i, NULL);
                if (++shown >= 16) /* keep table reasonable in height */
                    break;
            }
            overlay_table_end();
        }
        if (overlay_button("Apply Batch"))
        {
            int ok = 0, fail = 0;
            for (int i = 0; i < batch_count; ++i)
            {
                int n = batch_start + i;
                char id[ROGUE_MAX_ITEM_ID_LEN];
                char nm[64];
                if (snprintf(id, sizeof id, batch_id_fmt, n) <= 0)
                    id[0] = '\0';
                if (snprintf(nm, sizeof nm, batch_name_fmt, n) <= 0)
                    nm[0] = '\0';
                if (id[0] == '\0' || rogue_item_def_index(id) >= 0)
                {
                    ++fail;
                    continue;
                }
                int idx = rogue_item_debug_create(
                    id, nm[0] ? nm : id, (RogueItemCategory) new_category, new_level, new_stack_max,
                    new_base_value, new_dmg_min, new_dmg_max, new_armor, new_rarity, new_socket_min,
                    new_socket_max);
                if (idx >= 0)
                    ++ok;
                else
                    ++fail;
            }
            char msg[160];
            snprintf(msg, sizeof msg, "Batch create: %d ok, %d failed", ok, fail);
            overlay_toast_push(fail == 0 ? OVERLAY_TOAST_INFO : OVERLAY_TOAST_WARN, msg, 2400);
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
