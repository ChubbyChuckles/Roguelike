/* Sprite Sheet Import Wizard – common format enumeration & staging */
#include "visuals_internal.h"

/*
    NOTE: This panel previously used several layout helpers (overlay_same_line, overlay_labelf,
    overlay_separator, overlay_begin_group, overlay_end_group, overlay_small_button) that do not
    exist in the current overlay public API. This caused unresolved externals at link time.
    For now we provide lightweight local shims so the feature compiles without requiring broader
    overlay changes. If richer layout primitives are added later, these can be removed and calls
    updated to the real API.
*/

/* Entire implementation (helpers + panel) is wrapped so we only compile when overlay enabled. */
#if ROGUE_ENABLE_DEBUG_OVERLAY

#include <stdarg.h>

static void overlay_labelf(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    overlay_label(buf);
}
static void overlay_separator(void)
{
    /* Simple visual divider */
    overlay_label("----------------------------------------");
}
static void overlay_same_line(void) { /* Layout not supported in minimal overlay; no-op */ }
static void overlay_begin_group(void) { (void) 0; }
static void overlay_end_group(void) { (void) 0; }
static int overlay_small_button(const char* label) { return overlay_button(label); }

/* Implementation below guarded by ROGUE_ENABLE_DEBUG_OVERLAY; helpers above provide any
   missing symbols for this translation unit only. */

/* Supported import targets (visual params fields) */
enum import_target
{
    IMPORT_CAST = 0,
    IMPORT_PROJECTILE = 1,
    IMPORT_IMPACT = 2,
    IMPORT_AOE = 3
};

struct staged_entry
{
    char path[256];
    int target; /* enum import_target */
};

struct import_wizard_state
{
    int show;
    /* Scan state */
    char filter[64];
    int auto_scan_performed;
    char scanned[1024][260];
    int scanned_count;
    int sel_index;
    int sel_target; /* enum import_target for staging */
    /* Staging */
    struct staged_entry staged[64];
    int staged_count;
    int scroll_offset; /* future: virtualized; currently unused */
};

static struct import_wizard_state g_wizard; /* zero-init */

static int has_img_ext(const char* p)
{
    const char* ext = strrchr(p, '.');
    if (!ext)
        return 0;
    char lo[8] = {0};
    for (int i = 0; ext[i] && i < 7; ++i)
        lo[i] = (char) tolower((unsigned char) ext[i]);
    return (strcmp(lo, ".png") == 0 || strcmp(lo, ".tga") == 0 || strcmp(lo, ".bmp") == 0);
}

static void wizard_scan_images(void)
{
#if defined(_WIN32)
    g_wizard.scanned_count = 0;
    typedef struct
    {
        char path[260];
        int depth;
    } Node;
    Node stack[512];
    int sp = 0;
    strncpy(stack[sp].path, "assets", sizeof stack[sp].path - 1);
    stack[sp].path[sizeof stack[sp].path - 1] = '\0';
    stack[sp].depth = 0;
    sp++;
    while (sp > 0)
    {
        Node cur = stack[--sp];
        char pattern[300];
        snprintf(pattern, sizeof pattern, "%s\\*", cur.path);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                continue;
            int is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            char child[300];
            snprintf(child, sizeof child, "%s\\%s", cur.path, fd.cFileName);
            if (is_dir && cur.depth < 5)
            {
                if (sp < (int) (sizeof stack / sizeof stack[0]))
                {
                    strncpy(stack[sp].path, child, sizeof stack[sp].path - 1);
                    stack[sp].path[sizeof stack[sp].path - 1] = '\0';
                    stack[sp].depth = cur.depth + 1;
                    sp++;
                }
            }
            else if (!is_dir)
            {
                if (!has_img_ext(child))
                    continue;
                if (g_wizard.scanned_count >=
                    (int) (sizeof g_wizard.scanned / sizeof g_wizard.scanned[0]))
                    break; /* cap */
                char norm[260];
                snprintf(norm, sizeof norm, "%s", child);
                for (char* p = norm; *p; ++p)
                    if (*p == '\\')
                        *p = '/';
                strncpy(g_wizard.scanned[g_wizard.scanned_count], norm,
                        sizeof g_wizard.scanned[g_wizard.scanned_count] - 1);
                g_wizard.scanned[g_wizard.scanned_count]
                                [sizeof g_wizard.scanned[g_wizard.scanned_count] - 1] = '\0';
                g_wizard.scanned_count++;
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#endif
    g_wizard.auto_scan_performed = 1;
}

static const char* target_label(int t)
{
    switch (t)
    {
    case IMPORT_CAST:
        return "Cast";
    case IMPORT_PROJECTILE:
        return "Projectile";
    case IMPORT_IMPACT:
        return "Impact";
    case IMPORT_AOE:
        return "AoE";
    default:
        return "?";
    }
}

void rogue_visuals_draw_import_wizard(RogueSkillVisualParams* vis, int* vchanged)
{
    if (overlay_button("Asset Import Wizard"))
        g_wizard.show = 1;
    if (!g_wizard.show)
        return;

    overlay_begin_panel("Asset Import Wizard", 520, 60, 520);
    overlay_label("Enumerates common sprite sheet formats (PNG/TGA/BMP). Stage entries then Import "
                  "→ assigns to selected targets.");

    /* Controls row */
    if (overlay_button("Scan Images"))
        wizard_scan_images();
    overlay_same_line();
    overlay_input_text("Filter", g_wizard.filter, (int) sizeof g_wizard.filter);
    overlay_same_line();
    const char* tgt_items[] = {"Cast", "Projectile", "Impact", "AoE"};
    overlay_combo("Assign Target", &g_wizard.sel_target, tgt_items, 4);

    if (!g_wizard.auto_scan_performed)
        overlay_label("(Hint: Click 'Scan Images' to populate list)");

    /* Build filtered list view */
    const char* hdr[] = {"Discovered Image Assets (click row to select)"};
    int sc = 0, sd = 0;
    int selrow = -1;
    if (overlay_table_begin("import_wizard_scan", hdr, 1, &sc, &sd, g_wizard.filter))
    {
        for (int i = 0; i < g_wizard.scanned_count; ++i)
        {
            const char* row[1] = {g_wizard.scanned[i]};
            if (overlay_table_row(row, 1, i, &selrow))
            {
                g_wizard.sel_index = i;
            }
        }
        overlay_table_end();
    }
    if (g_wizard.sel_index >= 0 && g_wizard.sel_index < g_wizard.scanned_count)
    {
        overlay_labelf("Selected: %s", g_wizard.scanned[g_wizard.sel_index]);
        if (overlay_button("Stage Selected"))
        {
            if (g_wizard.staged_count < (int) (sizeof g_wizard.staged / sizeof g_wizard.staged[0]))
            {
                struct staged_entry* e = &g_wizard.staged[g_wizard.staged_count++];
                strncpy(e->path, g_wizard.scanned[g_wizard.sel_index], sizeof e->path - 1);
                e->path[sizeof e->path - 1] = '\0';
                e->target = g_wizard.sel_target;
            }
        }
    }

    /* Staged list */
    overlay_separator();
    overlay_label("Staged Imports (will apply last staged per target)");
    for (int i = 0; i < g_wizard.staged_count; ++i)
    {
        overlay_begin_group();
        overlay_labelf("[%s] %s", target_label(g_wizard.staged[i].target), g_wizard.staged[i].path);
        overlay_same_line();
        if (overlay_small_button("Remove"))
        {
            /* Compact remove */
            if (i + 1 < g_wizard.staged_count)
                memmove(&g_wizard.staged[i], &g_wizard.staged[i + 1],
                        (size_t) (g_wizard.staged_count - (i + 1)) * sizeof g_wizard.staged[0]);
            g_wizard.staged_count--;
            i--; /* revisit this slot */
        }
        overlay_end_group();
    }

    int applied = 0;
    if (g_wizard.staged_count > 0 && overlay_button("Import → Assign Targets"))
    {
        /* Apply last staged for each target */
        char last_cast[256] = {0}, last_proj[256] = {0}, last_impact[256] = {0},
             last_aoe[256] = {0};
        for (int i = 0; i < g_wizard.staged_count; ++i)
        {
            switch (g_wizard.staged[i].target)
            {
            case IMPORT_CAST:
                strncpy(last_cast, g_wizard.staged[i].path, sizeof last_cast - 1);
                break;
            case IMPORT_PROJECTILE:
                strncpy(last_proj, g_wizard.staged[i].path, sizeof last_proj - 1);
                break;
            case IMPORT_IMPACT:
                strncpy(last_impact, g_wizard.staged[i].path, sizeof last_impact - 1);
                break;
            case IMPORT_AOE:
                strncpy(last_aoe, g_wizard.staged[i].path, sizeof last_aoe - 1);
                break;
            }
        }
        if (*last_cast)
        {
            strncpy(vis->cast_sprite_sheet, last_cast, sizeof vis->cast_sprite_sheet - 1);
            vis->cast_sprite_sheet[sizeof vis->cast_sprite_sheet - 1] = '\0';
            applied = 1;
        }
        if (*last_proj)
        {
            strncpy(vis->projectile_sprite, last_proj, sizeof vis->projectile_sprite - 1);
            vis->projectile_sprite[sizeof vis->projectile_sprite - 1] = '\0';
            applied = 1;
        }
        if (*last_impact)
        {
            strncpy(vis->impact_sprite, last_impact, sizeof vis->impact_sprite - 1);
            vis->impact_sprite[sizeof vis->impact_sprite - 1] = '\0';
            applied = 1;
        }
        if (*last_aoe)
        {
            strncpy(vis->aoe_sprite, last_aoe, sizeof vis->aoe_sprite - 1);
            vis->aoe_sprite[sizeof vis->aoe_sprite - 1] = '\0';
            applied = 1;
        }
        g_wizard.staged_count = 0; /* clear after apply */
        g_wizard.sel_index = -1;
        if (applied && vchanged)
            *vchanged = 1;
    }
    if (applied)
        overlay_label("(Import successful – visual params updated)");

    overlay_separator();
    if (overlay_button("Close Wizard"))
        g_wizard.show = 0;
    overlay_end_panel();
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
