#include "visuals_internal.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

/* Sprite Browser (Windows only recursive scan) */
void rogue_visuals_draw_sprite_browser(RogueSkillVisualParams* vis, int* vchanged)
{
    enum
    {
        TARGET_CAST = 0,
        TARGET_PROJECTILE,
        TARGET_IMPACT,
        TARGET_AOE
    };
    static int browser_open = 0;
    static int browser_target = TARGET_CAST;
    static char filter[64] = {0};

    const char* assign_items[] = {"Cast Sheet", "Projectile", "Impact", "AoE"};
    (void) overlay_combo("Assign To", &browser_target, assign_items, 4);
    if (overlay_button("Browse Sprites (assets/*)"))
        browser_open = 1;
    if (!browser_open)
        return;

    overlay_label("Sprite Browser");
    overlay_input_text("Filter (substring)", filter, sizeof filter);

    static char paths[2048][260];
    int count = 0;
#if defined(_WIN32)
    typedef struct
    {
        char path[260];
        int depth;
    } Node;
    Node stack[256];
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
            if (is_dir && cur.depth < 3)
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
                const char* ext = strrchr(child, '.');
                if (ext)
                {
                    char el[8] = {0};
                    for (int i = 0; ext[i] && i < 7; ++i)
                        el[i] = (char) tolower((unsigned char) ext[i]);
                    if (strcmp(el, ".png") == 0 || strcmp(el, ".bmp") == 0 ||
                        strcmp(el, ".jpg") == 0 || strcmp(el, ".jpeg") == 0)
                    {
                        if (count < (int) (sizeof paths / sizeof paths[0]))
                        {
                            char norm[260];
                            snprintf(norm, sizeof norm, "%s", child);
                            for (char* p = norm; *p; ++p)
                                if (*p == '\\')
                                    *p = '/';
                            strncpy(paths[count], norm, sizeof paths[count] - 1);
                            paths[count][sizeof paths[count] - 1] = '\0';
                            count++;
                        }
                    }
                }
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#endif

    const char* hdr[] = {"Path (assets/...)"};
    int sc = 0, sd = 0;
    if (overlay_table_begin("sprite_browser", hdr, 1, &sc, &sd, filter))
    {
        int selected = -1;
        for (int i = 0; i < count; ++i)
        {
            const char* row[1] = {paths[i]};
            if (overlay_table_row(row, 1, i, &selected))
            {
                const char* chosen = paths[i];
                if (browser_target == TARGET_CAST)
                {
                    strncpy(vis->cast_sprite_sheet, chosen, sizeof vis->cast_sprite_sheet - 1);
                    vis->cast_sprite_sheet[sizeof vis->cast_sprite_sheet - 1] = '\0';
                }
                else if (browser_target == TARGET_PROJECTILE)
                {
                    strncpy(vis->projectile_sprite, chosen, sizeof vis->projectile_sprite - 1);
                    vis->projectile_sprite[sizeof vis->projectile_sprite - 1] = '\0';
                }
                else if (browser_target == TARGET_IMPACT)
                {
                    strncpy(vis->impact_sprite, chosen, sizeof vis->impact_sprite - 1);
                    vis->impact_sprite[sizeof vis->impact_sprite - 1] = '\0';
                }
                else if (browser_target == TARGET_AOE)
                {
                    strncpy(vis->aoe_sprite, chosen, sizeof vis->aoe_sprite - 1);
                    vis->aoe_sprite[sizeof vis->aoe_sprite - 1] = '\0';
                }
                *vchanged = 1;
                browser_open = 0;
                break;
            }
        }
        overlay_table_end();
    }
    if (overlay_button("Close Browser"))
        browser_open = 0;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
