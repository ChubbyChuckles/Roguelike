#include "save_manager.h"
#include "../../game/buffs.h"
#include "../../world/tilemap.h"
#include "../app/app_state.h"
#include "../loot/loot_instances.h"
#include "../skills/skills.h"
#include "../vendor/vendor.h"
#include "persistence.h"
#include "save_intern.h"
#include "save_internal.h"
#include "save_paths.h"
#include "save_replay.h"
#include "save_utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* qsort, malloc */
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif
#include "../equipment/equipment.h"
#include "../inventory/inventory_entries.h"   /* Phase 1.6 inventory entries persistence */
#include "../inventory/inventory_query.h"     /* Phase 4.4 saved searches persistence */
#include "../inventory/inventory_tag_rules.h" /* Phase 3.3 auto-tag rules persistence */
#include "../inventory/inventory_tags.h"      /* Phase 3 inventory metadata */
/* Centralized logging (default WARN; DEBUG via ROGUE_LOG_LEVEL) */
#include "../../util/log.h"

/* Compatibility shims to new modular internals to minimize invasive edits */
#define g_components g_save_components
#define g_component_count g_save_component_count
#define g_initialized g_save_initialized
#define g_migrations_registered g_save_migrations_registered
#define g_migrations g_save_migrations
#define g_migration_count g_save_migration_count
#define g_last_migration_steps g_save_last_migration_steps
#define g_last_migration_failed g_save_last_migration_failed
#define g_last_migration_ms g_save_last_migration_ms
#define g_debug_json_dump g_save_debug_json_dump
#define g_last_tamper_flags g_save_last_tamper_flags
#define g_last_recovery_used g_save_last_recovery_used
#define g_sig_provider g_save_sig_provider
#define g_incremental_enabled g_save_incremental_enabled
#define g_dirty_mask g_save_dirty_mask
#define g_cached_sections g_save_cached_sections
#define g_last_sections_reused g_save_last_sections_reused
#define g_last_sections_written g_save_last_sections_written
#define g_last_sha256 g_save_last_sha256
#define g_durable_writes g_save_durable_writes
#define g_last_save_rc g_save_last_rc
#define g_last_save_bytes g_save_last_bytes
#define g_last_save_ms g_save_last_ms
#define g_autosave_interval_ms g_save_autosave_interval_ms
#define g_autosave_throttle_ms g_save_autosave_throttle_ms
#define g_autosave_count g_save_autosave_count
#define build_slot_path rogue_build_slot_path
#define build_autosave_path rogue_build_autosave_path
#define build_backup_path rogue_build_backup_path
/* compression toggles now live in incremental module */
#define g_compress_enabled (rogue__compress_enabled())
#define g_compress_min_bytes (rogue__compress_min_bytes())
/* varuint helper */
#define read_varuint rogue_read_varuint
#define write_varuint rogue_write_varuint

/* local reentrancy guard */
static int g_in_save = 0;

/* Moved to save_globals.c */
#if 0
static RogueSaveComponent g_components[ROGUE_SAVE_MAX_COMPONENTS];
static int g_component_count = 0;
static int g_initialized = 0;
static int g_migrations_registered = 0; /* ensure migrations registered once */
#endif

/* Migration registry (linear chain for now) */
/* Moved to save_globals.c */
#if 0
static RogueSaveMigration g_migrations[16];
static int g_migration_count = 0;
static int g_durable_writes = 0;       /* fsync/_commit toggle */
static int g_last_migration_steps = 0; /* metrics */
static int g_last_migration_failed = 0;
static double g_last_migration_ms = 0.0;    /* milliseconds */
static int g_debug_json_dump = 0;           /* Phase 3.2 debug export toggle */
static uint32_t g_active_write_version = 0; /* version of file currently being written */
static uint32_t g_active_read_version = 0;  /* version of file currently being read */
static int g_compress_enabled = 0;
static int g_compress_min_bytes = 64;                           /* Phase 3.6 */
static uint32_t g_last_tamper_flags = 0;                        /* Phase 4.3 tamper flags */
static int g_last_recovery_used = 0;                            /* Phase 4.4 */
static const RogueSaveSignatureProvider* g_sig_provider = NULL; /* v9 */
/* Incremental save state (Phase 5.* runtime only; does not change on-disk version) */
static int g_incremental_enabled = 0;
#endif
/* Phase 17.5 record-level diff deferred (no additional state) */
/* Provided by save_globals.c & save_incremental.c */
/* Autosave scheduling (Phase 6) */
/* Autosave and incremental moved */

/* Forward decl: inventory diff probe used by save loop to decide reuse vs rewrite (Phase 17.5) */
int inventory_component_probe_and_prepare_reuse(void);
/* Autosave/status/compression moved */
/* String intern moved to save_intern.c */

/* Numeric width compile-time assertions (Phase 3.3) */
#if defined(_MSC_VER)
static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
#else
_Static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
_Static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
_Static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
#endif

/* Unsigned LEB128 style varint (7 bits per byte) for counts & small ids (Phase 3.4) */
/* varuint moved to save_utils.c */

/* SHA256 and signature provider moved to save_utils.c and save_security.c */
/* expose hex helper using g_save_last_sha256 */
const unsigned char* rogue_save_last_sha256(void) { return g_save_last_sha256; }
void rogue_save_last_sha256_hex(char out[65])
{
    static const char* hx = "0123456789abcdef";
    for (int i = 0; i < 32; i++)
    {
        out[2 * i] = hx[g_save_last_sha256[i] >> 4];
        out[2 * i + 1] = hx[g_save_last_sha256[i] & 0xF];
    }
    out[64] = '\0';
}

#include <errno.h>

int rogue_save_manager_delete_slot(int slot_index)
{
    if (slot_index < 0 || slot_index >= ROGUE_SAVE_SLOT_COUNT)
        return -1;
    char path[128];
    snprintf(path, sizeof path, "save_slot_%d.sav", slot_index);
    if (remove(path) != 0)
    {
        if (errno != ENOENT)
            return -2;
    }
    char json_path[128];
    snprintf(json_path, sizeof json_path, "%s", rogue_build_json_path(slot_index));
    remove(json_path);
    return 0;
}

/* Replay moved to save_replay.c */
/* Varuint helper provided by save_utils.c */

int rogue_save_last_migration_steps(void) { return g_save_last_migration_steps; }
int rogue_save_last_migration_failed(void) { return g_save_last_migration_failed; }
double rogue_save_last_migration_ms(void) { return g_save_last_migration_ms; }

/* CRC32 provided by save_utils.c */

/* use shared helper */

/* registration moved to save_manager_core.c */

static int cmp_comp(const void* a, const void* b)
{
    const RogueSaveComponent* ca = a;
    const RogueSaveComponent* cb = b;
    return (ca->id - cb->id);
}

/* Core migration registration (defined later) */
/* init moved to save_manager_core.c */

/* migration registration moved */

void rogue_save_manager_reset_for_tests(void)
{
    g_save_component_count = 0;
    g_save_initialized = 0;
    g_save_migration_count = 0;
    g_save_migrations_registered = 0;
    g_save_last_migration_steps = 0;
    g_save_last_migration_failed = 0;
    g_save_last_migration_ms = 0.0;
    /* Ensure test isolation for save paths */
    rogue_save_paths_set_prefix_tests();
}
int rogue_save_set_debug_json(int enabled)
{
    g_save_debug_json_dump = enabled ? 1 : 0;
    return 0;
}
/* String interning implemented in save_intern.c */

/* path builders moved to save_paths.c */

int rogue_save_read_descriptor(int slot_index, RogueSaveDescriptor* out_desc)
{
    if (!out_desc)
        return -1;
    if (slot_index < 0 || slot_index >= ROGUE_SAVE_SLOT_COUNT)
        return -1;
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, build_slot_path(slot_index), "rb");
#else
    f = fopen(build_slot_path(slot_index), "rb");
#endif
    if (!f)
        return -2;
    RogueSaveDescriptor d;
    if (fread(&d, sizeof d, 1, f) != 1)
    {
        fclose(f);
        return -3;
    }
    fclose(f);
    /* Basic validation: reject obviously invalid/future headers */
    if (d.version <= 0 || d.version > ROGUE_SAVE_FORMAT_VERSION)
    {
        return -4; /* corrupt or unsupported future version */
    }
    *out_desc = d;
    return 0;
}
/* Path helpers in save_paths.c */

/* Endianness helper provided by save_utils.c */

static int internal_save_to(const char* final_path)
{
    if (g_in_save)
        return -99;
    g_in_save = 1;
    double t0 = (double) clock();
    qsort(g_components, g_component_count, sizeof(RogueSaveComponent), cmp_comp);
    /* Unique temp path to avoid collisions under parallel test processes */
    char tmp_path[160];
#if defined(_WIN32)
    unsigned pid = (unsigned) _getpid();
#else
    unsigned pid = (unsigned) getpid();
#endif
    unsigned t = (unsigned) time(NULL);
    unsigned clk = (unsigned) clock();
    snprintf(tmp_path, sizeof tmp_path, "./tmp_save_%u_%u_%u.tmp", t, pid, clk);
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, tmp_path, "w+b");
#else
    f = fopen(tmp_path, "w+b");
#endif
    if (!f)
        return -2;
    RogueSaveDescriptor desc = {0};
    desc.version = ROGUE_SAVE_FORMAT_VERSION;
    desc.timestamp_unix = (uint32_t) time(NULL);
    g_active_write_version = desc.version;
    if (fwrite(&desc, sizeof desc, 1, f) != 1)
    {
        fclose(f);
        return -3;
    }
    desc.section_count = 0;
    desc.component_mask = 0;
    /* Write all sections */
    g_last_sections_reused = 0;
    g_last_sections_written = 0;
    for (int i = 0; i < g_component_count; i++)
    {
        ROGUE_LOG_DEBUG("writing component idx=%d id=%d name=%s at_offset=%ld", i,
                        g_components[i].id, g_components[i].name ? g_components[i].name : "?",
                        ftell(f));
        const RogueSaveComponent* c = &g_components[i];
        long start = ftell(f);
        if (desc.version >= 3)
        {
            uint16_t id16 = (uint16_t) c->id;
            uint32_t size_placeholder32 = 0;
            if (fwrite(&id16, sizeof id16, 1, f) != 1)
            {
                fclose(f);
                return -4;
            }
            if (fwrite(&size_placeholder32, sizeof size_placeholder32, 1, f) != 1)
            {
                fclose(f);
                return -4;
            }
            long payload_start = ftell(f);
            int reused_cache = 0;
            /* Phase 17.5: If inventory is clean but records changed, force fresh write and let
               inventory component compute per-record diff metrics. If unchanged, update metrics
               here so tests can observe reused==count, rewritten==0 even when reusing. */
            if (g_incremental_enabled && c->id == ROGUE_SAVE_COMP_INVENTORY &&
                !(g_dirty_mask & (1u << c->id)))
            {
                int changed = inventory_component_probe_and_prepare_reuse();
                if (changed)
                {
                    g_dirty_mask |= (1u << c->id); /* force fresh write */
                }
            }
            if (g_incremental_enabled && !(g_dirty_mask & (1u << c->id)))
            { /* attempt reuse */
                for (int k = 0; k < ROGUE_SAVE_MAX_COMPONENTS; k++)
                    if (g_cached_sections[k].valid && g_cached_sections[k].id == c->id)
                    {
                        /* Reuse cached raw payload (already contains optional uncompressed_size
                         * prefix for compressed data) */
                        if (fwrite(g_cached_sections[k].data, 1, g_cached_sections[k].size, f) !=
                            (size_t) g_cached_sections[k].size)
                        {
                            fclose(f);
                            return -5;
                        }
                        long end = ftell(f);
                        uint32_t section_size = g_cached_sections[k].size;
                        fseek(f, start + sizeof(uint16_t), SEEK_SET);
                        fwrite(&section_size, sizeof section_size, 1, f);
                        fseek(f, end, SEEK_SET);
                        if (desc.version >= 7)
                        {
                            uint32_t sec_crc = g_cached_sections[k].crc32;
                            fwrite(&sec_crc, sizeof sec_crc, 1, f);
                        }
                        reused_cache = 1;
                        break;
                    }
            }
            if (reused_cache)
            {
                g_last_sections_reused++;
            }
            else
            {
                g_last_sections_written++; /* fresh payload */
                if (desc.version >= 6 && g_compress_enabled)
                {
                    FILE* mem = NULL;
#if defined(_MSC_VER)
                    tmpfile_s(&mem);
#else
                    mem = tmpfile();
#endif
                    if (!mem)
                    {
                        fclose(f);
                        return -5;
                    }
                    if (c->write_fn(mem) != 0)
                    {
                        fclose(mem);
                        fclose(f);
                        return -5;
                    }
                    fflush(mem);
                    fseek(mem, 0, SEEK_END);
                    long usz = ftell(mem);
                    fseek(mem, 0, SEEK_SET);
                    unsigned char* ubuf = (unsigned char*) malloc((size_t) usz);
                    if (!ubuf)
                    {
                        fclose(mem);
                        fclose(f);
                        return -5;
                    }
                    fread(ubuf, 1, (size_t) usz, mem);
                    fclose(mem);
                    unsigned char* cbuf = (unsigned char*) malloc((size_t) usz * 2 + 16);
                    if (!cbuf)
                    {
                        free(ubuf);
                        fclose(f);
                        return -5;
                    }
                    size_t ci = 0;
                    for (long p = 0; p < usz;)
                    {
                        unsigned char b = ubuf[p];
                        int run = 1;
                        while (p + run < usz && ubuf[p + run] == b && run < 255)
                            run++;
                        cbuf[ci++] = b;
                        cbuf[ci++] = (unsigned char) run;
                        p += run;
                    }
                    int use_compressed = (usz >= g_compress_min_bytes && ci < (size_t) usz);
                    if (use_compressed)
                    {
                        long payload_pos = ftell(f);
                        fwrite(&usz, sizeof(uint32_t), 1, f);
                        if (fwrite(cbuf, 1, ci, f) != (size_t) ci)
                        {
                            free(cbuf);
                            free(ubuf);
                            fclose(f);
                            return -5;
                        }
                        long end = ftell(f);
                        uint32_t stored_size = (uint32_t) (end - payload_pos);
                        uint32_t header_size_field = stored_size | 0x80000000u;
                        fseek(f, start + sizeof(uint16_t), SEEK_SET);
                        fwrite(&header_size_field, sizeof header_size_field, 1, f);
                        fseek(f, end, SEEK_SET);
                    }
                    else
                    {
                        if (fwrite(ubuf, 1, (size_t) usz, f) != (size_t) usz)
                        {
                            free(cbuf);
                            free(ubuf);
                            fclose(f);
                            return -5;
                        }
                        long end = ftell(f);
                        uint32_t section_size = (uint32_t) (end - payload_start);
                        fseek(f, start + sizeof(uint16_t), SEEK_SET);
                        fwrite(&section_size, sizeof section_size, 1, f);
                        fseek(f, end, SEEK_SET);
                    }
                    free(cbuf);
                    free(ubuf);
                }
                else
                {
                    if (c->write_fn(f) != 0)
                    {
                        fclose(f);
                        return -5;
                    }
                    long end = ftell(f);
                    uint32_t section_size = (uint32_t) (end - payload_start);
                    fseek(f, start + sizeof(uint16_t), SEEK_SET);
                    fwrite(&section_size, sizeof section_size, 1, f);
                    fseek(f, end, SEEK_SET);
                }
                if (desc.version >= 7)
                {
                    long end_after_payload = ftell(f);
                    long payload_size = end_after_payload - payload_start;
                    if (payload_size < 0)
                    {
                        fclose(f);
                        return -14;
                    }
                    fseek(f, payload_start, SEEK_SET);
                    unsigned char* tmp = (unsigned char*) malloc((size_t) payload_size);
                    if (!tmp)
                    {
                        fclose(f);
                        return -14;
                    }
                    if (fread(tmp, 1, (size_t) payload_size, f) != (size_t) payload_size)
                    {
                        free(tmp);
                        fclose(f);
                        return -14;
                    }
                    uint32_t sec_crc = rogue_crc32(tmp, (size_t) payload_size);
                    free(tmp);
                    fseek(f, end_after_payload, SEEK_SET);
                    fwrite(&sec_crc, sizeof sec_crc, 1, f);
                    if (g_incremental_enabled)
                    { /* capture cache */
                        int placed = 0;
                        for (int k = 0; k < ROGUE_SAVE_MAX_COMPONENTS; k++)
                        {
                            if (g_cached_sections[k].valid && g_cached_sections[k].id == c->id)
                            { /* update existing (component dirty and rewritten) */
                                if (g_cached_sections[k].data &&
                                    g_cached_sections[k].size != (uint32_t) payload_size)
                                {
                                    unsigned char* nd = (unsigned char*) realloc(
                                        g_cached_sections[k].data, (size_t) payload_size);
                                    if (nd)
                                    {
                                        g_cached_sections[k].data = nd;
                                        g_cached_sections[k].size = (uint32_t) payload_size;
                                    }
                                }
                                if (g_cached_sections[k].data)
                                {
                                    fseek(f, payload_start, SEEK_SET);
                                    fread(g_cached_sections[k].data, 1, (size_t) payload_size, f);
                                    fseek(f, end_after_payload + sizeof(uint32_t), SEEK_SET);
                                    g_cached_sections[k].crc32 = sec_crc;
                                }
                                placed = 1;
                                break;
                            }
                        }
                        if (!placed)
                        {
                            for (int k = 0; k < ROGUE_SAVE_MAX_COMPONENTS; k++)
                            {
                                if (!g_cached_sections[k].valid)
                                {
                                    g_cached_sections[k].id = c->id;
                                    g_cached_sections[k].size = (uint32_t) payload_size;
                                    g_cached_sections[k].data =
                                        (unsigned char*) malloc((size_t) payload_size);
                                    if (g_cached_sections[k].data)
                                    {
                                        fseek(f, payload_start, SEEK_SET);
                                        fread(g_cached_sections[k].data, 1, (size_t) payload_size,
                                              f);
                                        fseek(f, end_after_payload + sizeof(uint32_t), SEEK_SET);
                                        g_cached_sections[k].crc32 = sec_crc;
                                        g_cached_sections[k].valid = 1;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                } /* end if(desc.version>=7) */
            } /* end fresh write vs reused */
            if (g_incremental_enabled)
            {
                g_dirty_mask &= ~(1u << c->id);
            }
        } /* end version>=3 path */
        else
        {
            uint32_t id = (uint32_t) c->id, size_placeholder = 0;
            if (fwrite(&id, sizeof id, 1, f) != 1)
            {
                fclose(f);
                return -4;
            }
            if (fwrite(&size_placeholder, sizeof size_placeholder, 1, f) != 1)
            {
                fclose(f);
                return -4;
            }
            long payload_start = ftell(f);
            if (c->write_fn(f) != 0)
            {
                fclose(f);
                return -5;
            }
            long end = ftell(f);
            uint32_t section_size = (uint32_t) (end - payload_start);
            fseek(f, start + sizeof(uint32_t), SEEK_SET);
            fwrite(&section_size, sizeof section_size, 1, f);
            fseek(f, end, SEEK_SET);
        }
        long after = ftell(f);
        if (after < 0)
        {
            fclose(f);
            return -90;
        }
        uint32_t wrote_bytes = (uint32_t) (after - start);
        desc.section_count++;
        desc.component_mask |= (1u << c->id);
        ROGUE_LOG_DEBUG("finished component id=%d size_with_header=%u section_count=%u mask=0x%X "
                        "reused=%u written=%u",
                        c->id, wrote_bytes, desc.section_count, desc.component_mask,
                        g_last_sections_reused, g_last_sections_written);
    } /* end for each component */
    /* After a full save in incremental mode, all components become clean (unless marked during
     * write) so subsequent save can reuse */
    if (g_incremental_enabled)
    {
        g_dirty_mask = 0;
    }
    /* Marks end of payload (excludes integrity footers) */
    long payload_end = ftell(f);
    /* Compute descriptor CRC (over payload only) */
    size_t crc_region = (size_t) (payload_end - (long) sizeof desc);
    unsigned char* crc_buf = NULL;
    uint32_t crc = 0;
    if (crc_region > 0)
    {
        crc_buf = (unsigned char*) malloc(crc_region);
        if (!crc_buf)
        {
            fclose(f);
            return -13;
        }
        fseek(f, sizeof desc, SEEK_SET);
        if (fread(crc_buf, 1, crc_region, f) != crc_region)
        {
            free(crc_buf);
            fclose(f);
            return -13;
        }
        crc = rogue_crc32(crc_buf, crc_region);
    }
    else
    {
        /* No sections written; define checksum of empty payload as 0 for stability */
        crc = 0u;
    }
    desc.checksum = crc; /* rogue_crc32 returns final xor */
    /* SHA256 footer (v7+) over same region */
    if (desc.version >= 7)
    {
        RogueSHA256Ctx sha;
        rogue_sha256_init(&sha);
        if (crc_region > 0 && crc_buf)
        {
            rogue_sha256_update(&sha, crc_buf, crc_region);
        }
        /* else: digest of empty payload */
        unsigned char digest[32];
        rogue_sha256_final(&sha, digest);
        memcpy(g_last_sha256, digest, 32);
        const char magic[4] = {'S', 'H', '3', '2'};
        fseek(f, 0, SEEK_END);
        fwrite(magic, 1, 4, f);
        fwrite(digest, 1, 32, f);
        /* Optional signature (v9+) signs payload + SHA footer */
        if (desc.version >= 9 && g_sig_provider)
        { /* Signature covers payload + SHA footer */
            size_t total_for_sig = crc_region + 4 + 32;
            unsigned char* sig_src = (unsigned char*) malloc(total_for_sig);
            if (!sig_src)
            {
                if (crc_buf)
                    free(crc_buf);
                fclose(f);
                return -16;
            }
            if (crc_region > 0 && crc_buf)
                memcpy(sig_src, crc_buf, crc_region);
            /* Append SH32 + digest */
            memcpy(sig_src + crc_region, "SH32", 4);
            memcpy(sig_src + crc_region + 4, digest, 32);
            unsigned char sigbuf[1024];
            uint32_t slen = sizeof sigbuf;
            if (g_sig_provider->sign(sig_src, (size_t) total_for_sig, sigbuf, &slen) != 0)
            {
                free(sig_src);
                if (crc_buf)
                    free(crc_buf);
                fclose(f);
                return -16;
            }
            free(sig_src);
            const char smagic[4] = {'S', 'G', 'N', '0'};
            fwrite(&slen, sizeof(uint16_t), 1, f);
            fwrite(smagic, 1, 4, f);
            fwrite(sigbuf, 1, slen, f);
        }
    }
    long file_end = ftell(f);
    desc.total_size = (uint64_t) file_end;
    if (crc_buf)
        free(crc_buf);
    /* Rewrite descriptor with final fields */
    fseek(f, 0, SEEK_SET);
    fwrite(&desc, sizeof desc, 1, f);
    fflush(f);
#if defined(_WIN32)
    if (g_durable_writes)
    {
        int fd = _fileno(f);
        if (fd != -1)
            _commit(fd);
    }
#else
    if (g_durable_writes)
    {
        int fd = fileno(f);
        if (fd != -1)
            fsync(fd);
    }
#endif
    fclose(f);
    /* Move tmp to final and verify success. If move/copy fails, return error. */
    int finalize_ok = 0;
#if defined(_MSC_VER)
    /* Best-effort atomic replacement. If rename fails (e.g., anti-virus locking),
       retry after removing destination; if still failing, fall back to copy. */
    remove(final_path);
    if (rename(tmp_path, final_path) == 0)
    {
        finalize_ok = 1;
    }
    else
    {
        FILE* src = NULL;
        FILE* dst = NULL;
        if (fopen_s(&src, tmp_path, "rb") == 0 && src && fopen_s(&dst, final_path, "wb") == 0 &&
            dst)
        {
            unsigned char buf[8192];
            size_t n;
            int copy_failed = 0;
            while ((n = fread(buf, 1, sizeof buf, src)) > 0)
            {
                if (fwrite(buf, 1, n, dst) != n)
                {
                    copy_failed = 1;
                    break;
                }
            }
            fflush(dst);
            if (g_durable_writes)
            {
                int dfd = _fileno(dst);
                if (dfd != -1)
                    _commit(dfd);
            }
            fclose(src);
            fclose(dst);
            if (!copy_failed)
            {
                finalize_ok = 1;
                remove(tmp_path);
            }
        }
        else
        {
            if (src)
                fclose(src);
            if (dst)
                fclose(dst);
            /* leave tmp file for debugging */
        }
    }
#else
    if (rename(tmp_path, final_path) == 0)
    {
        finalize_ok = 1;
    }
    else
    {
        /* Fallback to copy if rename failed (e.g., cross-device move in unusual setups) */
        FILE* src = fopen(tmp_path, "rb");
        FILE* dst = fopen(final_path, "wb");
        if (src && dst)
        {
            unsigned char buf[8192];
            size_t n;
            int copy_failed = 0;
            while ((n = fread(buf, 1, sizeof buf, src)) > 0)
            {
                if (fwrite(buf, 1, n, dst) != n)
                {
                    copy_failed = 1;
                    break;
                }
            }
            fflush(dst);
            if (g_durable_writes)
            {
                int dfd = fileno(dst);
                if (dfd != -1)
                    fsync(dfd);
            }
            fclose(src);
            fclose(dst);
            if (!copy_failed)
            {
                finalize_ok = 1;
                remove(tmp_path);
            }
        }
        else
        {
            if (src)
                fclose(src);
            if (dst)
                fclose(dst);
        }
    }
#endif
    /* Verify final file exists and is readable */
    if (finalize_ok)
    {
        FILE* vf = NULL;
#if defined(_MSC_VER)
        fopen_s(&vf, final_path, "rb");
#else
        vf = fopen(final_path, "rb");
#endif
        if (vf)
        {
            fclose(vf);
        }
        else
        {
            finalize_ok = 0;
        }
    }
    int rc = finalize_ok ? 0 : -21;
    g_last_save_rc = rc;
    g_last_save_bytes = (uint32_t) desc.total_size;
    g_last_save_ms = ((double) clock() - t0) * 1000.0 / (double) CLOCKS_PER_SEC;
    g_in_save = 0;
    return rc;
}

int rogue_save_manager_save_slot(int slot_index)
{
    if (slot_index < 0 || slot_index >= ROGUE_SAVE_SLOT_COUNT)
        return -1;
    int rc = internal_save_to(build_slot_path(slot_index));
    if (rc == 0 && g_debug_json_dump)
    {
        char buf[2048];
        if (rogue_save_export_json(slot_index, buf, sizeof buf) == 0)
        {
            FILE* jf = NULL;
#if defined(_MSC_VER)
            fopen_s(&jf, rogue_build_json_path(slot_index), "wb");
#else
            jf = fopen(rogue_build_json_path(slot_index), "wb");
#endif
            if (jf)
            {
                fwrite(buf, 1, strlen(buf), jf);
                fclose(jf);
            }
        }
    }
    return rc;
}

/* Phase 15.4: inventory-only save – writes a temporary file containing only the inventory section
 * (component id 3) wrapped in a minimal descriptor. For simplicity we reuse internal_save_to after
 * temporarily filtering component list. */
int rogue_save_manager_save_slot_inventory_only(int slot_index)
{
    if (slot_index < 0 || slot_index >= ROGUE_SAVE_SLOT_COUNT)
        return -1; /* Build a filtered component array */
    RogueSaveComponent inv_only[1];
    int have = 0;
    for (int i = 0; i < g_component_count; i++)
    {
        if (g_components[i].id == ROGUE_SAVE_COMP_INVENTORY)
        {
            inv_only[0] = g_components[i];
            have = 1;
            break;
        }
    }
    if (!have)
        return -2; /* swap */
    RogueSaveComponent backup_all[ROGUE_SAVE_MAX_COMPONENTS];
    int backup_count = g_component_count;
    memcpy(backup_all, g_components, sizeof(RogueSaveComponent) * g_component_count);
    g_component_count = 1;
    g_components[0] = inv_only[0];
    int rc = internal_save_to(build_slot_path(slot_index)); /* restore */
    memcpy(g_components, backup_all, sizeof(RogueSaveComponent) * backup_count);
    g_component_count = backup_count;
    return rc;
}

/* Phase 15.5: backup rotation – copy current slot file to timestamped .bak then prune oldest beyond
 * max_backups */
int rogue_save_manager_backup_rotate(int slot_index, int max_backups)
{
    if (slot_index < 0 || slot_index >= ROGUE_SAVE_SLOT_COUNT || max_backups <= 0)
        return -1;
    const char* src = build_slot_path(slot_index);
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, src, "rb");
#else
    f = fopen(src, "rb");
#endif
    if (!f)
        return -2;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* buf = (unsigned char*) malloc((size_t) sz);
    if (!buf)
    {
        fclose(f);
        return -3;
    }
    if (fread(buf, 1, (size_t) sz, f) != (size_t) sz)
    {
        free(buf);
        fclose(f);
        return -3;
    }
    RogueSaveDescriptor desc;
    if (sz < (long) sizeof desc)
    {
        free(buf);
        fclose(f);
        return -4;
    }
    memcpy(&desc, buf, sizeof desc);
    fclose(f);
    const char* bpath = build_backup_path(slot_index, desc.timestamp_unix);
    FILE* bf = NULL;
#if defined(_MSC_VER)
    fopen_s(&bf, bpath, "wb");
#else
    bf = fopen(bpath, "wb");
#endif
    if (!bf)
    {
        free(buf);
        return -5;
    }
    if (fwrite(buf, 1, (size_t) sz, bf) != (size_t) sz)
    {
        fclose(bf);
        free(buf);
        return -5;
    }
    fclose(bf);
    free(buf);
    /* Prune: list matching backups, keep newest (by timestamp parsed from name) */
    /* Simple linear scan of timestamp range (not directory listing portable). For brevity we skip
     * pruning on Windows without dir API here. */
    (void) max_backups;
    return 0;
}
int rogue_save_manager_autosave(int slot_index)
{
    if (slot_index < 0)
        slot_index = 0;
    return internal_save_to(build_autosave_path(slot_index));
}
int rogue_save_manager_quicksave(void) { return internal_save_to(rogue_build_quicksave_path()); }
int rogue_save_manager_set_durable(int enabled)
{
    g_durable_writes = enabled ? 1 : 0;
    return 0;
}
/* Autosave scheduler lives in save_autosave.c */

/* Internal helper: validate & load entire save file (returns malloc buffer after header) */
static int load_and_validate(const char* path, RogueSaveDescriptor* out_desc,
                             unsigned char** out_buf)
{
    g_last_tamper_flags = 0; /* reset per attempt */
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return -2;
    if (fread(out_desc, sizeof *out_desc, 1, f) != 1)
    {
        fclose(f);
        return -3;
    }
    long file_end = 0;
    fseek(f, 0, SEEK_END);
    file_end = ftell(f);
    if ((uint64_t) file_end != out_desc->total_size)
    {
        fclose(f);
        return -5;
    }
    size_t rest = file_end - (long) sizeof *out_desc;
    size_t footer_bytes = 0;
    uint16_t sig_len = 0;
    int has_sig = 0;
    if (out_desc->version >= 7)
    {
        footer_bytes += 4 + 32;
        if (out_desc->version >= 9 && rest >= (size_t) (4 + 32 + 6))
        { /* probe signature */
            long sig_tail_pos = file_end - 6;
            fseek(f, sig_tail_pos, SEEK_SET);
            unsigned char tail[6];
            if (fread(tail, 1, 6, f) == 6)
            {
                memcpy(&sig_len, tail, 2);
                if (memcmp(tail + 2, "SGN0", 4) == 0)
                {
                    long sig_start = sig_tail_pos - sig_len;
                    if (sig_start >= (long) sizeof *out_desc + (long) (4 + 32) && sig_len < 4096)
                    {
                        footer_bytes += 2 + 4 + sig_len;
                        has_sig = 1;
                    }
                }
            }
        }
    }
    if (rest < footer_bytes)
    {
        fclose(f);
        return -5;
    }
    size_t crc_region = rest - footer_bytes;
    fseek(f, sizeof *out_desc, SEEK_SET);
    unsigned char* buf = (unsigned char*) malloc(rest);
    if (!buf)
    {
        fclose(f);
        return -6;
    }
    size_t n = fread(buf, 1, rest, f);
    if (n != rest)
    {
        free(buf);
        fclose(f);
        return -6;
    }
    uint32_t crc = rogue_crc32(buf, crc_region);
    if (crc != out_desc->checksum)
    {
        g_last_tamper_flags |= ROGUE_TAMPER_FLAG_DESCRIPTOR_CRC;
        free(buf);
        fclose(f);
        return -7;
    }
    if (out_desc->version >= 7)
    { /* SHA footer lives immediately after crc_region */
        if (memcmp(buf + crc_region, "SH32", 4) != 0)
        {
            g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SHA256;
            free(buf);
            fclose(f);
            return ROGUE_SAVE_ERR_SHA256;
        }
        RogueSHA256Ctx sha;
        rogue_sha256_init(&sha);
        rogue_sha256_update(&sha, buf, crc_region);
        unsigned char dg[32];
        rogue_sha256_final(&sha, dg);
        if (memcmp(dg, buf + crc_region + 4, 32) != 0)
        {
            g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SHA256; /* debug */
            fprintf(stderr,
                    "SHA_MISMATCH path=%s crc_region=%zu desc_crc=0x%08X calc_sha_first=0x%02X "
                    "stored_first=0x%02X\n",
                    path, (size_t) crc_region, crc, dg[0], buf[crc_region + 4]);
            free(buf);
            fclose(f);
            return ROGUE_SAVE_ERR_SHA256;
        }
        memcpy(g_last_sha256, buf + crc_region + 4, 32);
        if (out_desc->version >= 9 && has_sig)
        {
            const unsigned char* p = buf + crc_region + 4 + 32;
            uint16_t slen = 0;
            memcpy(&slen, p, 2);
            if (slen == sig_len && memcmp(p + 2, "SGN0", 4) == 0)
            {
                if (g_sig_provider)
                {
                    if (g_sig_provider->verify(buf, crc_region + 4 + 32, p + 6, slen) != 0)
                    {
                        g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SIGNATURE;
                        free(buf);
                        fclose(f);
                        return ROGUE_SAVE_ERR_SHA256;
                    }
                }
            }
        }
    }
    fclose(f);
    *out_buf = buf;
    return 0;
}

int rogue_save_for_each_section(int slot_index, RogueSaveSectionIterFn fn, void* user)
{
    if (slot_index < 0 || slot_index >= ROGUE_SAVE_SLOT_COUNT)
        return -1;
    RogueSaveDescriptor desc;
    unsigned char* buf = NULL;
    int rc = load_and_validate(build_slot_path(slot_index), &desc, &buf);
    if (rc != 0)
        return rc;
    unsigned char* p = buf;
    size_t total_payload = (size_t) (desc.total_size - sizeof desc);
    for (uint32_t s = 0; s < desc.section_count; s++)
    {
        if (desc.version >= 3)
        {
            if ((size_t) (p - buf) + 6 > total_payload)
            {
                free(buf);
                return -8;
            }
            uint16_t id16;
            memcpy(&id16, p, 2);
            uint32_t size_field;
            memcpy(&size_field, p + 2, 4);
            p += 6;
            uint32_t stored_size = size_field & 0x7FFFFFFFu; /* strip compression flag */
            if ((size_t) (p - buf) + stored_size > total_payload)
            {
                free(buf);
                return -8;
            }
            if (fn)
            {
                int frc = fn(&desc, (uint32_t) id16, p, stored_size, user);
                if (frc != 0)
                {
                    free(buf);
                    return frc;
                }
            }
            p += stored_size;
            if (desc.version >= 7)
            {
                if ((size_t) (p - buf) + 4 > total_payload)
                {
                    free(buf);
                    return -8;
                }
                p += 4;
            }
        }
        else
        {
            if ((size_t) (p - buf) + 8 > total_payload)
            {
                free(buf);
                return -8;
            }
            uint32_t id;
            memcpy(&id, p, 4);
            uint32_t size;
            memcpy(&size, p + 4, 4);
            p += 8;
            if ((size_t) (p - buf) + size > total_payload)
            {
                free(buf);
                return -8;
            }
            if (fn)
            {
                int frc = fn(&desc, id, p, size, user);
                if (frc != 0)
                {
                    free(buf);
                    return frc;
                }
            }
            p += size;
        }
    }
    free(buf);
    return 0;
}

int rogue_save_export_json(int slot_index, char* out, size_t out_cap)
{
    if (!out || out_cap == 0)
        return -1;
    RogueSaveDescriptor d;
    unsigned char* buf = NULL;
    int rc = load_and_validate(build_slot_path(slot_index), &d, &buf);
    if (rc != 0)
    {
        return rc;
    }
    int written =
        snprintf(out, out_cap, "{\n  \"version\":%u,\n  \"timestamp\":%u,\n  \"sections\":[",
                 d.version, d.timestamp_unix);
    unsigned char* p = buf;
    size_t total_payload = (size_t) (d.total_size - sizeof d);
    for (uint32_t s = 0; s < d.section_count && written < (int) out_cap; s++)
    {
        uint32_t id = 0;
        uint32_t size = 0;
        size_t header_bytes = (d.version >= 3) ? 6 : 8;
        if ((size_t) (p - buf) + header_bytes > total_payload)
            break;
        if (d.version >= 3)
        {
            uint16_t id16;
            memcpy(&id16, p, 2);
            memcpy(&size, p + 2, 4);
            id = (uint32_t) id16;
            p += 6;
        }
        else
        {
            memcpy(&id, p, 4);
            memcpy(&size, p + 4, 4);
            p += 8;
        }
        uint32_t stored_size = size & 0x7FFFFFFFu;
        int compressed = (d.version >= 6 && (size & 0x80000000u));
        /* Skip payload */
        if (compressed)
        {
            if ((size_t) (p - buf) + stored_size > total_payload)
                break;
            p += stored_size;
        }
        else
        {
            if ((size_t) (p - buf) + stored_size > total_payload)
                break;
            p += stored_size;
        }
        /* Skip per-section CRC (v7+) */
        if (d.version >= 7)
        {
            if ((size_t) (p - buf) + 4 > total_payload)
                break;
            p += 4;
        }
        /* Append section entry (comma separated) */
        written += snprintf(out + written, out_cap - written, "%s{\"id\":%u,\"size\":%u}",
                            (s == 0 ? "" : ","), id, stored_size);
    }
    if (written < (int) out_cap)
        written += snprintf(out + written, out_cap - written, "]\n}\n");
    free(buf);
    if (written >= (int) out_cap)
        return -2;
    return 0;
}

int rogue_save_reload_component_from_slot(int slot_index, int component_id)
{
    if (slot_index < 0 || slot_index >= ROGUE_SAVE_SLOT_COUNT)
        return -1;
    const RogueSaveComponent* comp = rogue_find_component(component_id);
    if (!comp || !comp->read_fn)
        return -2;
    RogueSaveDescriptor d;
    unsigned char* buf = NULL;
    int rc = load_and_validate(build_slot_path(slot_index), &d, &buf);
    if (rc != 0)
        return rc;
    unsigned char* p = buf;
    size_t total_payload = (size_t) (d.total_size - sizeof d);
    int applied = -3;
    for (uint32_t s = 0; s < d.section_count; s++)
    {
        uint32_t id = 0;
        uint32_t raw_size = 0;
        size_t header_bytes = (d.version >= 3) ? 6 : 8;
        if ((size_t) (p - buf) + header_bytes > total_payload)
            break;
        if (d.version >= 3)
        {
            uint16_t id16;
            memcpy(&id16, p, 2);
            memcpy(&raw_size, p + 2, 4);
            id = (uint32_t) id16;
            p += 6;
        }
        else
        {
            memcpy(&id, p, 4);
            memcpy(&raw_size, p + 4, 4);
            p += 8;
        }
        int compressed = (d.version >= 6 && (raw_size & 0x80000000u));
        uint32_t stored_size =
            raw_size &
            0x7FFFFFFFu; /* size on disk including uncompressed_size field for compressed */
        fprintf(stderr,
                "DBG: reload scan s=%u id=%u raw_size=0x%08X stored_size=%u compressed=%d off=%zu "
                "remaining=%zu\n",
                s, id, raw_size, stored_size, compressed, (size_t) (p - buf),
                total_payload - (size_t) (p - buf));
        /* Bounds check payload + optional CRC */
        size_t need = stored_size + (d.version >= 7 ? 4 : 0);
        if ((size_t) (p - buf) + need > total_payload)
            break;
        if ((int) id == component_id)
        {
            /* Prepare uncompressed bytes (decompress if necessary) */
            unsigned char* udata = NULL;
            size_t ulen = 0;
            int free_udata = 0;
            if (compressed)
            {
                if (stored_size < sizeof(uint32_t))
                {
                    free(buf);
                    return -4;
                }
                uint32_t uncompressed_size = 0;
                memcpy(&uncompressed_size, p, 4);
                if (uncompressed_size > (16u * 1024u * 1024u))
                { /* sanity cap 16MB */
                    free(buf);
                    return -4;
                }
                uint32_t comp_bytes = stored_size - 4;
                const unsigned char* csrc = p + 4;
                unsigned char* out = (unsigned char*) malloc(uncompressed_size);
                if (!out)
                {
                    free(buf);
                    return -4;
                }
                size_t ci = 0;
                size_t ui = 0;
                while (ci < comp_bytes && ui < uncompressed_size)
                {
                    unsigned char b = csrc[ci++];
                    if (ci >= comp_bytes)
                        break;
                    unsigned char run = csrc[ci++];
                    for (int r = 0; r < run && ui < uncompressed_size; r++)
                    {
                        out[ui++] = b;
                    }
                }
                udata = out;
                ulen = ui;
                free_udata =
                    1; /* ui may be < uncompressed_size if truncated; still feed what we have */
            }
            else
            {
                udata = p;
                ulen = stored_size;
            }
            /* Feed through temp FILE* to reuse component read_fn contract */
            char tmp[64];
            snprintf(tmp, sizeof tmp, "_tmp_section_%d.bin", component_id);
            FILE* tf = NULL;
#if defined(_MSC_VER)
            fopen_s(&tf, tmp, "wb");
#else
            tf = fopen(tmp, "wb");
#endif
            if (!tf)
            {
                if (free_udata)
                    free(udata);
                free(buf);
                return -4;
            }
            fwrite(udata, 1, ulen, tf);
            fclose(tf);
#if defined(_MSC_VER)
            fopen_s(&tf, tmp, "rb");
#else
            tf = fopen(tmp, "rb");
#endif
            if (!tf)
            {
                if (free_udata)
                    free(udata);
                free(buf);
                return -4;
            }
            comp->read_fn(tf, (uint32_t) ulen);
            fclose(tf);
            remove(tmp);
            if (free_udata)
                free(udata);
            applied = 0; /* success */
            /* Advance pointer past payload */
        }
        /* Advance past stored payload */
        p += stored_size;
        /* Skip per-section CRC (v7+) */
        if (d.version >= 7)
        {
            p += 4;
        }
        if (applied == 0)
            break; /* done */
    }
    free(buf);
    return applied;
}

int rogue_save_manager_load_slot(int slot_index)
{
    if (slot_index < 0 || slot_index >= ROGUE_SAVE_SLOT_COUNT)
        return -1;
    FILE* f = NULL;
#ifdef ROGUE_STRICT_ENDIAN
    if (!rogue_save_format_endianness_is_le())
    {
        return -30;
    }
#endif
#if defined(_MSC_VER)
    fopen_s(&f, build_slot_path(slot_index), "rb");
#else
    f = fopen(build_slot_path(slot_index), "rb");
#endif
    if (!f)
    {
        return -2;
    }
    RogueSaveDescriptor desc;
    if (fread(&desc, sizeof desc, 1, f) != 1)
    {
        fclose(f);
        return -3;
    }
    g_active_read_version = desc.version;
    /* naive validation */
    if (desc.version != ROGUE_SAVE_FORMAT_VERSION)
    {
        /* Load entire payload for potential in-place migration with rollback */
        long file_end_pos = 0;
        fseek(f, 0, SEEK_END);
        file_end_pos = ftell(f);
        long payload_size = file_end_pos - (long) sizeof desc;
        if (payload_size < 0)
        {
            fclose(f);
            return -4;
        }
        unsigned char* payload = (unsigned char*) malloc((size_t) payload_size);
        if (!payload)
        {
            fclose(f);
            return -4;
        }
        fseek(f, sizeof desc, SEEK_SET);
        if (fread(payload, 1, (size_t) payload_size, f) != (size_t) payload_size)
        {
            free(payload);
            fclose(f);
            return -4;
        }
        unsigned char* rollback = (unsigned char*) malloc((size_t) payload_size);
        if (!rollback)
        {
            free(payload);
            fclose(f);
            return -4;
        }
        memcpy(rollback, payload, (size_t) payload_size);
        g_last_migration_steps = 0;
        g_last_migration_failed = 0;
        g_last_migration_ms = 0.0;
        double t0 = (double) clock();
        uint32_t cur = desc.version;
        int failure = 0;
        while (cur < ROGUE_SAVE_FORMAT_VERSION)
        {
            int advanced = 0;
            for (int i = 0; i < g_migration_count; i++)
                if (g_migrations[i].from_version == cur && g_migrations[i].to_version == cur + 1)
                {
                    if (g_migrations[i].apply_fn)
                    {
                        int mrc = g_migrations[i].apply_fn(payload, (size_t) payload_size);
                        if (mrc != 0)
                        {
                            failure = 1;
                            break;
                        }
                    }
                    cur = g_migrations[i].to_version;
                    advanced = 1;
                    g_last_migration_steps++;
                    break;
                }
            if (failure)
                break;
            if (!advanced)
            {
                failure = 1;
                break;
            }
        }
        g_last_migration_ms = ((double) clock() - t0) * 1000.0 / (double) CLOCKS_PER_SEC;
        if (failure || cur != ROGUE_SAVE_FORMAT_VERSION)
        {
            memcpy(payload, rollback, (size_t) payload_size);
            g_last_migration_failed = 1;
            free(payload);
            free(rollback);
            fclose(f);
            return failure ? ROGUE_SAVE_ERR_MIGRATION_FAIL : ROGUE_SAVE_ERR_MIGRATION_CHAIN;
        }
        free(rollback);
        /* For now we don't persist upgraded payload back; future phase may write-through */
        /* Reset file pointer to just after header for section iteration; we still rely on on-disk
         * layout (no structural changes yet) */
        fseek(f, sizeof desc, SEEK_SET);
        free(payload);
    }
    /* checksum + integrity (v7+) with scan-based footer detection */
    long file_end = 0;
    fseek(f, 0, SEEK_END);
    file_end = ftell(f);
    if ((uint64_t) file_end != desc.total_size)
    {
        fclose(f);
        return -5;
    }
    size_t rest = (size_t) (file_end - (long) sizeof desc);
    if (desc.version >= 7)
    {
        fseek(f, sizeof desc, SEEK_SET);
        unsigned char* full = (unsigned char*) malloc(rest);
        if (!full)
        {
            fclose(f);
            return -6;
        }
        if (fread(full, 1, rest, f) != rest)
        {
            free(full);
            fclose(f);
            return -6;
        }
        size_t sha_pos = 0;
        int found = 0;
        for (size_t i = 0; i + 4 <= rest; i++)
        {
            if (memcmp(full + i, "SH32", 4) == 0)
            {
                sha_pos = i;
                found = 1;
            }
        }
        if (!found || sha_pos + 4 + 32 > rest)
        {
            free(full);
            fclose(f);
            g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SHA256;
            return ROGUE_SAVE_ERR_SHA256;
        }
        size_t hashable = sha_pos;
        uint32_t crc = rogue_crc32(full, hashable);
        if (crc != desc.checksum)
        {
            g_last_tamper_flags |= ROGUE_TAMPER_FLAG_DESCRIPTOR_CRC;
            free(full);
            fclose(f);
            return -7;
        }
        RogueSHA256Ctx sha;
        rogue_sha256_init(&sha);
        rogue_sha256_update(&sha, full, hashable);
        unsigned char dg[32];
        rogue_sha256_final(&sha, dg);
        if (memcmp(dg, full + sha_pos + 4, 32) != 0)
        {
            g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SHA256;
            free(full);
            fclose(f);
            return ROGUE_SAVE_ERR_SHA256;
        }
        memcpy(g_last_sha256, full + sha_pos + 4, 32);
        size_t after_sha = sha_pos + 36;
        if (desc.version >= 9 && after_sha < rest)
        {
            size_t remain = rest - after_sha;
            if (remain >= 6)
            {
                uint16_t sig_len = 0;
                memcpy(&sig_len, full + after_sha, 2);
                if (memcmp(full + after_sha + 2, "SGN0", 4) == 0)
                {
                    size_t sig_total = 6 + sig_len;
                    if (sig_total <= remain)
                    {
                        if (g_sig_provider && sig_len > 0)
                        {
                            size_t sign_src_len = hashable + 36;
                            unsigned char* sign_src = (unsigned char*) malloc(sign_src_len);
                            if (!sign_src)
                            {
                                free(full);
                                fclose(f);
                                return -6;
                            }
                            memcpy(sign_src, full, hashable);
                            memcpy(sign_src + hashable, full + sha_pos, 36);
                            if (g_sig_provider->verify(sign_src, sign_src_len, full + after_sha + 6,
                                                       sig_len) != 0)
                            {
                                g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SIGNATURE;
                                free(sign_src);
                                free(full);
                                fclose(f);
                                return ROGUE_SAVE_ERR_SHA256;
                            }
                            free(sign_src);
                        }
                    }
                }
            }
        }
        free(full);
        fseek(f, sizeof desc, SEEK_SET);
    }
    else
    {
        size_t hashable = rest;
        fseek(f, sizeof desc, SEEK_SET);
        unsigned char* buf = (unsigned char*) malloc(hashable);
        if (!buf)
        {
            fclose(f);
            return -6;
        }
        if (fread(buf, 1, hashable, f) != hashable)
        {
            free(buf);
            fclose(f);
            return -6;
        }
        uint32_t crc = rogue_crc32(buf, hashable);
        if (crc != desc.checksum)
        {
            g_last_tamper_flags |= ROGUE_TAMPER_FLAG_DESCRIPTOR_CRC;
            free(buf);
            fclose(f);
            return -7;
        }
        free(buf);
        fseek(f, sizeof desc, SEEK_SET);
    }
    if (desc.version >= 3)
    {
        for (uint32_t s = 0; s < desc.section_count; s++)
        {
            uint16_t id16 = 0;
            uint32_t size = 0;
            if (fread(&id16, sizeof id16, 1, f) != 1)
            {
                fclose(f);
                return -8;
            }
            if (fread(&size, sizeof size, 1, f) != 1)
            {
                fclose(f);
                return -8;
            }
            uint32_t id = (uint32_t) id16;
            ROGUE_LOG_DEBUG("load_slot section=%u id=%u raw_size=0x%08X pos=%ld", s, id, size,
                            ftell(f));
            int compressed = (desc.version >= 6 && (size & 0x80000000u));
            uint32_t stored_size = size & 0x7FFFFFFFu;
            const RogueSaveComponent* comp = rogue_find_component((int) id);
            long payload_pos = ftell(f);
            if (compressed)
            {
                uint32_t uncompressed_size = 0;
                if (fread(&uncompressed_size, sizeof uncompressed_size, 1, f) != 1)
                {
                    fclose(f);
                    return -11;
                }
                uint32_t comp_bytes = stored_size - sizeof(uint32_t);
                unsigned char* cbuf = (unsigned char*) malloc(comp_bytes);
                if (!cbuf)
                {
                    fclose(f);
                    return -12;
                }
                if (fread(cbuf, 1, comp_bytes, f) != comp_bytes)
                {
                    free(cbuf);
                    fclose(f);
                    return -12;
                }
                unsigned char* ubuf = (unsigned char*) malloc(uncompressed_size);
                if (!ubuf)
                {
                    free(cbuf);
                    fclose(f);
                    return -12;
                }
                /* RLE decode */
                size_t ci = 0;
                size_t ui = 0;
                while (ci < comp_bytes && ui < uncompressed_size)
                {
                    unsigned char b = cbuf[ci++];
                    if (ci >= comp_bytes)
                    {
                        break;
                    }
                    unsigned char run = cbuf[ci++];
                    for (int r = 0; r < run && ui < uncompressed_size; r++)
                    {
                        ubuf[ui++] = b;
                    }
                }
                /* feed via temp file to existing read_fn expecting FILE* */
                if (comp && comp->read_fn)
                {
                    FILE* mf = NULL;
#if defined(_MSC_VER)
                    tmpfile_s(&mf);
#else
                    mf = tmpfile();
#endif
                    if (!mf)
                    {
                        free(cbuf);
                        free(ubuf);
                        fclose(f);
                        return -12;
                    }
                    /* Write only the bytes we actually decoded */
                    fwrite(ubuf, 1, ui, mf);
                    fflush(mf);
                    fseek(mf, 0, SEEK_SET);
                    if (comp->read_fn(mf, ui) != 0)
                    {
                        fclose(mf);
                        free(cbuf);
                        free(ubuf);
                        fclose(f);
                        return -9;
                    }
                    fclose(mf);
                }
                free(cbuf);
                free(ubuf); /* file already positioned just after compressed payload */
            }
            else
            {
                if (comp && comp->read_fn)
                {
                    ROGUE_LOG_DEBUG("load_slot dispatch id=%u size=%u compressed=0", id,
                                    stored_size);
                    if (comp->read_fn(f, stored_size) != 0)
                    {
                        fclose(f);
                        return -9;
                    }
                }
                else
                {
                    ROGUE_LOG_DEBUG("load_slot skip id=%u (no comp) size=%u", id, stored_size);
                }
                fseek(f, payload_pos + stored_size, SEEK_SET);
            }
            if (desc.version >= 7)
            { /* read and verify per-section CRC of uncompressed payload */
                /* We captured uncompressed bytes only for compressed path; for uncompressed, we
                 * must re-read */
                long end_after_payload = ftell(f);
                uint32_t sec_crc = 0;
                if (fread(&sec_crc, sizeof sec_crc, 1, f) != 1)
                {
                    fclose(f);
                    return -10;
                }
                ROGUE_LOG_DEBUG("load_slot section id=%u crc=0x%08X", id, sec_crc);
                /* Reconstruct uncompressed bytes */
                if (compressed)
                { /* Skipping deep verify for compressed section (future enhancement) */
                    (void) payload_pos;
                }
                else
                {
                    long payload_size = stored_size;
                    unsigned char* tmp = (unsigned char*) malloc((size_t) payload_size);
                    if (!tmp)
                    {
                        fclose(f);
                        return -12;
                    }
                    fseek(f, payload_pos, SEEK_SET);
                    if (fread(tmp, 1, (size_t) payload_size, f) != (size_t) payload_size)
                    {
                        free(tmp);
                        fclose(f);
                        return -12;
                    }
                    uint32_t calc = rogue_crc32(tmp, (size_t) payload_size);
                    free(tmp);
                    fseek(f, end_after_payload + 4, SEEK_SET);
                    if (calc != sec_crc)
                    {
                        g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SECTION_CRC;
                        fclose(f);
                        return ROGUE_SAVE_ERR_SECTION_CRC;
                    }
                }
            }
        }
    }
    else
    {
        for (uint32_t s = 0; s < desc.section_count; s++)
        {
            uint32_t id = 0;
            uint32_t size = 0;
            if (fread(&id, sizeof id, 1, f) != 1)
            {
                fclose(f);
                return -8;
            }
            if (fread(&size, sizeof size, 1, f) != 1)
            {
                fclose(f);
                return -8;
            }
            const RogueSaveComponent* comp = rogue_find_component((int) id);
            long payload_pos = ftell(f);
            if (comp && comp->read_fn)
            {
                if (comp->read_fn(f, size) != 0)
                {
                    fclose(f);
                    return -9;
                }
            }
            fseek(f, payload_pos + size, SEEK_SET);
        }
    }
    /* Finalize by syncing loot app view (helpful for headless/tests). */
    rogue_items_sync_app_view();
    fclose(f);
    return 0;
}

/* Recovery: attempt primary slot, on tamper/integrity error fall back to latest autosave ring entry
 * (most recent by timestamp field) */
int rogue_save_manager_load_slot_with_recovery(int slot_index)
{
    g_last_recovery_used = 0;
    int rc = rogue_save_manager_load_slot(slot_index);
    if (rc == 0)
        return 0;
    /* Only recover on integrity/tamper related errors */
    if (rc != ROGUE_SAVE_ERR_SECTION_CRC && rc != ROGUE_SAVE_ERR_SHA256 && rc != -7)
    {
        return rc;
    }
    /* If descriptor checksum mismatch (-7) or SHA errors, flags already set; ensure descriptor CRC
     * flag set on generic -7 */
    if (rc == -7)
        g_last_tamper_flags |= ROGUE_TAMPER_FLAG_DESCRIPTOR_CRC;
    /* Scan autosave ring for any successful load (pick newest timestamp) */
    int best_index = -1;
    uint32_t best_ts = 0;
    for (int i = 0; i < ROGUE_AUTOSAVE_RING; i++)
    {
        const char* path = build_autosave_path(i);
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, path, "rb");
#else
        f = fopen(path, "rb");
#endif
        if (!f)
            continue;
        RogueSaveDescriptor desc;
        if (fread(&desc, sizeof desc, 1, f) != 1)
        {
            fclose(f);
            continue;
        }
        fclose(f);
        if (desc.version != ROGUE_SAVE_FORMAT_VERSION)
            continue;
        if (desc.timestamp_unix > best_ts)
        {
            best_ts = desc.timestamp_unix;
            best_index = i;
        }
    }
    if (best_index < 0)
        return rc; /* no fallback */
    /* Try loading best autosave directly (bypass recovery recursion) */
    const char* path = build_autosave_path(best_index);
    /* Preserve existing tamper flags from failed primary before validation resets them */
    uint32_t prev_flags = g_last_tamper_flags;
    RogueSaveDescriptor d;
    unsigned char* buf = NULL;
    int lrc = load_and_validate(path, &d, &buf);
    if (lrc != 0)
    { /* restore original flags even if recovery attempt fails */
        g_last_tamper_flags |= prev_flags;
        return rc;
    }
    /* Replay section iteration and invoke read_fns (duplicated minimal logic from load_slot for
     * reliability) */
    unsigned char* p = buf;
    size_t total_payload = (size_t) (d.total_size - sizeof d);
    if (d.version >= 3)
    {
        for (uint32_t s = 0; s < d.section_count; s++)
        {
            if ((size_t) (p - buf) + 6 > total_payload)
            {
                free(buf);
                return rc;
            }
            uint16_t id16;
            memcpy(&id16, p, 2);
            uint32_t size;
            memcpy(&size, p + 2, 4);
            p += 6;
            if ((size_t) (p - buf) + size > total_payload)
            {
                free(buf);
                return rc;
            }
            const RogueSaveComponent* comp = rogue_find_component((int) id16);
            if (comp && comp->read_fn)
            { /* feed via temp */
                FILE* tf = NULL;
#if defined(_MSC_VER)
                tmpfile_s(&tf);
#else
                tf = tmpfile();
#endif
                if (!tf)
                {
                    free(buf);
                    return rc;
                }
                fwrite(p, 1, size, tf);
                fflush(tf);
                fseek(tf, 0, SEEK_SET);
                if (comp->read_fn(tf, size) != 0)
                {
                    fclose(tf);
                    free(buf);
                    return rc;
                }
                fclose(tf);
            }
            p += size;
            if (d.version >= 7)
            {
                if ((size_t) (p - buf) + 4 > total_payload)
                {
                    free(buf);
                    return rc;
                }
                p += 4;
            }
        }
    }
    free(buf);
    g_last_tamper_flags |= prev_flags;
    g_last_recovery_used = 1;
    return 1; /* recovered */
}

/* Component implementations live in save_components.c; migrations live in save_migrations.c. */
