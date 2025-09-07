/* asset_tool_cli.c - Phase 7: Developer Tools & Utilities
   Multi-command asset management helper consolidating common inspection
   and debugging workflows that previously required separate utilities.

   Commands (invoke with one at a time):
     asset_tool stats
       Prints RogueAssetUsageStats (peaks, reload counters) after initializing
       the asset manager headless. Does not scan or load files automatically.

     asset_tool list --dir <root> [--ext .png,.jpg,.bmp,.ogg]
       Recursively enumerates files under <root> (default assets/) filtered by
       a comma separated extension list (case-insensitive) and prints them.

     asset_tool checksum-verify --manifest <file>
       Reads a manifest produced by asset_validate ("path CRC SIZE") and
       recomputes CRC32 of each path on disk. Reports mismatches and summary.

     asset_tool checksum-snapshot --manifest <out>
       Generates a new manifest (same simple format) for current assets/ tree.

     asset_tool diff --baseline <file>
       Produces a diff (added/removed/changed) against a prior manifest file.

     asset_tool inspect --id <substring>
       Initializes asset manager, acquires any textures whose relative path
       contains <substring> (case-insensitive) then prints per-entry info +
       current CRC32 (if file exists) and dependency list (if registered).

   Notes:
     - Headless safe: all SDL calls are guarded by existing asset manager code.
     - This intentionally duplicates a minimal subset of scanning logic from
       asset_validate_cli to keep coupling low; further refactors may extract
       shared helpers if the surface grows.
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "asset_manager.h"
#include "asset_validation.h"

#ifdef _WIN32
#include <windows.h>
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
/* Provide strcasestr polyfill for MSVC */
static const char* strcasestr_local(const char* haystack, const char* needle)
{
    if (!haystack || !needle || !*needle)
        return haystack;
    size_t nlen = strlen(needle);
    for (const char* p = haystack; *p; ++p)
    {
        if (_strnicmp(p, needle, nlen) == 0)
            return p;
    }
    return NULL;
}
#define strcasestr strcasestr_local
#define PATH_SEP '\\'
#else
#include <dirent.h>
#include <errno.h>
#define PATH_SEP '/'
#endif

typedef struct ScanFile
{
    char* rel;     /* relative path under root */
    uint64_t size; /* bytes */
    uint32_t crc;  /* crc32 (0 if failed) */
} ScanFile;
typedef struct ScanList
{
    ScanFile* v;
    size_t count;
    size_t cap;
} ScanList;

static void scanlist_push(ScanList* l, const char* rel, uint64_t size, uint32_t crc)
{
    if (l->count == l->cap)
    {
        size_t nc = l->cap ? l->cap * 2 : 256;
        ScanFile* nv = (ScanFile*) realloc(l->v, nc * sizeof(ScanFile));
        if (!nv)
            return;
        l->v = nv;
        l->cap = nc;
    }
    size_t n = strlen(rel) + 1;
    l->v[l->count].rel = (char*) malloc(n);
    if (l->v[l->count].rel)
        memcpy(l->v[l->count].rel, rel, n);
    l->v[l->count].size = size;
    l->v[l->count].crc = crc;
    l->count++;
}
static void scanlist_free(ScanList* l)
{
    for (size_t i = 0; i < l->count; ++i)
        free(l->v[i].rel);
    free(l->v);
}

static uint64_t file_size_simple(const char* path)
{
    struct stat st;
    if (stat(path, &st) == 0)
        return (uint64_t) st.st_size;
    return 0ULL;
}

static int has_ext(const char* name, const char* list_csv)
{
    if (!list_csv || !list_csv[0])
        return 1; /* no filter */
    const char* dot = strrchr(name, '.');
    if (!dot)
        return 0;
    char lower[32];
    size_t len = strlen(dot);
    if (len >= sizeof lower)
        return 0;
    for (size_t i = 0; i < len; ++i)
        lower[i] = (char) ((dot[i] >= 'A' && dot[i] <= 'Z') ? dot[i] - 'A' + 'a' : dot[i]);
    lower[len] = '\0';
    /* iterate tokens */
    char buf[256];
    strncpy(buf, list_csv, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    for (char* tok = strtok(buf, ","); tok; tok = strtok(NULL, ","))
    {
        while (*tok == ' ')
            tok++;
        size_t tlen = strlen(tok);
        if (tlen == len && strncasecmp(tok, lower, len) == 0)
            return 1;
    }
    return 0;
}

static void scan_dir(const char* root, const char* rel, ScanList* out, const char* ext_filter)
{
    char full[1024];
    if (rel && rel[0])
        snprintf(full, sizeof full, "%s%c%s", root, PATH_SEP, rel);
    else
        snprintf(full, sizeof full, "%s", root);
#ifdef _WIN32
    char pattern[1060];
    snprintf(pattern, sizeof pattern, "%s\\*", full);
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
            snprintf(rel_child, sizeof rel_child, "%s/%s", rel, name);
        else
            snprintf(rel_child, sizeof rel_child, "%s", name);
        char full_child[1024];
        snprintf(full_child, sizeof full_child, "%s%c%s", root, PATH_SEP, rel_child);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            scan_dir(root, rel_child, out, ext_filter);
        }
        else
        {
            if (!has_ext(name, ext_filter))
                continue;
            bool ok = false;
            uint32_t crc = rogue_asset_crc32_file(full_child, &ok);
            if (!ok)
                crc = 0;
            uint64_t sz = file_size_simple(full_child);
            scanlist_push(out, rel_child, sz, crc);
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
            snprintf(rel_child, sizeof rel_child, "%s/%s", rel, name);
        else
            snprintf(rel_child, sizeof rel_child, "%s", name);
        char full_child[1024];
        snprintf(full_child, sizeof full_child, "%s/%s", full, name);
        struct stat st;
        if (stat(full_child, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            scan_dir(root, rel_child, out, ext_filter);
        else if (S_ISREG(st.st_mode))
        {
            if (!has_ext(name, ext_filter))
                continue;
            bool ok = false;
            uint32_t crc = rogue_asset_crc32_file(full_child, &ok);
            if (!ok)
                crc = 0;
            scanlist_push(out, rel_child, (uint64_t) st.st_size, crc);
        }
    }
    closedir(d);
#endif
}

static void command_list(const char* dir, const char* ext_filter)
{
    ScanList l = {0};
    scan_dir(dir, "", &l, ext_filter);
    for (size_t i = 0; i < l.count; ++i)
        printf("%s %08X %llu\n", l.v[i].rel, l.v[i].crc, (unsigned long long) l.v[i].size);
    printf("Total files: %zu\n", l.count);
    scanlist_free(&l);
}

static void command_stats(void)
{
    if (!rogue_asset_manager_init(NULL))
    {
        fprintf(stderr, "asset_manager init failed\n");
        return;
    }
    RogueAssetUsageStats s = rogue_asset_usage_stats();
    printf("TextureRecords=%u Peak=%u WithHandle=%u Failed=%u\n", s.texture_records,
           s.peak_texture_records, s.textures_with_handle, s.textures_failed);
    printf("AudioRecords=%u Peak=%u WithHandle=%u Failed=%u\n", s.audio_records,
           s.peak_audio_records, s.audio_with_handle, s.audio_failed);
    printf("Reloads=%u LastReloadMS=%u\n", s.reloads_detected, s.last_reload_ms);
    rogue_asset_manager_shutdown();
    rogue_asset_validation_shutdown();
}

static void load_manifest(const char* file, ScanList* out)
{
    FILE* f = NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, file, "r") != 0)
        f = NULL;
#else
    f = fopen(file, "r");
#endif
    if (!f)
    {
        fprintf(stderr, "Failed to open manifest %s\n", file);
        return;
    }
    char path[900];
    char hex[32];
    unsigned long long sz = 0ULL;
    while (fgets(path, sizeof path, f))
    {
        char line[1024];
        strcpy(line, path);
        if (sscanf(line, "%899s %31s %llu", path, hex, &sz) >= 2)
        {
            uint32_t crc = 0;
            sscanf(hex, "%x", &crc);
            scanlist_push(out, path, sz, crc);
        }
    }
    fclose(f);
}

static const ScanFile* find_scan(const ScanList* l, const char* rel)
{
    for (size_t i = 0; i < l->count; i++)
        if (strcmp(l->v[i].rel, rel) == 0)
            return &l->v[i];
    return NULL;
}

static void command_diff(const char* baseline_file)
{
    ScanList base = {0};
    load_manifest(baseline_file, &base);
    if (!base.count)
    {
        fprintf(stderr, "Baseline empty or missing.\n");
        scanlist_free(&base);
        return;
    }
    ScanList cur = {0};
    scan_dir("assets", "", &cur, "");
    size_t added = 0, removed = 0, changed = 0;
    bool* seen = (bool*) calloc(base.count, sizeof(bool));
    for (size_t i = 0; i < cur.count; i++)
    {
        const ScanFile* b = find_scan(&base, cur.v[i].rel);
        if (!b)
        {
            added++;
            continue;
        }
        size_t idx = (size_t) (b - base.v);
        seen[idx] = true;
        if (b->crc != cur.v[i].crc)
            changed++;
    }
    for (size_t i = 0; i < base.count; i++)
        if (!seen[i])
            removed++;
    printf("Diff: added=%zu removed=%zu changed=%zu\n", added, removed, changed);
    if (added || removed || changed)
    {
        printf("Details:\n");
        for (size_t i = 0; i < cur.count; i++)
        {
            const ScanFile* b = find_scan(&base, cur.v[i].rel);
            if (!b)
                printf("  + %s\n", cur.v[i].rel);
            else if (b->crc != cur.v[i].crc)
                printf("  ~ %s (%08X -> %08X)\n", cur.v[i].rel, b->crc, cur.v[i].crc);
        }
        for (size_t i = 0; i < base.count; i++)
            if (!seen[i])
                printf("  - %s\n", base.v[i].rel);
    }
    free(seen);
    scanlist_free(&base);
    scanlist_free(&cur);
}

static void command_checksum_verify(const char* manifest)
{
    ScanList man = {0};
    load_manifest(manifest, &man);
    if (!man.count)
    {
        fprintf(stderr, "Manifest empty or load failed.\n");
        scanlist_free(&man);
        return;
    }
    size_t mismatches = 0;
    size_t missing = 0;
    for (size_t i = 0; i < man.count; i++)
    {
        bool ok = false;
        uint32_t crc = rogue_asset_crc32_file(man.v[i].rel, &ok);
        if (!ok)
        {
            missing++;
            continue;
        }
        if (crc != man.v[i].crc)
        {
            mismatches++;
            printf("Mismatch %s (%08X -> %08X)\n", man.v[i].rel, man.v[i].crc, crc);
        }
    }
    printf("Checksum verify: files=%zu mismatches=%zu missing=%zu\n", man.count, mismatches,
           missing);
    scanlist_free(&man);
}

static void command_checksum_snapshot(const char* out_file)
{
    ScanList cur = {0};
    scan_dir("assets", "", &cur, "");
    FILE* f = NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, out_file, "w") != 0)
        f = NULL;
#else
    f = fopen(out_file, "w");
#endif
    if (!f)
    {
        fprintf(stderr, "Failed to write %s\n", out_file);
        scanlist_free(&cur);
        return;
    }
    for (size_t i = 0; i < cur.count; i++)
        fprintf(f, "%s %08X %llu\n", cur.v[i].rel, cur.v[i].crc,
                (unsigned long long) cur.v[i].size);
    fclose(f);
    printf("Wrote manifest %s (%zu entries)\n", out_file, cur.count);
    scanlist_free(&cur);
}

static void command_inspect(const char* id_substr)
{
    if (!rogue_asset_manager_init(NULL))
    {
        fprintf(stderr, "asset_manager init failed\n");
        return;
    }
    /* Discover candidate textures containing substring and acquire them */
    ScanList cur = {0};
    scan_dir("assets", "", &cur, ".png,.bmp,.jpg");
    size_t loaded = 0;
    for (size_t i = 0; i < cur.count; i++)
    {
        if (strcasestr(cur.v[i].rel, id_substr))
        {
            int idx = rogue_asset_manager_acquire_texture(cur.v[i].rel);
            if (idx >= 0)
                loaded++;
        }
    }
    printf("Loaded %zu matching textures for inspection.\n", loaded);
    RogueAssetManager* m = rogue_asset_manager_instance();
    for (uint32_t i = 0; i < m->texture_count; i++)
    {
        const RogueAssetTexture* t = &m->textures[i];
        if (!strcasestr(t->path, id_substr) && !strcasestr(t->id, id_substr))
            continue;
        bool ok = false;
        uint32_t crc = rogue_asset_crc32_file(t->path, &ok);
        printf("[T%u] id=%s w=%d h=%d loaded=%d failed=%d ref=%u crc=%08X%s\n", i, t->id, t->width,
               t->height, t->sdl_texture ? 1 : 0, t->load_failed ? 1 : 0, t->ref_count,
               ok ? crc : 0, ok ? "" : " (read fail)");
        const char* deps[16];
        size_t depc = rogue_asset_validation_dep_get(t->id, deps, 16);
        if (depc)
        {
            printf("  deps (%zu): ", depc);
            for (size_t d = 0; d < depc; d++)
            {
                printf("%s%s", deps[d], d + 1 < depc ? "," : "");
            }
            printf("\n");
        }
    }
    rogue_asset_manager_shutdown();
    rogue_asset_validation_shutdown();
    scanlist_free(&cur);
}

static void print_help(const char* argv0)
{
    printf("Usage: %s <command> [options]\n", argv0);
    printf("Commands:\n");
    printf("  stats\n");
    printf("  list [--dir <root>] [--ext .png,.jpg]\n");
    printf("  checksum-verify --manifest <file>\n");
    printf("  checksum-snapshot --manifest <out_file>\n");
    printf("  diff --baseline <file>\n");
    printf("  inspect --id <substring>\n");
}

int asset_tool_main(int argc, char** argv)
{
    if (argc < 2)
    {
        print_help(argv[0]);
        return 0;
    }
    const char* cmd = argv[1];
    if (strcmp(cmd, "stats") == 0)
    {
        command_stats();
        return 0;
    }
    if (strcmp(cmd, "list") == 0)
    {
        const char* dir = "assets";
        const char* ext = NULL;
        for (int i = 2; i < argc; i++)
        {
            if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc)
                dir = argv[++i];
            else if (strcmp(argv[i], "--ext") == 0 && i + 1 < argc)
                ext = argv[++i];
        }
        command_list(dir, ext ? ext : "");
        return 0;
    }
    if (strcmp(cmd, "checksum-verify") == 0)
    {
        const char* man = NULL;
        for (int i = 2; i < argc; i++)
            if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc)
                man = argv[++i];
        if (!man)
        {
            fprintf(stderr, "--manifest required\n");
            return 1;
        }
        command_checksum_verify(man);
        return 0;
    }
    if (strcmp(cmd, "checksum-snapshot") == 0)
    {
        const char* out = NULL;
        for (int i = 2; i < argc; i++)
            if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc)
                out = argv[++i];
        if (!out)
        {
            fprintf(stderr, "--manifest <out> required\n");
            return 1;
        }
        command_checksum_snapshot(out);
        return 0;
    }
    if (strcmp(cmd, "diff") == 0)
    {
        const char* base = NULL;
        for (int i = 2; i < argc; i++)
            if (strcmp(argv[i], "--baseline") == 0 && i + 1 < argc)
                base = argv[++i];
        if (!base)
        {
            fprintf(stderr, "--baseline required\n");
            return 1;
        }
        command_diff(base);
        return 0;
    }
    if (strcmp(cmd, "inspect") == 0)
    {
        const char* id = NULL;
        for (int i = 2; i < argc; i++)
            if (strcmp(argv[i], "--id") == 0 && i + 1 < argc)
                id = argv[++i];
        if (!id)
        {
            fprintf(stderr, "--id required\n");
            return 1;
        }
        command_inspect(id);
        return 0;
    }
    print_help(argv[0]);
    return 0;
}

/*
 * Standalone entry point. Some unit tests (e.g. test_asset_phase7_tool_cli) want to
 * link against asset_tool_main() directly without pulling in an additional "main".
 * Allow those targets to compile this TU by defining ASSET_TOOL_NO_STANDALONE.
 */
#ifndef ASSET_TOOL_NO_STANDALONE
int main(int argc, char** argv) { return asset_tool_main(argc, argv); }
#endif
