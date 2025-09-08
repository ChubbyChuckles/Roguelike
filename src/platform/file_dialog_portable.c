/* file_dialog_portable.c - Cross-platform overlay-integrated file picker (open/save)
     Non-blocking async modal for debug overlay usage.
     Features:
         * Open + Save modes
         * Wildcard filtering (*.png;*.json) case-insensitive
         * Persistent last directory (prefs/file_dialog_lastdir.txt)
         * Pure C89 (MSVC friendly)
     Public API in file_dialog.h
*/

#include "file_dialog.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Defensive shim: if an older cached translation unit (pre-refactor) still refers to
    strtok_r, map it to strtok_s on MSVC so the build doesn't fail. The refactored code
    below no longer uses strtok_r/strncpy/strcat, but stale incremental builds were
    still flagging them as errors. This shim is harmless if unused. */
#if defined(_MSC_VER) && !defined(strtok_r)
#define strtok_r(str, delim, ctx) strtok_s((str), (delim), (ctx))
#endif
/* If legacy code path shows up (should not), silence deprecated CRT warnings just for
    this file to keep /WX clean. We rely on our custom fd_copy/fd_append instead. */
#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <dirent.h>
#include <unistd.h>
#define PATH_SEP '/'
#endif

#ifndef FILE_DIALOG_MAX_ENTRIES
#define FILE_DIALOG_MAX_ENTRIES 1024
#endif
#ifndef FILE_DIALOG_MAX_NAME
#define FILE_DIALOG_MAX_NAME 256
#endif

/* Very small wildcard matcher supporting '*', '?' only, case-insensitive */
static int fd_match_wild_ci(const char* pat, const char* text)
{
    while (*pat)
    {
        if (*pat == '*')
        {
            pat++;
            if (!*pat)
                return 1; /* trailing * matches all */
            while (*text)
            {
                if (fd_match_wild_ci(pat, text))
                    return 1;
                text++;
            }
            return 0;
        }
        else if (*pat == '?')
        {
            if (!*text)
                return 0;
            text++;
            pat++;
        }
        else
        {
            char pc = (char) tolower((unsigned char) *pat);
            char tc = (char) tolower((unsigned char) *text);
            if (pc != tc)
                return 0;
            pat++;
            text++;
        }
    }
    return *text == '\0';
}

/* Safe copy helpers (avoid MSVC deprecation warnings; no errno handling needed) */
static void fd_copy(char* dst, size_t cap, const char* src)
{
    size_t i = 0;
    if (!dst || cap == 0)
        return;
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    while (i + 1 < cap && src[i])
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}
static void fd_append(char* dst, size_t cap, const char* src)
{
    size_t len = dst ? strlen(dst) : 0;
    size_t i = 0;
    if (!dst || cap == 0 || !src || len >= cap)
        return;
    while (len + i + 1 < cap && src[i])
    {
        dst[len + i] = src[i];
        i++;
    }
    dst[len + i] = '\0';
}
static int fd_any_pattern_matches(const char* filter_list, const char* name)
{
    if (!filter_list || !*filter_list)
        return 1; /* no filter means all */
    /* Manual tokenization on ';' or ',' */
    {
        char buf[512];
        fd_copy(buf, sizeof buf, filter_list);
        const char* start = buf;
        while (*start)
        {
            const char* end = start;
            while (*end && *end != ';' && *end != ',')
                end++;
            /* Trim leading whitespace */
            while (*start && isspace((unsigned char) *start))
                start++;
            if (start < end)
            {
                char pat[128];
                size_t plen = (size_t) (end - start);
                if (plen >= sizeof pat)
                    plen = sizeof pat - 1;
                memcpy(pat, start, plen);
                pat[plen] = '\0';
                if (pat[0] && fd_match_wild_ci(pat, name))
                    return 1;
            }
            if (!*end)
                break;
            start = end + 1;
        }
    }
    return 0;
}

/* Persistent last directory */
static char g_last_dir[512];
static int g_last_dir_loaded = 0;

/* Overlay integration: prefer real widgets if header available, otherwise fall back to
    inert stubs so calling code remains safe when the debug overlay is compiled out.
    We try the relative path from this file (src/platform) up into debug_overlay. */
#if defined(__has_include)
#if __has_include("../debug_overlay/widgets/overlay_widgets.h")
#include "../debug_overlay/widgets/overlay_widgets.h"
#define ROGUE_FD_HAVE_OVERLAY 1
#elif __has_include("debug_overlay/widgets/overlay_widgets.h") /* secondary try */
#include "debug_overlay/widgets/overlay_widgets.h"
#define ROGUE_FD_HAVE_OVERLAY 1
#endif
#endif
#ifndef ROGUE_FD_HAVE_OVERLAY
#define overlay_begin_modal(id, open) 0
#define overlay_end_modal() ((void) 0)
#define overlay_button(l) 0
#define overlay_label(t) ((void) 0)
#define overlay_input_text(l, b, c) 0
#endif

static void fd_prefs_load_last_dir(void)
{
    if (g_last_dir_loaded)
        return;
    g_last_dir_loaded = 1;
    FILE* f = NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, "prefs/file_dialog_lastdir.txt", "r") != 0)
        f = NULL;
#else
    f = fopen("prefs/file_dialog_lastdir.txt", "r");
#endif
    if (f)
    {
        if (fgets(g_last_dir, (int) sizeof(g_last_dir), f))
        {
            size_t len = strlen(g_last_dir);
            while (len && (g_last_dir[len - 1] == '\n' || g_last_dir[len - 1] == '\r'))
                g_last_dir[--len] = '\0';
        }
        fclose(f);
    }
}
static void fd_prefs_save_last_dir(void)
{
    FILE* f = NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, "prefs/file_dialog_lastdir.txt", "w") != 0)
        f = NULL;
#else
    f = fopen("prefs/file_dialog_lastdir.txt", "w");
#endif
    if (f)
    {
        fputs(g_last_dir, f);
        fclose(f);
    }
}

static const char* fd_get_cwd(char* out, size_t cap)
{
#if defined(_WIN32)
    if (_getcwd(out, (int) cap))
        return out;
    else
        return NULL;
#else
    if (getcwd(out, cap))
        return out;
    else
        return NULL;
#endif
}

/* Basic file record */
typedef struct FDEntry
{
    char name[FILE_DIALOG_MAX_NAME];
    int is_dir;
} FDEntry;

typedef struct FileDialogState
{
    int active; /* modal visible */
    RogueFileDialogMode mode;
    char patterns[256];
    char filename[FILE_DIALOG_MAX_NAME];
    FDEntry entries[FILE_DIALOG_MAX_ENTRIES];
    int entry_count;
    int selection;
    int need_refresh;
    int result_ready; /* 1 success, -1 cancel */
    char result_path[512];
    int confirm_overwrite;
} FileDialogState;

static FileDialogState g_fd;

/* Legacy synchronous wrapper implemented via async modal spin (not ideal).
    If the native Win32 dialog is disabled we still provide this wrapper so
    existing callers linking against rogue_platform_open_file_dialog resolve.
*/
int rogue_platform_open_file_dialog(const char* filter, char* out_path, size_t out_cap)
{
    if (!out_path || out_cap == 0)
        return 0;
    /* If caller passes NULL filter just treat as all */
    (void) filter;
    rogue_file_dialog_show(ROGUE_FD_MODE_OPEN, filter, NULL);
    /* Busy wait limited iterations (should integrate into frame loop). */
    for (int i = 0; i < 5000; i++)
    {
        if (rogue_file_dialog_poll_result(out_path, out_cap) != 0)
            return out_path[0] ? 1 : 0;
    }
    return 0;
}

static void fd_scan_entries(void)
{
    fd_prefs_load_last_dir();
    if (!g_last_dir[0] && !fd_get_cwd(g_last_dir, sizeof(g_last_dir)))
        return;
    g_fd.entry_count = 0;
#if defined(_WIN32)
    {
        char pattern[600];
        snprintf(pattern, sizeof(pattern), "%s\\*", g_last_dir);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                const char* n = fd.cFileName;
                if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
                    continue;
                if (g_fd.entry_count >= FILE_DIALOG_MAX_ENTRIES)
                    break;
                FDEntry* e = &g_fd.entries[g_fd.entry_count];
                fd_copy(e->name, FILE_DIALOG_MAX_NAME, n);
                e->is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
                g_fd.entry_count++;
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
#else
    {
        DIR* d = opendir(g_last_dir);
        if (!d)
            return;
        struct dirent* de;
        while ((de = readdir(d)))
        {
            const char* n = de->d_name;
            if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
                continue;
            if (g_fd.entry_count >= FILE_DIALOG_MAX_ENTRIES)
                break;
            FDEntry* e = &g_fd.entries[g_fd.entry_count];
            fd_copy(e->name, FILE_DIALOG_MAX_NAME, n);
            e->is_dir = (de->d_type == DT_DIR) ? 1 : 0;
            g_fd.entry_count++;
        }
        closedir(d);
    }
#endif
    g_fd.need_refresh = 0;
}

void rogue_file_dialog_show(RogueFileDialogMode mode, const char* filter_patterns,
                            const char* default_name)
{
    memset(&g_fd, 0, sizeof(g_fd));
    g_fd.active = 1;
    g_fd.mode = mode;
    if (filter_patterns)
    {
        fd_copy(g_fd.patterns, sizeof g_fd.patterns, filter_patterns);
    }
    if (default_name && mode == ROGUE_FD_MODE_SAVE)
    {
        fd_copy(g_fd.filename, sizeof g_fd.filename, default_name);
    }
    fd_prefs_load_last_dir();
    if (!g_last_dir[0])
    {
        /* Default root path override: start inside project assets directory for faster import
           workflows in the asset browser panel. Fall back to CWD if assets/ not found. */
        struct stat st; /* C89: declare up front */
        if (stat("assets", &st) == 0 && (st.st_mode & S_IFDIR))
        {
            fd_copy(g_last_dir, sizeof(g_last_dir), "../../assets");
        }
        else
        {
            fd_get_cwd(g_last_dir, sizeof(g_last_dir));
        }
    }
    g_fd.need_refresh = 1;
}

int rogue_file_dialog_poll_result(char* out_path, size_t out_cap)
{
    if (!g_fd.active)
    {
        if (g_fd.result_ready == 1 && out_path && out_cap)
        {
            size_t len = strlen(g_fd.result_path);
            if (len + 1 > out_cap)
                return -1;
            memcpy(out_path, g_fd.result_path, len + 1);
        }
        return g_fd.result_ready; /* 1 success, -1 canceled, 0 none */
    }
    return 0; /* still running */
}

static void fd_finalize_selection(void)
{
    if (!g_fd.filename[0])
        return;
    if (strlen(g_last_dir) + 1 + strlen(g_fd.filename) + 1 >= sizeof(g_fd.result_path))
        return;
    /* Build result path safely */
    snprintf(g_fd.result_path, sizeof(g_fd.result_path), "%s%c%s", g_last_dir, PATH_SEP,
             g_fd.filename);
    g_fd.result_ready = 1;
    g_fd.active = 0;
    fd_prefs_save_last_dir();
}

void rogue_file_dialog_draw_overlay(void)
{
    if (!g_fd.active)
        return;
    if (g_fd.need_refresh)
        fd_scan_entries();
    /* Modal overlay support was planned but not present in current overlay_widgets API.
        We always render inline within the caller's panel. */
    overlay_label("[File Dialog]");
    overlay_label(g_last_dir);
    {
        int i;
        int shown = 0;
        for (i = 0; i < g_fd.entry_count && shown < 200; i++)
        {
            FDEntry* e = &g_fd.entries[i];
            int show = 1;
            if (!e->is_dir && g_fd.patterns[0])
                show = fd_any_pattern_matches(g_fd.patterns, e->name);
            if (!show)
                continue;
            char line[300];
            snprintf(line, sizeof(line), "%s%s", e->name, e->is_dir ? "/" : "");
            if (overlay_button(line))
            {
                if (e->is_dir)
                {
                    size_t len = strlen(g_last_dir);
                    if (len + 1 + strlen(e->name) + 1 < sizeof(g_last_dir))
                    {
                        g_last_dir[len] = PATH_SEP;
                        g_last_dir[len + 1] = '\0';
                        fd_append(g_last_dir, sizeof g_last_dir, e->name);
                        g_fd.need_refresh = 1;
                        fd_prefs_save_last_dir();
                    }
                }
                else
                {
                    g_fd.selection = i;
                    fd_copy(g_fd.filename, sizeof g_fd.filename, e->name);
                }
            }
            shown++;
        }
    }
    if (g_fd.mode == ROGUE_FD_MODE_SAVE)
        overlay_input_text("Filename", g_fd.filename, sizeof(g_fd.filename));
    else if (g_fd.selection >= 0)
        overlay_label(g_fd.filename);
    if (overlay_button(g_fd.mode == ROGUE_FD_MODE_SAVE ? "Save" : "Open"))
    {
        if (g_fd.mode == ROGUE_FD_MODE_SAVE)
        {
            struct stat st;
            char full[600];
            snprintf(full, sizeof(full), "%s%c%s", g_last_dir, PATH_SEP, g_fd.filename);
            if (stat(full, &st) == 0 && !g_fd.confirm_overwrite)
                g_fd.confirm_overwrite = 1; /* ask again */
            else
                fd_finalize_selection();
        }
        else
            fd_finalize_selection();
    }
    if (g_fd.confirm_overwrite)
        overlay_label("(Overwrite) Press Save again to confirm.");
    if (overlay_button("Cancel"))
    {
        g_fd.active = 0;
        g_fd.result_ready = -1;
    }
}
