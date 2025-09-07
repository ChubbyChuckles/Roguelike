/* asset_validation.c - Phase 4 implementation */
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#include "asset_validation.h"
#include "asset_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

/* ---------- File Existence ---------- */
bool rogue_asset_file_exists(const char* path)
{
    if (!path || !*path)
        return false;
#ifdef _WIN32
    struct _stat s;
    return _stat(path, &s) == 0 && (s.st_mode & _S_IFREG);
#else
    struct stat s;
    return stat(path, &s) == 0 && S_ISREG(s.st_mode);
#endif
}

/* ---------- CRC32 ---------- */
static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void crc32_init_table(void)
{
    if (crc32_table_init)
        return;
    for (uint32_t i = 0; i < 256; ++i)
    {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_table_init = true;
}

uint32_t rogue_asset_crc32_file(const char* path, bool* ok)
{
    if (ok)
        *ok = false;
    if (!rogue_asset_file_exists(path))
    {
        /* If path contains "../" attempt to normalize by stripping leading ../ segments */
        if (path && strstr(path, "../") == path)
        {
            const char* p = path;
            while (strncmp(p, "../", 3) == 0)
                p += 3;
            if (rogue_asset_file_exists(p))
                path = p; /* use normalized */
        }
        if (!rogue_asset_file_exists(path))
            return 0;
    }
    FILE* f = NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, path, "rb") != 0)
        f = NULL;
#else
    f = fopen(path, "rb"); /* POSIX */
#endif
    if (!f)
        return 0;
    crc32_init_table();
    uint32_t crc = 0xFFFFFFFFu;
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
    {
        for (size_t i = 0; i < n; ++i)
            crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    fclose(f);
    crc ^= 0xFFFFFFFFu;
    if (ok)
        *ok = true;
    return crc;
}

/* ---------- Checksum Registry ---------- */
typedef struct ChecksumEntry
{
    char* path;
    uint32_t expected;
    struct ChecksumEntry* next;
} ChecksumEntry;

static ChecksumEntry* g_checksums = NULL;

bool rogue_asset_checksum_register(const char* path, uint32_t crc32)
{
    if (!path)
        return false;
    /* Overwrite if exists */
    for (ChecksumEntry* e = g_checksums; e; e = e->next)
        if (strcasecmp(e->path, path) == 0)
        {
            e->expected = crc32;
            return true;
        }
    ChecksumEntry* ne = (ChecksumEntry*) malloc(sizeof *ne);
    if (!ne)
        return false;
#ifdef _WIN32
    ne->path = _strdup(path);
#else
    ne->path = strdup(path);
#endif
    ne->expected = crc32;
    ne->next = g_checksums;
    g_checksums = ne;
    return true;
}

bool rogue_asset_checksum_verify_one(const char* path, uint32_t expected_crc32)
{
    bool ok = false;
    uint32_t crc = rogue_asset_crc32_file(path, &ok);
    return ok && crc == expected_crc32;
}

bool rogue_asset_checksum_verify_all(void)
{
    for (ChecksumEntry* e = g_checksums; e; e = e->next)
        if (!rogue_asset_checksum_verify_one(e->path, e->expected))
            return false;
    return true;
}

/* ---------- Fallback Texture Path ---------- */
static char g_fallback_texture[260];

void rogue_asset_set_fallback_texture(const char* path)
{
    if (!path)
    {
        g_fallback_texture[0] = '\0';
        return;
    }
    size_t len = strlen(path);
    if (len >= sizeof g_fallback_texture)
        len = sizeof g_fallback_texture - 1;
    memcpy(g_fallback_texture, path, len);
    g_fallback_texture[len] = '\0';
}

const char* rogue_asset_get_fallback_texture(void)
{
    return g_fallback_texture[0] ? g_fallback_texture : NULL;
}

/* ---------- Dependency Tracking ---------- */
typedef struct DepEdge
{
    char* owner;
    char* dep;
    struct DepEdge* next;
} DepEdge;

static DepEdge* g_dep_edges = NULL;
/* Extended usage tracking (lifecycle scoped) */
static uint32_t g_peak_texture_records = 0;
static uint32_t g_peak_audio_records = 0;
static uint32_t g_reloads_detected = 0;
static uint32_t g_last_reload_ms = 0; /* SDL_GetTicks snapshot */

bool rogue_asset_dep_add(const char* owner_id, const char* dependency_id)
{
    if (!owner_id || !dependency_id)
        return false;
    /* prevent duplicates */
    for (DepEdge* e = g_dep_edges; e; e = e->next)
        if (strcasecmp(e->owner, owner_id) == 0 && strcasecmp(e->dep, dependency_id) == 0)
            return true;
    DepEdge* ne = (DepEdge*) malloc(sizeof *ne);
    if (!ne)
        return false;
#ifdef _WIN32
    ne->owner = _strdup(owner_id);
    ne->dep = _strdup(dependency_id);
#else
    ne->owner = strdup(owner_id);
    ne->dep = strdup(dependency_id);
#endif
    ne->next = g_dep_edges;
    g_dep_edges = ne;
    return true;
}

size_t rogue_asset_validation_dep_get(const char* owner_id, const char** out, size_t max)
{
    if (!owner_id)
        return 0;
    size_t count = 0;
    for (DepEdge* e = g_dep_edges; e; e = e->next)
    {
        if (strcasecmp(e->owner, owner_id) == 0)
        {
            if (out && count < max)
                out[count] = e->dep;
            count++;
        }
    }
    return count;
}

/* ---------- Usage Stats ---------- */
RogueAssetUsageStats rogue_asset_usage_stats(void)
{
    RogueAssetUsageStats s = {0};
    RogueAssetManager* m = rogue_asset_manager_instance();
    if (!m || !m->initialized)
        return s;
    s.texture_records = m->texture_count;
    s.audio_records = m->audio_count;
    for (uint32_t i = 0; i < m->texture_count; ++i)
    {
        if (m->textures[i].load_failed)
            s.textures_failed++;
        if (m->textures[i].sdl_texture)
            s.textures_with_handle++;
    }
    for (uint32_t i = 0; i < m->audio_count; ++i)
    {
        if (m->audio[i].load_failed)
            s.audio_failed++;
        if (m->audio[i].sdl_chunk)
            s.audio_with_handle++;
    }
    /* update peaks */
    if (s.texture_records > g_peak_texture_records)
        g_peak_texture_records = s.texture_records;
    if (s.audio_records > g_peak_audio_records)
        g_peak_audio_records = s.audio_records;
    s.peak_texture_records = g_peak_texture_records;
    s.peak_audio_records = g_peak_audio_records;
    s.reloads_detected = g_reloads_detected;
    s.last_reload_ms = g_last_reload_ms;
    return s;
}

void rogue_asset_usage_reset_tracking(void)
{
    g_peak_texture_records = 0;
    g_peak_audio_records = 0;
    g_reloads_detected = 0;
    g_last_reload_ms = 0;
}

void rogue_asset_usage_note_reload(void)
{
    g_reloads_detected++;
#ifdef ROGUE_ENABLE_SDL
    extern uint32_t SDL_GetTicks(void);
    g_last_reload_ms = SDL_GetTicks();
#endif
}

/* ---------- JSON Validation Stub ---------- */
bool rogue_asset_json_validate_basic(const char* path)
{
    if (!path)
        return false;
    size_t len = strlen(path);
    if (len < 6)
        return false;
    if (!(len >= 5 && strcasecmp(path + len - 5, ".json") == 0))
        return false;
    bool ok = false;
    (void) rogue_asset_crc32_file(path, &ok); /* existence & readable check */
    return ok;
}

/* ---------- Shutdown ---------- */
void rogue_asset_validation_shutdown(void)
{
    /* free deps */
    DepEdge* de = g_dep_edges;
    while (de)
    {
        DepEdge* nxt = de->next;
        free(de->owner);
        free(de->dep);
        free(de);
        de = nxt;
    }
    g_dep_edges = NULL;
    /* free checksums */
    ChecksumEntry* ce = g_checksums;
    while (ce)
    {
        ChecksumEntry* nxt = ce->next;
        free(ce->path);
        free(ce);
        ce = nxt;
    }
    g_checksums = NULL;
    g_fallback_texture[0] = '\0';
    rogue_asset_usage_reset_tracking();
}
