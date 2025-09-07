#include "visuals_internal.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

void rogue_visuals_draw_asset_picker(RogueSkillVisualParams* vis, int* vchanged)
{
    static int picker_open = 0;
    static int picker_category = 0;
    static char filter[64] = {0};
    if (overlay_button("Asset File Picker"))
        picker_open = 1;
    if (!picker_open)
        return;

    overlay_label("Asset File Picker");
    const char* cat_items[] = {"Images", "Audio", "All"};
    overlay_combo("Category", &picker_category, cat_items, 3);
    overlay_input_text("Filter (substring)", filter, (int) sizeof filter);

    static char apaths[4096][260];
    int acount = 0;
#if defined(_WIN32)
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
            if (is_dir && cur.depth < 4)
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
                int keep = 0;
                if (ext)
                {
                    char lo[8] = {0};
                    for (int i = 0; ext[i] && i < 7; ++i)
                        lo[i] = (char) tolower((unsigned char) ext[i]);
                    int is_img = (strcmp(lo, ".png") == 0 || strcmp(lo, ".bmp") == 0 ||
                                  strcmp(lo, ".jpg") == 0 || strcmp(lo, ".jpeg") == 0);
                    int is_audio = (strcmp(lo, ".wav") == 0 || strcmp(lo, ".ogg") == 0 ||
                                    strcmp(lo, ".mp3") == 0);
                    if (picker_category == 0)
                        keep = is_img;
                    else if (picker_category == 1)
                        keep = is_audio;
                    else
                        keep = (is_img || is_audio);
                }
                if (keep && acount < (int) (sizeof apaths / sizeof apaths[0]))
                {
                    char norm[260];
                    snprintf(norm, sizeof norm, "%s", child);
                    for (char* p = norm; *p; ++p)
                        if (*p == '\\')
                            *p = '/';
                    strncpy(apaths[acount], norm, sizeof apaths[acount] - 1);
                    apaths[acount][sizeof apaths[acount] - 1] = '\0';
                    acount++;
                }
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#endif
    const char* hdr[] = {"Path"};
    int sc = 0, sd = 0;
    if (overlay_table_begin("asset_file_picker", hdr, 1, &sc, &sd, filter))
    {
        int selrow = -1;
        for (int i = 0; i < acount; ++i)
        {
            const char* row[1] = {apaths[i]};
            if (overlay_table_row(row, 1, i, &selrow))
            {
                const char* chosen = apaths[i];
                const char* ext = strrchr(chosen, '.');
                int is_img = 0;
                if (ext)
                {
                    char lo[8] = {0};
                    for (int k = 0; ext[k] && k < 7; ++k)
                        lo[k] = (char) tolower((unsigned char) ext[k]);
                    is_img = (strcmp(lo, ".png") == 0 || strcmp(lo, ".bmp") == 0 ||
                              strcmp(lo, ".jpg") == 0 || strcmp(lo, ".jpeg") == 0);
                }
                if (is_img)
                { /* assign using current browser target semantics (cast) */
                    strncpy(vis->cast_sprite_sheet, chosen, sizeof vis->cast_sprite_sheet - 1);
                    vis->cast_sprite_sheet[sizeof vis->cast_sprite_sheet - 1] = '\0';
                    *vchanged = 1;
                    picker_open = 0;
                }
                else
                    overlay_label("(Selected file not an image – no assignment)");
                break;
            }
        }
        overlay_table_end();
    }
    if (overlay_button("Close Asset Picker"))
        picker_open = 0;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
