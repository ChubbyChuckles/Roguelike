#include "asset_browser_dir.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef ROGUE_FILE_DIALOG_PATH_MAX
#define ROGUE_FILE_DIALOG_PATH_MAX 260
#endif

#define AB_DIR_RESERVE_STEP 128

/* Portable case-insensitive strcmp wrapper */
#if defined(_WIN32)
#define ab_strcasecmp _stricmp
#else
#define ab_strcasecmp strcasecmp
#endif

static int ab_dir_is_sep(char c) { return c == '/' || c == '\\'; }

static void ab_copy_safe(char* dst, size_t cap, const char* src)
{
    size_t i = 0;
    if (!dst || !cap)
    {
        return;
    }
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    while (src[i] && i + 1 < cap)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int ab_path_is_dir(const char* p)
{
#ifdef _WIN32
    DWORD a = GetFileAttributesA(p);
    if (a == INVALID_FILE_ATTRIBUTES)
        return 0;
    return (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (stat(p, &st) != 0)
        return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
#endif
}

static void ab_locate_assets_root(AssetBrowserEnhancedState* s)
{
    if (s->dir_root[0])
        return; /* already */
    /* 1 env */
    {
        const char* env = getenv("ROGUE_ASSETS_DIR");
        if (env && env[0] && ab_path_is_dir(env))
        {
            ab_copy_safe(s->dir_root, sizeof s->dir_root, env);
            return;
        }
    }
    /* 2 rel */
    {
        const char* rels[] = {"assets",       "./assets",        "../assets",
                              "../../assets", "../../../assets", 0};
        int i = 0;
        while (rels[i])
        {
            if (ab_path_is_dir(rels[i]))
            {
                ab_copy_safe(s->dir_root, sizeof s->dir_root, rels[i]);
                return;
            }
            i++;
        }
    }
    /* 3 ascend */
    {
        char cwd[ROGUE_FILE_DIALOG_PATH_MAX];
#ifdef _WIN32
        if (!_getcwd(cwd, (int) sizeof cwd))
            cwd[0] = '\0';
#else
        if (!getcwd(cwd, sizeof cwd))
            cwd[0] = '\0';
#endif
        if (cwd[0])
        {
            char probe[ROGUE_FILE_DIALOG_PATH_MAX * 2];
            for (int depth = 0; depth < 8 && cwd[0]; ++depth)
            {
                snprintf(probe, sizeof probe, "%s/%s", cwd, "assets");
                if (ab_path_is_dir(probe))
                {
                    ab_copy_safe(s->dir_root, sizeof s->dir_root, probe);
                    return;
                }
                /* trim */
                size_t len = strlen(cwd);
                while (len && (cwd[len - 1] == '/' || cwd[len - 1] == '\\'))
                    cwd[--len] = '\0';
                while (len && cwd[len - 1] != '/' && cwd[len - 1] != '\\')
                    cwd[--len] = '\0';
                while (len && (cwd[len - 1] == '/' || cwd[len - 1] == '\\'))
                    cwd[--len] = '\0';
            }
        }
    }
    ab_copy_safe(s->dir_root, sizeof s->dir_root, "assets");
}

void rogue_asset_browser_dir_init_if_needed(void)
{
    AssetBrowserEnhancedState* s = rogue_asset_browser_state();
    if (!s->dir_root[0])
        ab_locate_assets_root(s);
    if (!s->dir_cwd[0])
    {
        ab_copy_safe(s->dir_cwd, sizeof s->dir_cwd, s->dir_root);
        s->dir_scroll = 0;
        s->dir_selected = -1;
        s->dir_entries = NULL;
        s->dir_count = 0;
        s->dir_capacity = 0;
    }
}

void rogue_asset_browser_dir_parent(char* path)
{
    AssetBrowserEnhancedState* s = rogue_asset_browser_state();
    if (!path || !path[0])
        return;
    if (s->dir_root[0] && strcmp(path, s->dir_root) == 0)
        return; /* root */
    size_t root_len = s->dir_root[0] ? strlen(s->dir_root) : 0;
    size_t len = strlen(path);
    while (len && ab_dir_is_sep(path[len - 1]))
        path[--len] = '\0';
    while (len && !ab_dir_is_sep(path[len - 1]))
        path[--len] = '\0';
    while (len && ab_dir_is_sep(path[len - 1]))
        path[--len] = '\0';
    if (root_len && (len < root_len || strncmp(path, s->dir_root, root_len) != 0))
    {
        ab_copy_safe(path, ROGUE_FILE_DIALOG_PATH_MAX, s->dir_root);
    }
}

void rogue_asset_browser_dir_join(char* out, size_t cap, const char* a, const char* b)
{
    if (!out || !cap)
    {
        return;
    }
    if (!a || !b)
    {
        out[0] = '\0';
        return;
    }
    snprintf(out, cap, "%s%s%s", a, (a[0] && !ab_dir_is_sep(a[strlen(a) - 1])) ? "/" : "", b);
}

void rogue_asset_browser_dir_refresh(void)
{
    AssetBrowserEnhancedState* s = rogue_asset_browser_state();
    s->dir_count = 0;
    rogue_asset_browser_dir_init_if_needed();
    if (s->dir_capacity == 0)
    {
        s->dir_capacity = AB_DIR_RESERVE_STEP;
        s->dir_entries =
            (RogueAssetBrowserDirEntry*) malloc(sizeof(*s->dir_entries) * s->dir_capacity);
        if (!s->dir_entries)
        {
            s->dir_capacity = 0;
            return;
        }
    }
#ifdef _WIN32
    {
        char pattern[ROGUE_FILE_DIALOG_PATH_MAX * 2];
        snprintf(pattern, sizeof pattern, "%s/*", s->dir_cwd);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                const char* n = fd.cFileName;
                if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
                    continue;
                if (s->dir_count >= s->dir_capacity)
                {
                    int new_cap = s->dir_capacity ? s->dir_capacity * 2 : 128;
                    void* nm = realloc(s->dir_entries, sizeof(*s->dir_entries) * new_cap);
                    if (!nm)
                        break;
                    s->dir_entries = (RogueAssetBrowserDirEntry*) nm;
                    s->dir_capacity = new_cap;
                }
                ab_copy_safe(s->dir_entries[s->dir_count].name,
                             sizeof s->dir_entries[s->dir_count].name, n);
                s->dir_entries[s->dir_count].is_dir =
                    (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
                s->dir_count++;
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
#else
    {
        DIR* d = opendir(s->dir_cwd);
        if (d)
        {
            struct dirent* ent;
            while ((ent = readdir(d)))
            {
                const char* n = ent->d_name;
                if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
                    continue;
                if (s->dir_count >= s->dir_capacity)
                {
                    int new_cap = s->dir_capacity ? s->dir_capacity * 2 : 128;
                    void* nm = realloc(s->dir_entries, sizeof(*s->dir_entries) * new_cap);
                    if (!nm)
                        break;
                    s->dir_entries = (RogueAssetBrowserDirEntry*) nm;
                    s->dir_capacity = new_cap;
                }
                ab_copy_safe(s->dir_entries[s->dir_count].name,
                             sizeof s->dir_entries[s->dir_count].name, n);
                s->dir_entries[s->dir_count].is_dir = (ent->d_type == DT_DIR);
                s->dir_count++;
            }
            closedir(d);
        }
    }
#endif
    if (s->dir_count == 0)
    {
        int had_root = s->dir_root[0] ? 1 : 0;
        ab_locate_assets_root(s);
        if (!had_root ||
            (s->dir_root[0] && strncmp(s->dir_cwd, s->dir_root, strlen(s->dir_root)) != 0))
        {
#ifdef _WIN32
            char pattern2[ROGUE_FILE_DIALOG_PATH_MAX * 2];
            ab_copy_safe(s->dir_cwd, sizeof s->dir_cwd, s->dir_root);
            snprintf(pattern2, sizeof pattern2, "%s/*", s->dir_cwd);
            WIN32_FIND_DATAA fd2;
            HANDLE h2 = FindFirstFileA(pattern2, &fd2);
            if (h2 != INVALID_HANDLE_VALUE)
            {
                do
                {
                    const char* n = fd2.cFileName;
                    if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
                        continue;
                    if (s->dir_count >= s->dir_capacity)
                    {
                        int new_cap2 = s->dir_capacity ? s->dir_capacity * 2 : 128;
                        void* nm2 = realloc(s->dir_entries, sizeof(*s->dir_entries) * new_cap2);
                        if (!nm2)
                            break;
                        s->dir_entries = (RogueAssetBrowserDirEntry*) nm2;
                        s->dir_capacity = new_cap2;
                    }
                    ab_copy_safe(s->dir_entries[s->dir_count].name,
                                 sizeof s->dir_entries[s->dir_count].name, n);
                    s->dir_entries[s->dir_count].is_dir =
                        (fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
                    s->dir_count++;
                } while (FindNextFileA(h2, &fd2));
                FindClose(h2);
            }
#else
            DIR* d2 = opendir(s->dir_root);
            if (d2)
            {
                struct dirent* ent2;
                ab_copy_safe(s->dir_cwd, sizeof s->dir_cwd, s->dir_root);
                while ((ent2 = readdir(d2)))
                {
                    const char* n = ent2->d_name;
                    if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
                        continue;
                    if (s->dir_count >= s->dir_capacity)
                    {
                        int new_cap2 = s->dir_capacity ? s->dir_capacity * 2 : 128;
                        void* nm2 = realloc(s->dir_entries, sizeof(*s->dir_entries) * new_cap2);
                        if (!nm2)
                            break;
                        s->dir_entries = (RogueAssetBrowserDirEntry*) nm2;
                        s->dir_capacity = new_cap2;
                    }
                    ab_copy_safe(s->dir_entries[s->dir_count].name,
                                 sizeof s->dir_entries[s->dir_count].name, n);
                    s->dir_entries[s->dir_count].is_dir = (ent2->d_type == DT_DIR);
                    s->dir_count++;
                }
                closedir(d2);
            }
#endif
        }
    }
    /* sort: dirs first then case insensitive name */
    for (int i = 0; i < s->dir_count; i++)
    {
        for (int j = i + 1; j < s->dir_count; j++)
        {
            int swap = 0;
            int ai = s->dir_entries[i].is_dir;
            int aj = s->dir_entries[j].is_dir;
            if (aj && !ai)
                swap = 1;
            else if (aj == ai && ab_strcasecmp(s->dir_entries[i].name, s->dir_entries[j].name) > 0)
                swap = 1;
            if (swap)
            {
                RogueAssetBrowserDirEntry tmp = s->dir_entries[i];
                s->dir_entries[i] = s->dir_entries[j];
                s->dir_entries[j] = tmp;
            }
        }
    }
}
