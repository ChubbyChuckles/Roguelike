/* Phase 13: Modding & Data Extensibility implementation
 * Implements descriptor pack loading & hot reload (initial subset: biome descriptors).
 * Provides schema migration registration (text-level), validation, and atomic swap semantics.
 */
#include "world/world_gen.h"
#include "world/world_gen_biome_desc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Active registries (currently only biome) */
static RogueBiomeRegistry g_active_biomes; /* schema-owned */
static int g_pack_schema_version = 0;

/* Migration entries */
typedef struct MigrationEntry
{
    int old_v;
    int target_v;
    RoguePackMigrationFn fn;
} MigrationEntry;
#define ROGUE_MAX_MIGRATIONS 16
static MigrationEntry g_migrations[ROGUE_MAX_MIGRATIONS];
static int g_migration_count = 0;

int rogue_pack_register_migration(int old_version, int target_version, RoguePackMigrationFn fn)
{
    if (!fn || g_migration_count >= ROGUE_MAX_MIGRATIONS)
        return 0;
    /* simple duplicate check */
    for (int i = 0; i < g_migration_count; i++)
        if (g_migrations[i].old_v == old_version && g_migrations[i].target_v == target_version)
            return 0;
    g_migrations[g_migration_count++] = (MigrationEntry){old_version, target_version, fn};
    return 1;
}

static RoguePackMigrationFn find_migration(int old_v, int target_v)
{
    for (int i = 0; i < g_migration_count; i++)
        if (g_migrations[i].old_v == old_v && g_migrations[i].target_v == target_v)
            return g_migrations[i].fn;
    return NULL;
}

/* Simple helper: copy file into buffer (null-terminated). Uses safer fopen_s on MSVC. */
static int read_text_file(const char* path, char** out_buf, size_t* out_len)
{
    FILE* f = NULL;
#ifdef _WIN32
    if (fopen_s(&f, path, "rb") != 0)
        return 0;
#else
    f = fopen(path, "rb");
    if (!f)
        return 0;
#endif
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return 0;
    }
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return 0;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return 0;
    }
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    if (rd != (size_t) sz)
    {
        free(buf);
        return 0;
    }
    buf[sz] = '\0';
    *out_buf = buf;
    if (out_len)
        *out_len = (size_t) sz;
    return 1;
}

/* Directory scan minimal (platform: assume POSIX-like or provide Win fallback using _findfirst).
 * For simplicity in tests, we expect a manifest file: pack.meta (key=value lines) and biome files
 * *.biome.cfg in the directory.
 */

static int parse_schema_version(const char* meta_text)
{
    /* look for line starting with schema_version= */
    const char* p = meta_text;
    while (*p)
    {
        if (strncmp(p, "schema_version=", 15) == 0)
        {
            return atoi(p + 15);
        }
        while (*p && *p != '\n')
            p++;
        if (*p == '\n')
            p++;
    }
    return 0;
}

/* Platform abstraction: list files ending with suffix into dynamic array */
#ifdef _WIN32
#include <io.h>
#include <windows.h>
static int list_biome_files(const char* dir, char*** out_list, int* out_count)
{
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s\\*.biome.cfg", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        *out_list = NULL;
        *out_count = 0;
        return 1;
    }
    int cap = 8, count = 0;
    char** list = (char**) malloc(sizeof(char*) * cap);
    if (!list)
    {
        FindClose(h);
        return 0;
    }
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (count >= cap)
            {
                cap *= 2;
                char** nl = (char**) realloc(list, sizeof(char*) * cap);
                if (!nl)
                {
                    for (int i = 0; i < count; i++)
                        free(list[i]);
                    free(list);
                    FindClose(h);
                    return 0;
                }
                list = nl;
            }
            char full[512];
            snprintf(full, sizeof(full), "%s\\%s", dir, fd.cFileName);
            list[count] = (char*) malloc(strlen(full) + 1);
            if (!list[count])
            {
                for (int i = 0; i < count; i++)
                    free(list[i]);
                free(list);
                FindClose(h);
                return 0;
            }
            memcpy(list[count], full, strlen(full) + 1);
            count++;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    *out_list = list;
    *out_count = count;
    return 1;
}
#else
#include <dirent.h>
static int list_biome_files(const char* dir, char*** out_list, int* out_count)
{
    DIR* d = opendir(dir);
    if (!d)
    {
        *out_list = NULL;
        *out_count = 0;
        return 1;
    }
    int cap = 8, count = 0;
    char** list = (char**) malloc(sizeof(char*) * cap);
    if (!list)
    {
        closedir(d);
        return 0;
    }
    struct dirent* e;
    while ((e = readdir(d)))
    {
        size_t n = strlen(e->d_name);
        if (n >= 10 && strcmp(e->d_name + n - 10, ".biome.cfg") == 0)
        {
            if (count >= cap)
            {
                cap *= 2;
                char** nl = (char**) realloc(list, sizeof(char*) * cap);
                if (!nl)
                {
                    for (int i = 0; i < count; i++)
                        free(list[i]);
                    free(list);
                    closedir(d);
                    return 0;
                }
                list = nl;
            }
            char full[512];
            snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
            list[count] = strdup(full);
            if (!list[count])
            {
                for (int i = 0; i < count; i++)
                    free(list[i]);
                free(list);
                closedir(d);
                return 0;
            }
            count++;
        }
    }
    closedir(d);
    *out_list = list;
    *out_count = count;
    return 1;
}
#endif

/* Validate biome registry basic invariants */
static int validate_biomes(const RogueBiomeRegistry* reg)
{
    if (!reg || reg->count <= 0)
        return 0;
    for (int i = 0; i < reg->count; i++)
    {
        const RogueBiomeDescriptor* d = &reg->biomes[i];
        if (d->tile_weight_count <= 0)
            return 0;
    }
    return 1;
}

static void ensure_pack_state_init(void)
{
    if (!g_active_biomes.biomes && g_active_biomes.capacity == 0)
    {
        memset(&g_active_biomes, 0, sizeof(g_active_biomes));
    }
}

int rogue_pack_load_dir(const char* dir_path, int allow_hot_reload,
                        RogueDescriptorPackMeta* out_meta, char* err, size_t err_cap)
{
    ensure_pack_state_init();
    if (err && err_cap > 0)
        err[0] = '\0';
    if (!dir_path)
        return ROGUE_PACK_LOAD_ERR_IO;
    /* read meta file */
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s%spack.meta", dir_path,
             (dir_path[strlen(dir_path) - 1] == '/' || dir_path[strlen(dir_path) - 1] == '\\')
                 ? ""
                 : "/");
    char* meta_text = NULL;
    size_t meta_len = 0;
    if (!read_text_file(meta_path, &meta_text, &meta_len))
    {
        if (err)
            snprintf(err, err_cap, "missing pack.meta");
        return ROGUE_PACK_LOAD_ERR_IO;
    }
    int schema_version = parse_schema_version(meta_text);
    if (schema_version <= 0)
    {
        if (err)
            snprintf(err, err_cap, "invalid schema_version");
        free(meta_text);
        return ROGUE_PACK_LOAD_ERR_PARSE;
    }

    /* list biome files */
    char** files = NULL;
    int file_count = 0;
    if (!list_biome_files(dir_path, &files, &file_count))
    {
        free(meta_text);
        if (err)
            snprintf(err, err_cap, "biome file list failed");
        return ROGUE_PACK_LOAD_ERR_IO;
    }
    if (file_count == 0)
    {
        free(meta_text);
        if (err)
            snprintf(err, err_cap, "no biome files");
        return ROGUE_PACK_LOAD_ERR_VALIDATION;
    }

    RogueBiomeRegistry temp_reg;
    rogue_biome_registry_init(&temp_reg);
    int load_ok = 1;
    for (int i = 0; i < file_count && load_ok; i++)
    {
        char* text = NULL;
        size_t len = 0;
        if (!read_text_file(files[i], &text, &len))
        {
            load_ok = 0;
            if (err)
                snprintf(err, err_cap, "read fail %s", files[i]);
            break;
        }
        /* apply migrations if needed (only direct single-step supported for simplicity) */
        int current_version =
            schema_version; /* assumption: files already at schema_version; migrations would be used
                               once older packs introduced */
        (void) current_version; /* placeholder for future multi-step migration */
        RogueBiomeDescriptor desc;
        char perr[128];
        if (!rogue_biome_descriptor_parse_cfg(text, &desc, perr, sizeof(perr)))
        {
            load_ok = 0;
            if (err)
                snprintf(err, err_cap, "parse %s: %s", files[i], perr);
            free(text);
            break;
        }
        if (rogue_biome_registry_add(&temp_reg, &desc) < 0)
        {
            load_ok = 0;
            if (err)
                snprintf(err, err_cap, "registry add fail");
            free(text);
            break;
        }
        free(text);
    }
    for (int i = 0; i < file_count; i++)
        free(files[i]);
    free(files);
    free(meta_text);
    if (!load_ok)
    {
        rogue_biome_registry_free(&temp_reg);
        return ROGUE_PACK_LOAD_ERR_PARSE;
    }
    if (!validate_biomes(&temp_reg))
    {
        if (err)
            snprintf(err, err_cap, "validation failed");
        rogue_biome_registry_free(&temp_reg);
        return ROGUE_PACK_LOAD_ERR_VALIDATION;
    }

    /* atomic swap */
    if (g_active_biomes.biomes)
    {
        RogueBiomeRegistry old = g_active_biomes;
        g_active_biomes = temp_reg;
        rogue_biome_registry_free(&old);
    }
    else
        g_active_biomes = temp_reg;
    g_pack_schema_version = schema_version;
    if (out_meta)
    {
        out_meta->schema_version = schema_version;
        out_meta->source_path = dir_path;
    }
    (void) allow_hot_reload; /* semantics identical for now (always swap) */
    return ROGUE_PACK_LOAD_OK;
}

int rogue_pack_active_schema_version(void) { return g_pack_schema_version; }

int rogue_pack_validate_active(void) { return validate_biomes(&g_active_biomes); }

void rogue_pack_summary(char* buf, size_t cap)
{
    if (!buf || cap == 0)
        return;
    snprintf(buf, cap, "schema=%d biomes=%d", g_pack_schema_version, g_active_biomes.count);
}

void rogue_pack_clear(void)
{
    if (g_active_biomes.biomes)
    {
        rogue_biome_registry_free(&g_active_biomes);
    }
    memset(&g_active_biomes, 0, sizeof(g_active_biomes));
    g_pack_schema_version = 0;
}

int rogue_pack_cli_validate(const char* dir_path)
{
    char err[256];
    int r = rogue_pack_load_dir(dir_path, 0, NULL, err, sizeof(err));
    if (r != ROGUE_PACK_LOAD_OK)
    {
        fprintf(stderr, "Pack load failed: %s\n", err);
        return 0;
    }
    printf("Pack OK: version %d\n", g_pack_schema_version);
    return 1;
}
