/* asset_validate_cli.c - Phase 5: CI/CD integration tool
   Performs cross-platform asset validation tasks:
     - Recursive scan of assets/ directory
     - CRC32 computation via rogue_asset_crc32_file
     - Manifest generation (path crc32 size)
     - Optional baseline comparison (added/removed/changed)
     - Simple optimization advisor (duplicate files, large PNGs)
   Usage:
     asset_validate --manifest <out_file> [--baseline <file>] [--optimize]
   Exit codes:
     0 success (differences are reported but not treated as failure)
     1 unrecoverable error (assets dir missing or write failure)
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "asset_validation.h"

#ifdef _WIN32
#include <windows.h>
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#define PATH_SEP '\\'
#else
#include <dirent.h>
#include <errno.h>
#define PATH_SEP '/'
#endif

typedef struct AssetEntry
{
    char* path;     /* relative path under assets/ */
    uint32_t crc32; /* checksum */
    uint64_t size;  /* bytes */
} AssetEntry;

typedef struct AssetList
{
    AssetEntry* items;
    size_t count;
    size_t cap;
} AssetList;

static void list_init(AssetList* l)
{
    l->items = NULL;
    l->count = 0;
    l->cap = 0;
}
static void list_free(AssetList* l)
{
    for (size_t i = 0; i < l->count; i++)
        free(l->items[i].path);
    free(l->items);
}

static char* xdup(const char* s)
{
    size_t n = strlen(s) + 1;
    char* p = (char*) malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}

static void list_push(AssetList* l, const char* path, uint32_t crc, uint64_t size)
{
    if (l->count == l->cap)
    {
        size_t ncap = l->cap ? l->cap * 2 : 256;
        AssetEntry* n = (AssetEntry*) realloc(l->items, ncap * sizeof(AssetEntry));
        if (!n)
            return;
        l->items = n;
        l->cap = ncap;
    }
    l->items[l->count].path = xdup(path);
    l->items[l->count].crc32 = crc;
    l->items[l->count].size = size;
    l->count++;
}

static uint64_t file_size(const char* path)
{
    struct stat st;
    if (stat(path, &st) == 0)
        return (uint64_t) st.st_size;
    return 0ULL;
}

static void scan_dir(const char* root, const char* rel, AssetList* out)
{
    char full[1024];
    if (rel && rel[0])
        snprintf(full, sizeof(full), "%s%c%s", root, PATH_SEP, rel);
    else
        snprintf(full, sizeof(full), "%s", root);
#ifdef _WIN32
    char pattern[1060];
    snprintf(pattern, sizeof(pattern), "%s\\*", full);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const char* name = fd.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        char rel_child[1024];
        if (rel && rel[0])
            snprintf(rel_child, sizeof(rel_child), "%s/%s", rel, name);
        else
            snprintf(rel_child, sizeof(rel_child), "%s", name);
        char full_child[1024];
        snprintf(full_child, sizeof(full_child), "%s%c%s", root, PATH_SEP, rel_child);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            scan_dir(root, rel_child, out);
        }
        else
        {
            bool ok = false;
            uint32_t crc = rogue_asset_crc32_file(full_child, &ok);
            if (!ok)
                crc = 0;
            uint64_t sz = file_size(full_child);
            list_push(out, rel_child, crc, sz);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(full);
    if (!d)
        return;
    struct dirent* de;
    while ((de = readdir(d)))
    {
        const char* name = de->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        char rel_child[1024];
        if (rel && rel[0])
            snprintf(rel_child, sizeof(rel_child), "%s/%s", rel, name);
        else
            snprintf(rel_child, sizeof(rel_child), "%s", name);
        char full_child[1024];
        snprintf(full_child, sizeof(full_child), "%s/%s", full, name);
        struct stat st;
        if (stat(full_child, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            scan_dir(root, rel_child, out);
        else if (S_ISREG(st.st_mode))
        {
            bool ok = false;
            uint32_t crc = rogue_asset_crc32_file(full_child, &ok);
            if (!ok)
                crc = 0;
            list_push(out, rel_child, crc, (uint64_t) st.st_size);
        }
    }
    closedir(d);
#endif
}

typedef struct BaselineEntry
{
    char* path;
    uint32_t crc32;
} BaselineEntry;
typedef struct BaselineList
{
    BaselineEntry* items;
    size_t count;
    size_t cap;
} BaselineList;
static void baseline_add(BaselineList* b, const char* p, uint32_t crc)
{
    if (b->count == b->cap)
    {
        size_t nc = b->cap ? b->cap * 2 : 256;
        BaselineEntry* n = (BaselineEntry*) realloc(b->items, nc * sizeof(BaselineEntry));
        if (!n)
            return;
        b->items = n;
        b->cap = nc;
    }
    b->items[b->count].path = xdup(p);
    b->items[b->count].crc32 = crc;
    b->count++;
}
static void baseline_free(BaselineList* b)
{
    for (size_t i = 0; i < b->count; i++)
        free(b->items[i].path);
    free(b->items);
}
static uint32_t parse_hex32(const char* s)
{
    uint32_t v = 0;
    sscanf(s, "%x", &v);
    return v;
}
static void load_baseline(const char* file, BaselineList* b)
{
    FILE* f = NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, file, "r") != 0)
        f = NULL;
#else
    f = fopen(file, "r");
#endif
    if (!f)
        return;
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        char path[900];
        char hex[64];
        if (sscanf(line, "%899s %63s", path, hex) == 2)
        {
            baseline_add(b, path, parse_hex32(hex));
        }
    }
    fclose(f);
}
static const BaselineEntry* baseline_find(const BaselineList* b, const char* path)
{
    for (size_t i = 0; i < b->count; i++)
        if (strcmp(b->items[i].path, path) == 0)
            return &b->items[i];
    return NULL;
}

static void write_manifest(const char* out_file, const AssetList* assets)
{
    if (!out_file)
        return;
    FILE* f = NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, out_file, "w") != 0)
        f = NULL;
#else
    f = fopen(out_file, "w");
#endif
    if (!f)
    {
        fprintf(stderr, "Failed to write manifest %s\n", out_file);
        return;
    }
    for (size_t i = 0; i < assets->count; i++)
        fprintf(f, "%s %08X %llu\n", assets->items[i].path, assets->items[i].crc32,
                (unsigned long long) assets->items[i].size);
    fclose(f);
}

static void report_differences(const AssetList* cur, const BaselineList* base)
{
    size_t added = 0, removed = 0, changed = 0;
    bool* seen = (bool*) calloc(base->count, sizeof(bool));
    for (size_t i = 0; i < cur->count; i++)
    {
        const BaselineEntry* be = baseline_find(base, cur->items[i].path);
        if (!be)
            added++;
        else
        {
            size_t idx = (size_t) (be - base->items);
            seen[idx] = true;
            if (be->crc32 != cur->items[i].crc32)
                changed++;
        }
    }
    for (size_t i = 0; i < base->count; i++)
        if (!seen[i])
            removed++;
    printf("Asset diff summary: added=%zu removed=%zu changed=%zu\n", added, removed, changed);
    if (added || removed || changed)
    {
        printf("Detailed changes:\n");
        for (size_t i = 0; i < cur->count; i++)
        {
            const BaselineEntry* be = baseline_find(base, cur->items[i].path);
            if (!be)
                printf("  + %s\n", cur->items[i].path);
            else if (be->crc32 != cur->items[i].crc32)
                printf("  ~ %s (%08X -> %08X)\n", cur->items[i].path, be->crc32,
                       cur->items[i].crc32);
        }
        for (size_t i = 0; i < base->count; i++)
        {
            if (!seen[i])
                printf("  - %s\n", base->items[i].path);
        }
    }
    free(seen);
}

static void optimization_advisor(const AssetList* cur)
{
    printf("Optimization advisor (heuristic):\n");
    for (size_t i = 0; i < cur->count; i++)
    {
        for (size_t j = i + 1; j < cur->count; j++)
        {
            if (cur->items[i].crc32 && cur->items[i].crc32 == cur->items[j].crc32 &&
                cur->items[i].size == cur->items[j].size)
            {
                printf("  Duplicate candidate: %s == %s (crc=%08X size=%llu)\n", cur->items[i].path,
                       cur->items[j].path, cur->items[i].crc32,
                       (unsigned long long) cur->items[i].size);
            }
        }
        const char* p = cur->items[i].path;
        size_t len = strlen(p);
        if (cur->items[i].size > 1024 * 1024 && len > 4 && (strcasecmp(p + len - 4, ".png") == 0))
        {
            printf("  Large PNG (>1MB) consider compression/atlas: %s (%.2f MB)\n", p,
                   (double) cur->items[i].size / 1024.0 / 1024.0);
        }
    }
}

int main(int argc, char** argv)
{
    const char* manifest = NULL;
    const char* baseline = NULL;
    bool do_opt = false;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc)
            manifest = argv[++i];
        else if (strcmp(argv[i], "--baseline") == 0 && i + 1 < argc)
            baseline = argv[++i];
        else if (strcmp(argv[i], "--optimize") == 0)
            do_opt = true;
        else if (strcmp(argv[i], "--help") == 0)
        {
            printf("Usage: %s --manifest <file> [--baseline <file>] [--optimize]\n", argv[0]);
            return 0;
        }
    }
    /* directory existence check */
#ifdef _WIN32
    {
        char cwd_dbg[512];
        DWORD cwd_len = GetCurrentDirectoryA(sizeof(cwd_dbg), cwd_dbg);
        if (cwd_len > 0 && cwd_len < sizeof(cwd_dbg))
            printf("[asset_validate] CWD=%s\n", cwd_dbg);
    }
#else
    {
        char cwd_dbg[512];
        if (getcwd(cwd_dbg, sizeof(cwd_dbg)))
            printf("[asset_validate] CWD=%s\n", cwd_dbg);
    }
#endif
#ifdef _WIN32
    struct _stat ds;
    if (_stat("assets", &ds) != 0 || !(ds.st_mode & _S_IFDIR))
    {
        fprintf(stderr, "assets directory not found (run from project root)\n");
        return 1;
    }
#else
    struct stat ds;
    if (stat("assets", &ds) != 0 || !S_ISDIR(ds.st_mode))
    {
        fprintf(stderr, "assets directory not found (run from project root)\n");
        return 1;
    }
#endif
    AssetList cur;
    list_init(&cur);
    printf("[asset_validate] Scanning assets...\n");
    scan_dir("assets", "", &cur);
    printf("[asset_validate] Scanned %zu asset files.\n", cur.count);
    if (cur.count == 0)
    {
        fprintf(stderr, "No assets found; failing.\n");
        list_free(&cur);
        return 1;
    }
    if (manifest)
    {
        write_manifest(manifest, &cur);
        printf("Wrote manifest: %s\n", manifest);
    }
    if (baseline)
    {
        BaselineList base = {0};
        load_baseline(baseline, &base);
        if (base.count)
            report_differences(&cur, &base);
        else
            printf("Baseline not loaded or empty: %s\n", baseline);
        baseline_free(&base);
    }
    if (do_opt)
        optimization_advisor(&cur);
    list_free(&cur);
    rogue_asset_validation_shutdown();
    return 0;
}
