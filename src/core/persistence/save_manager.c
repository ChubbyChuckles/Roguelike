#include "save_manager.h"
#include "../../game/buffs.h"
#include "../../world/tilemap.h"
#include "../app/app_state.h"
#include "../loot/loot_instances.h"
#include "../skills/skills.h"
#include "../vendor/vendor.h"
#include "persistence.h"
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

static RogueSaveComponent g_components[ROGUE_SAVE_MAX_COMPONENTS];
static int g_component_count = 0;
static int g_initialized = 0;
static int g_migrations_registered = 0; /* ensure migrations registered once */

/* Migration registry (linear chain for now) */
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
/* Phase 17.5 record-level diff deferred (no additional state) */
typedef struct
{
    int id;
    unsigned char* data;
    uint32_t size;
    uint32_t crc32;
    int valid;
} RogueCachedSection;
static RogueCachedSection g_cached_sections[ROGUE_SAVE_MAX_COMPONENTS];
static uint32_t g_dirty_mask = 0xFFFFFFFFu; /* start dirty */
static unsigned g_last_sections_reused = 0,
                g_last_sections_written = 0; /* Phase 3.10 incremental metrics */
void rogue_save_last_section_reuse(unsigned* reused, unsigned* written)
{
    if (reused)
        *reused = g_last_sections_reused;
    if (written)
        *written = g_last_sections_written;
}
int rogue_save_component_is_dirty(int component_id)
{
    if (component_id <= 0 || component_id >= 32)
        return -1;
    return (g_dirty_mask & (1u << component_id)) ? 1 : 0;
}
/* Autosave scheduling (Phase 6) */
static int g_autosave_interval_ms = 0; /* disabled by default */
static uint32_t g_last_autosave_time = 0;
static uint32_t g_autosave_count = 0;
static int g_last_save_rc = 0;
static uint32_t g_last_save_bytes = 0;
static double g_last_save_ms = 0.0;
static int g_in_save = 0;
static int g_autosave_throttle_ms = 0; /* gap after any save */
int rogue_save_set_incremental(int enabled)
{
    g_incremental_enabled = enabled ? 1 : 0;
    if (!g_incremental_enabled)
    {
        for (int i = 0; i < ROGUE_SAVE_MAX_COMPONENTS; i++)
        {
            free(g_cached_sections[i].data);
            g_cached_sections[i].data = NULL;
            g_cached_sections[i].valid = 0;
        }
        g_dirty_mask = 0xFFFFFFFFu;
    }
    return 0;
}
int rogue_save_mark_component_dirty(int component_id)
{
    if (component_id <= 0 || component_id >= 32)
        return -1;
    g_dirty_mask |= (1u << component_id);
    return 0;
}
int rogue_save_mark_all_dirty(void)
{
    g_dirty_mask = 0xFFFFFFFFu;
    return 0;
}
int rogue_save_set_autosave_interval_ms(int ms)
{
    g_autosave_interval_ms = ms;
    return 0;
}
uint32_t rogue_save_autosave_count(void) { return g_autosave_count; }
int rogue_save_last_save_rc(void) { return g_last_save_rc; }
uint32_t rogue_save_last_save_bytes(void) { return g_last_save_bytes; }
double rogue_save_last_save_ms(void) { return g_last_save_ms; }
int rogue_save_set_autosave_throttle_ms(int ms)
{
    g_autosave_throttle_ms = ms;
    return 0;
}
int rogue_save_status_string(char* buf, size_t cap)
{
    if (!buf || cap == 0)
        return -1;
    int n = snprintf(buf, cap, "save rc=%d bytes=%u ms=%.2f autosaves=%u interval=%d throttle=%d",
                     g_last_save_rc, g_last_save_bytes, g_last_save_ms, g_autosave_count,
                     g_autosave_interval_ms, g_autosave_throttle_ms);
    if (n < 0 || (size_t) n >= cap)
        return -1;
    return 0;
}
uint32_t rogue_save_last_tamper_flags(void) { return g_last_tamper_flags; }
int rogue_save_last_recovery_used(void) { return g_last_recovery_used; }
int rogue_save_set_compression(int enabled, int min_bytes)
{
    g_compress_enabled = enabled ? 1 : 0;
    if (min_bytes > 0)
        g_compress_min_bytes = min_bytes;
    return 0;
}
/* String interning table (Phase 3.5) */
#define ROGUE_SAVE_MAX_STRINGS 256
static char* g_intern_strings[ROGUE_SAVE_MAX_STRINGS];
static int g_intern_count = 0;

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
static int write_varuint(FILE* f, uint32_t v)
{
    while (v >= 0x80)
    {
        unsigned char b = (unsigned char) ((v & 0x7Fu) | 0x80u);
        if (fwrite(&b, 1, 1, f) != 1)
            return -1;
        v >>= 7;
    }
    unsigned char b = (unsigned char) (v & 0x7Fu);
    if (fwrite(&b, 1, 1, f) != 1)
        return -1;
    return 0;
}

/* Minimal SHA256 (public domain style) for overall save footer (Phase 4.1) */
typedef struct
{
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    size_t buf_len;
} RogueSHA256Ctx;
static uint32_t rs_rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static void rogue_sha256_init(RogueSHA256Ctx* c)
{
    static const uint32_t iv[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    memcpy(c->h, iv, sizeof iv);
    c->len = 0;
    c->buf_len = 0;
}
static void rogue_sha256_block(RogueSHA256Ctx* c, const unsigned char* p)
{
    static const uint32_t K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
        0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
        0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
        0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
        0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
        0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
        0xc67178f2u};
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
    {
        w[i] = (uint32_t) p[4 * i] << 24 | (uint32_t) p[4 * i + 1] << 16 |
               (uint32_t) p[4 * i + 2] << 8 | (uint32_t) p[4 * i + 3];
    }
    for (int i = 16; i < 64; i++)
    {
        uint32_t s0 = rs_rotr(w[i - 15], 7) ^ rs_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rs_rotr(w[i - 2], 17) ^ rs_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = c->h[0], b = c->h[1], d = c->h[3], e = c->h[4], f = c->h[5], g = c->h[6],
             h = c->h[7], cc = c->h[2];
    for (int i = 0; i < 64; i++)
    {
        uint32_t S1 = rs_rotr(e, 6) ^ rs_rotr(e, 11) ^ rs_rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rs_rotr(a, 2) ^ rs_rotr(a, 13) ^ rs_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t temp2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = cc;
        cc = b;
        b = a;
        a = temp1 + temp2;
    }
    c->h[0] += a;
    c->h[1] += b;
    c->h[2] += cc;
    c->h[3] += d;
    c->h[4] += e;
    c->h[5] += f;
    c->h[6] += g;
    c->h[7] += h;
}
static void rogue_sha256_update(RogueSHA256Ctx* c, const void* data, size_t len)
{
    const unsigned char* p = data;
    c->len += len;
    while (len > 0)
    {
        size_t space = 64 - c->buf_len;
        size_t take = len < space ? len : space;
        memcpy(c->buf + c->buf_len, p, take);
        c->buf_len += take;
        p += take;
        len -= take;
        if (c->buf_len == 64)
        {
            rogue_sha256_block(c, c->buf);
            c->buf_len = 0;
        }
    }
}
static void rogue_sha256_final(RogueSHA256Ctx* c, unsigned char out[32])
{
    uint64_t bit_len = c->len * 8;
    unsigned char pad = 0x80;
    rogue_sha256_update(c, &pad, 1);
    unsigned char z = 0;
    while (c->buf_len != 56)
    {
        rogue_sha256_update(c, &z, 1);
    }
    unsigned char len_be[8];
    for (int i = 0; i < 8; i++)
    {
        len_be[7 - i] = (unsigned char) (bit_len >> (i * 8));
    }
    rogue_sha256_update(c, len_be, 8);
    for (int i = 0; i < 8; i++)
    {
        out[4 * i] = (unsigned char) (c->h[i] >> 24);
        out[4 * i + 1] = (unsigned char) (c->h[i] >> 16);
        out[4 * i + 2] = (unsigned char) (c->h[i] >> 8);
        out[4 * i + 3] = (unsigned char) (c->h[i]);
    }
}
static unsigned char g_last_sha256[32];
const unsigned char* rogue_save_last_sha256(void) { return g_last_sha256; }
void rogue_save_last_sha256_hex(char out[65])
{
    static const char* hx = "0123456789abcdef";
    for (int i = 0; i < 32; i++)
    {
        out[2 * i] = hx[g_last_sha256[i] >> 4];
        out[2 * i + 1] = hx[g_last_sha256[i] & 0xF];
    }
    out[64] = '\0';
}
int rogue_save_set_signature_provider(const RogueSaveSignatureProvider* prov)
{
    g_sig_provider = prov;
    return 0;
}
const RogueSaveSignatureProvider* rogue_save_get_signature_provider(void) { return g_sig_provider; }

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
    snprintf(json_path, sizeof json_path, "save_slot_%d.json", slot_index);
    remove(json_path);
    return 0;
}

/* Replay hash state (v8). We capture event tuples (frame,action,value) and hash them in order. */
typedef struct
{
    uint32_t frame;
    uint32_t action;
    int32_t value;
} RogueReplayEvent;
#define ROGUE_REPLAY_MAX_EVENTS 4096
static RogueReplayEvent g_replay_events[ROGUE_REPLAY_MAX_EVENTS];
static uint32_t g_replay_event_count = 0;
static unsigned char g_last_replay_hash[32];
void rogue_save_replay_reset(void)
{
    g_replay_event_count = 0;
    memset(g_last_replay_hash, 0, 32);
}
int rogue_save_replay_record_input(uint32_t frame, uint32_t action_code, int32_t value)
{
    if (g_replay_event_count >= ROGUE_REPLAY_MAX_EVENTS)
        return -1;
    g_replay_events[g_replay_event_count].frame = frame;
    g_replay_events[g_replay_event_count].action = action_code;
    g_replay_events[g_replay_event_count].value = value;
    g_replay_event_count++;
    return 0;
}
static void rogue_replay_compute_hash(void)
{
    RogueSHA256Ctx sha;
    rogue_sha256_init(&sha);
    rogue_sha256_update(&sha, g_replay_events, g_replay_event_count * sizeof(RogueReplayEvent));
    rogue_sha256_final(&sha, g_last_replay_hash);
}
const unsigned char* rogue_save_last_replay_hash(void) { return g_last_replay_hash; }
void rogue_save_last_replay_hash_hex(char out[65])
{
    static const char* hx = "0123456789abcdef";
    for (int i = 0; i < 32; i++)
    {
        out[2 * i] = hx[g_last_replay_hash[i] >> 4];
        out[2 * i + 1] = hx[g_last_replay_hash[i] & 0xF];
    }
    out[64] = '\0';
}
uint32_t rogue_save_last_replay_event_count(void) { return g_replay_event_count; }
static int read_varuint(FILE* f, uint32_t* out)
{
    uint32_t result = 0;
    int shift = 0;
    for (int i = 0; i < 5; i++)
    {
        int c = fgetc(f);
        if (c == EOF)
            return -1;
        unsigned char b = (unsigned char) c;
        result |= (uint32_t) (b & 0x7F) << shift;
        if (!(b & 0x80))
        {
            *out = result;
            return 0;
        }
        shift += 7;
    }
    return -1;
}

int rogue_save_last_migration_steps(void) { return g_last_migration_steps; }
int rogue_save_last_migration_failed(void) { return g_last_migration_failed; }
double rogue_save_last_migration_ms(void) { return g_last_migration_ms; }

/* Simple CRC32 (polynomial 0xEDB88320) */
uint32_t rogue_crc32(const void* data, size_t len)
{
    static uint32_t table[256];
    static int have = 0;
    if (!have)
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
            table[i] = c;
        }
        have = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    const unsigned char* p = (const unsigned char*) data;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static const RogueSaveComponent* find_component(int id)
{
    for (int i = 0; i < g_component_count; i++)
        if (g_components[i].id == id)
            return &g_components[i];
    return NULL;
}

void rogue_save_manager_register(const RogueSaveComponent* comp)
{
    if (g_component_count >= ROGUE_SAVE_MAX_COMPONENTS)
    {
        ROGUE_LOG_DEBUG("register skipped (cap reached) id=%d", comp ? comp->id : -1);
        return;
    }
    if (!comp)
    {
        ROGUE_LOG_DEBUG("register NULL component");
        return;
    }
    if (find_component(comp->id))
    {
        ROGUE_LOG_DEBUG("register skipped (already present) id=%d count=%d", comp->id,
                        g_component_count);
        return;
    }
    g_components[g_component_count++] = *comp;
    ROGUE_LOG_DEBUG("registered id=%d name=%s new_count=%d", comp->id,
                    comp->name ? comp->name : "?", g_component_count);
}

static int cmp_comp(const void* a, const void* b)
{
    const RogueSaveComponent* ca = a;
    const RogueSaveComponent* cb = b;
    return (ca->id - cb->id);
}

/* Core migration registration (defined later) */
static void rogue_register_core_migrations(void);
void rogue_save_manager_init(void)
{
    if (!g_initialized)
    {
        g_initialized = 1; /* ensure deterministic order */
    }
    if (!g_migrations_registered)
    {
        rogue_register_core_migrations();
        g_migrations_registered = 1;
    }
}

void rogue_save_register_migration(const RogueSaveMigration* mig)
{
    if (g_migration_count < (int) (sizeof g_migrations / sizeof g_migrations[0]))
        g_migrations[g_migration_count++] = *mig;
}

void rogue_save_manager_reset_for_tests(void)
{
    g_component_count = 0;
    g_initialized = 0;
    g_migration_count = 0;
    g_migrations_registered = 0;
    g_last_migration_steps = 0;
    g_last_migration_failed = 0;
    g_last_migration_ms = 0.0;
}
int rogue_save_set_debug_json(int enabled)
{
    g_debug_json_dump = enabled ? 1 : 0;
    return 0;
}
int rogue_save_intern_string(const char* s)
{
    if (!s)
        return -1;
    for (int i = 0; i < g_intern_count; i++)
        if (strcmp(g_intern_strings[i], s) == 0)
            return i;
    if (g_intern_count >= ROGUE_SAVE_MAX_STRINGS)
        return -1;
    size_t len = strlen(s);
    char* dup = (char*) malloc(len + 1);
    if (!dup)
        return -1;
    memcpy(dup, s, len + 1);
    g_intern_strings[g_intern_count] = dup;
    return g_intern_count++;
}
const char* rogue_save_intern_get(int index)
{
    if (index < 0 || index >= g_intern_count)
        return NULL;
    return g_intern_strings[index];
}
int rogue_save_intern_count(void) { return g_intern_count; }

static char slot_path[128];
static const char* build_slot_path(int slot)
{
    snprintf(slot_path, sizeof slot_path, "save_slot_%d.sav", slot);
    return slot_path;
}

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
static char autosave_path[128];
static const char* build_autosave_path(int logical)
{
    int ring = logical % ROGUE_AUTOSAVE_RING;
    snprintf(autosave_path, sizeof autosave_path, "autosave_%d.sav", ring);
    return autosave_path;
}
static char backup_path_buf[160];
static const char* build_backup_path(int slot, uint32_t ts)
{
    snprintf(backup_path_buf, sizeof backup_path_buf, "save_slot_%d_%u.bak", slot, ts);
    return backup_path_buf;
}

int rogue_save_format_endianness_is_le(void)
{
    uint32_t x = 0x01020304u;
    unsigned char* p = (unsigned char*) &x;
    return p[0] == 0x04;
}

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
#if defined(_MSC_VER)
    remove(final_path);
    rename(tmp_path, final_path);
#else
    rename(tmp_path, final_path);
#endif
    int rc = 0;
    rc = 0;
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
        char json_path[128];
        snprintf(json_path, sizeof json_path, "save_slot_%d.json", slot_index);
        char buf[2048];
        if (rogue_save_export_json(slot_index, buf, sizeof buf) == 0)
        {
            FILE* jf = NULL;
#if defined(_MSC_VER)
            fopen_s(&jf, json_path, "wb");
#else
            jf = fopen(json_path, "wb");
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
int rogue_save_manager_quicksave(void) { return internal_save_to("quicksave.sav"); }
int rogue_save_manager_set_durable(int enabled)
{
    g_durable_writes = enabled ? 1 : 0;
    return 0;
}
int rogue_save_manager_update(uint32_t now_ms, int in_combat)
{
    if (g_autosave_interval_ms <= 0)
        return 0;
    if (in_combat)
        return 0; /* only when idle */
    if (g_last_autosave_time == 0)
        g_last_autosave_time = now_ms; /* init */
    if (now_ms - g_last_autosave_time >= (uint32_t) g_autosave_interval_ms)
    {
        /* throttle: ensure a minimum gap since last save event */
        static uint32_t g_last_any_save_time =
            0; /* local static to track last save timestamp in ms domain */
        if (g_last_any_save_time != 0 && g_autosave_throttle_ms > 0 &&
            now_ms - g_last_any_save_time < (uint32_t) g_autosave_throttle_ms)
        {
            return 0;
        }
        int rc = rogue_save_manager_autosave(g_autosave_count); /* rotate ring */
        if (rc == 0)
        {
            g_autosave_count++;
        }
        g_last_autosave_time = now_ms;
        g_last_any_save_time = now_ms;
        return rc;
    }
    return 0;
}

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
    const RogueSaveComponent* comp = find_component(component_id);
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
            const RogueSaveComponent* comp = find_component((int) id);
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
                while (ci < stored_size && ui < uncompressed_size)
                {
                    unsigned char b = cbuf[ci++];
                    if (ci >= stored_size)
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
                    fwrite(ubuf, 1, uncompressed_size, mf);
                    fflush(mf);
                    fseek(mf, 0, SEEK_SET);
                    if (comp->read_fn(mf, uncompressed_size) != 0)
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
            const RogueSaveComponent* comp = find_component((int) id);
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
            const RogueSaveComponent* comp = find_component((int) id16);
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

/* Basic player + world meta adapters bridging existing ad-hoc text save for Phase1 demonstration */
static int write_player_component(FILE* f)
{
    /* Serialize subset of player progression (minimal Phase 1 binary form) */
    fwrite(&g_app.player.level, sizeof g_app.player.level, 1, f);
    fwrite(&g_app.player.xp, sizeof g_app.player.xp, 1, f);
    fwrite(&g_app.player.xp_to_next, sizeof g_app.player.xp_to_next, 1, f);
    fwrite(&g_app.player.xp_total_accum, sizeof g_app.player.xp_total_accum, 1, f);
    fwrite(&g_app.player.health, sizeof g_app.player.health, 1, f);
    fwrite(&g_app.player.mana, sizeof g_app.player.mana, 1, f);
    fwrite(&g_app.player.action_points, sizeof g_app.player.action_points, 1, f);
    fwrite(&g_app.player.strength, sizeof g_app.player.strength, 1, f);
    fwrite(&g_app.player.dexterity, sizeof g_app.player.dexterity, 1, f);
    fwrite(&g_app.player.vitality, sizeof g_app.player.vitality, 1, f);
    fwrite(&g_app.player.intelligence, sizeof g_app.player.intelligence, 1, f);
    fwrite(&g_app.talent_points, sizeof g_app.talent_points, 1, f);
    /* Analytics counters & run metadata */
    fwrite(&g_app.analytics_damage_dealt_total, sizeof g_app.analytics_damage_dealt_total, 1, f);
    fwrite(&g_app.analytics_gold_earned_total, sizeof g_app.analytics_gold_earned_total, 1, f);
    fwrite(&g_app.permadeath_mode, sizeof g_app.permadeath_mode, 1, f);
    /* Phase 7.4 equipment slots + weapon infusion */
    fwrite(&g_app.player.equipped_weapon_id, sizeof g_app.player.equipped_weapon_id, 1, f);
    fwrite(&g_app.player.weapon_infusion, sizeof g_app.player.weapon_infusion, 1, f);
    /* Phase 7.8 start timestamp (seconds since process epoch) */
    fwrite(&g_app.session_start_seconds, sizeof g_app.session_start_seconds, 1, f);
    /* 13.5 inventory UI sort mode */
    fwrite(&g_app.inventory_sort_mode, sizeof g_app.inventory_sort_mode, 1, f);
    /* Equipment System Phase 1.6: persist expanded equipment slots (count + each) */
    int equip_count = ROGUE_EQUIP__COUNT;
    fwrite(&equip_count, sizeof equip_count, 1, f);
    for (int i = 0; i < equip_count; i++)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) i);
        fwrite(&inst, sizeof inst, 1, f);
    }
    return 0;
}
static int read_player_component(FILE* f, size_t size)
{
    /* Support legacy minimal layout (level,xp,health,talent_points) and extended Phase 7.1/7.7/7.8
     * layout. */
    if (size < sizeof(int) * 4)
        return -1;
    long start = ftell(f);
    int remain = (int) size; /* naive */
    fread(&g_app.player.level, sizeof g_app.player.level, 1, f);
    fread(&g_app.player.xp, sizeof g_app.player.xp, 1, f);
    remain -= sizeof(int) * 2;
    /* Try reading xp_to_next; if insufficient bytes fallback */
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.xp_to_next, sizeof g_app.player.xp_to_next, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.xp_to_next = 0;
    }
    if (remain >= (int) sizeof(unsigned long long))
    {
        fread(&g_app.player.xp_total_accum, sizeof g_app.player.xp_total_accum, 1, f);
        remain -= (int) sizeof(unsigned long long);
    }
    else
    {
        g_app.player.xp_total_accum = 0ULL;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.health, sizeof g_app.player.health, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.health = 0;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.mana, sizeof g_app.player.mana, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.mana = 0;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.action_points, sizeof g_app.player.action_points, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.action_points = 0;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.strength, sizeof g_app.player.strength, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.strength = 5;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.dexterity, sizeof g_app.player.dexterity, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.dexterity = 5;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.vitality, sizeof g_app.player.vitality, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.vitality = 15;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.intelligence, sizeof g_app.player.intelligence, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.intelligence = 5;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.talent_points, sizeof g_app.talent_points, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.talent_points = 0;
    }
    if (remain >= (int) sizeof(unsigned long long))
    {
        fread(&g_app.analytics_damage_dealt_total, sizeof g_app.analytics_damage_dealt_total, 1, f);
        remain -= sizeof(unsigned long long);
    }
    else
    {
        g_app.analytics_damage_dealt_total = 0ULL;
    }
    if (remain >= (int) sizeof(unsigned long long))
    {
        fread(&g_app.analytics_gold_earned_total, sizeof g_app.analytics_gold_earned_total, 1, f);
        remain -= sizeof(unsigned long long);
    }
    else
    {
        g_app.analytics_gold_earned_total = 0ULL;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.permadeath_mode, sizeof g_app.permadeath_mode, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.permadeath_mode = 0;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.equipped_weapon_id, sizeof g_app.player.equipped_weapon_id, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.equipped_weapon_id = -1;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.weapon_infusion, sizeof g_app.player.weapon_infusion, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.player.weapon_infusion = 0;
    }
    if (remain >= (int) sizeof(double))
    {
        fread(&g_app.session_start_seconds, sizeof g_app.session_start_seconds, 1, f);
        remain -= sizeof(double);
    }
    else
    {
        g_app.session_start_seconds = 0.0;
    }
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.inventory_sort_mode, sizeof g_app.inventory_sort_mode, 1, f);
        remain -= sizeof(int);
    }
    else
    {
        g_app.inventory_sort_mode = 0;
    }
    /* Equipment slots (Phase 1.6) backward-compatible: read and attempt immediate apply (inventory
     * component precedes player in registration order). */
    if (remain >= (int) sizeof(int))
    {
        int equip_count = 0;
        fread(&equip_count, sizeof equip_count, 1, f);
        remain -= sizeof(int);
        if (equip_count > 0 && equip_count <= ROGUE_EQUIP__COUNT)
        {
            for (int i = 0; i < equip_count && remain >= (int) sizeof(int); i++)
            {
                int inst = -1;
                fread(&inst, sizeof inst, 1, f);
                remain -= sizeof(int);
                if (inst >= 0)
                    rogue_equip_try((enum RogueEquipSlot) i, inst);
            }
        }
    }
    (void) start;
    return 0;
}

/* ---------------- Additional Phase 1 Components ---------------- */

/* INVENTORY: serialize active item instances (count + each record) */
/* ---- Phase 17.5: Record-level diff inside inventory component ---- */
typedef struct InvRecordSnapshot
{
    int def_index, quantity, rarity, prefix_index, prefix_value, suffix_index, suffix_value,
        durability_cur, durability_max, enchant_level;
} InvRecordSnapshot;
static InvRecordSnapshot* g_inv_prev_records = NULL; /* previous serialized snapshot */
static unsigned g_inv_prev_count = 0;
static unsigned g_inv_diff_reused_last = 0; /* metrics for last save */
static unsigned g_inv_diff_rewritten_last = 0;
void rogue_save_inventory_diff_metrics(unsigned* reused, unsigned* rewritten)
{
    if (reused)
        *reused = g_inv_diff_reused_last;
    if (rewritten)
        *rewritten = g_inv_diff_rewritten_last;
}
static void inv_record_snapshot_update(const InvRecordSnapshot* cur, unsigned count)
{
    InvRecordSnapshot* nr = (InvRecordSnapshot*) realloc(
        g_inv_prev_records, sizeof(InvRecordSnapshot) * (size_t) count);
    if (!nr && count > 0)
        return; /* keep old snapshot on alloc failure */
    if (nr)
    {
        g_inv_prev_records = nr;
        memcpy(g_inv_prev_records, cur, sizeof(InvRecordSnapshot) * (size_t) count);
        g_inv_prev_count = count;
    }
}
static int write_inventory_component(FILE* f)
{
    int count = 0;
    /* Enumerate active instances via public API to avoid dependence on g_app pointer/cap. */
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (it)
            count++;
    }
    ROGUE_LOG_DEBUG("write_inventory_component detected %d active instances (cap=%d)", count,
                    ROGUE_ITEM_INSTANCE_CAP);
    int dbg_seen = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP && dbg_seen < 8; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (it)
        {
            ROGUE_LOG_DEBUG(
                "inv_active idx=%d def=%d qty=%d dur=%d/%d rar=%d pref=(%d,%d) suf=(%d,%d)", i,
                it->def_index, it->quantity, it->durability_cur, it->durability_max, it->rarity,
                it->prefix_index, it->prefix_value, it->suffix_index, it->suffix_value);
            dbg_seen++;
        }
    }
    if (g_active_write_version >= 4)
    {
        if (write_varuint(f, (uint32_t) count) != 0)
            return -1;
    }
    else
    {
        if (fwrite(&count, sizeof count, 1, f) != 1)
            return -1;
    }
    if (count == 0)
    {
        g_inv_prev_count = 0;
        g_inv_diff_reused_last = 0;
        g_inv_diff_rewritten_last = 0;
        return 0;
    }
    /* Build current snapshot */
    InvRecordSnapshot* cur =
        (InvRecordSnapshot*) malloc(sizeof(InvRecordSnapshot) * (size_t) count);
    if (!cur)
        return -1;
    int out = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (!it)
            continue;
        cur[out++] = (InvRecordSnapshot){it->def_index,    it->quantity,       it->rarity,
                                         it->prefix_index, it->prefix_value,   it->suffix_index,
                                         it->suffix_value, it->durability_cur, it->durability_max,
                                         it->enchant_level};
    }
    if (out != count)
    {
        free(cur);
        return -1;
    }
    g_inv_diff_reused_last = 0;
    g_inv_diff_rewritten_last = 0;
    if (g_incremental_enabled && g_inv_prev_count == (unsigned) count)
    {
        for (int i = 0; i < count; i++)
        {
            if (memcmp(&g_inv_prev_records[i], &cur[i], sizeof(InvRecordSnapshot)) == 0)
            {
                g_inv_diff_reused_last++;
            }
            else
            {
                g_inv_diff_rewritten_last++;
            }
        }
    }
    else
    {
        /* All treated as rewritten (size change or incremental disabled) */
        g_inv_diff_rewritten_last = (unsigned) count;
    }
    /* Serialize (currently always writes full records; potential future optimization to copy reused
     * raw bytes) */
    for (int i = 0; i < count; i++)
    {
        InvRecordSnapshot* r = &cur[i];
        fwrite(&r->def_index, sizeof(r->def_index), 1, f);
        fwrite(&r->quantity, sizeof(r->quantity), 1, f);
        fwrite(&r->rarity, sizeof(r->rarity), 1, f);
        fwrite(&r->prefix_index, sizeof(r->prefix_index), 1, f);
        fwrite(&r->prefix_value, sizeof(r->prefix_value), 1, f);
        fwrite(&r->suffix_index, sizeof(r->suffix_index), 1, f);
        fwrite(&r->suffix_value, sizeof(r->suffix_value), 1, f);
        fwrite(&r->durability_cur, sizeof(r->durability_cur), 1, f);
        fwrite(&r->durability_max, sizeof(r->durability_max), 1, f);
        fwrite(&r->enchant_level, sizeof(r->enchant_level), 1, f);
    }
    inv_record_snapshot_update(cur, (unsigned) count);
    free(cur);
    return 0;
}
static int read_inventory_component(FILE* f, size_t size)
{
    /* Layout detection based on remaining payload bytes after the count field:
       - legacy: 7 ints per record (no durability, no enchant)
       - extended1: 9 ints per record (durability_cur, durability_max)
       - extended2: 10 ints per record (adds enchant_level)
    */
    /* Ensure loot runtime is initialized so g_app.item_instances points to the active pool.
       Some unit tests reinitialize or bypass app_init; if the pointer is NULL here, spawn calls
       will still populate the internal static pool, but tests that scan g_app.item_instances will
       see zero. Guard-init to keep both views consistent. */
    if (!g_app.item_instances || g_app.item_instance_cap <= 0)
    {
        rogue_items_init_runtime();
    }
    int count = 0;
    long section_start = ftell(f);
    ROGUE_LOG_DEBUG("read_inventory_component size=%zu", size);
    if (g_active_read_version >= 4)
    {
        uint32_t c = 0;
        if (read_varuint(f, &c) != 0)
            return -1;
        count = (int) c;
    }
    else if (fread(&count, sizeof count, 1, f) != 1)
        return -1;
    if (count < 0)
        return -1;
    long after_count = ftell(f);
    size_t count_bytes = (size_t) (after_count - section_start);
    if (count == 0)
        return 0;
    size_t remaining = (size_t) ((size > count_bytes) ? (size - count_bytes) : 0);
    size_t rec_ints = 0;
    if (remaining >= (size_t) count * (sizeof(int) * 10))
        rec_ints = 10;
    else if (remaining >= (size_t) count * (sizeof(int) * 9))
        rec_ints = 9;
    else if (remaining >= (size_t) count * (sizeof(int) * 7))
        rec_ints = 7;
    else
        return -1; /* malformed */
    ROGUE_LOG_DEBUG("inventory count=%d rec_ints=%zu remaining=%zu", count, rec_ints, remaining);
    for (int i = 0; i < count; i++)
    {
        int def_index = 0, quantity = 0, rarity = 0, pidx = 0, pval = 0, sidx = 0, sval = 0;
        if (fread(&def_index, sizeof def_index, 1, f) != 1)
            return -1;
        if (fread(&quantity, sizeof quantity, 1, f) != 1)
            return -1;
        if (fread(&rarity, sizeof rarity, 1, f) != 1)
            return -1;
        if (fread(&pidx, sizeof pidx, 1, f) != 1)
            return -1;
        if (fread(&pval, sizeof pval, 1, f) != 1)
            return -1;
        if (fread(&sidx, sizeof sidx, 1, f) != 1)
            return -1;
        if (fread(&sval, sizeof sval, 1, f) != 1)
            return -1;
        int durability_cur = 0, durability_max = 0, enchant_level = 0;
        if (rec_ints >= 9)
        {
            if (fread(&durability_cur, sizeof durability_cur, 1, f) != 1)
                return -1;
            if (fread(&durability_max, sizeof durability_max, 1, f) != 1)
                return -1;
        }
        if (rec_ints >= 10)
        {
            if (fread(&enchant_level, sizeof enchant_level, 1, f) != 1)
                return -1;
        }
        int inst = rogue_items_spawn(def_index, quantity, 0.0f, 0.0f);
        if (inst >= 0)
        {
            rogue_item_instance_apply_affixes(inst, rarity, pidx, pval, sidx, sval);
            if (durability_max > 0)
            {
                RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
                if (it)
                {
                    it->durability_max = durability_max;
                    it->durability_cur = durability_cur;
                }
            }
            if (enchant_level > 0)
            {
                RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
                if (it)
                {
                    it->enchant_level = enchant_level;
                }
            }
        }
    }
    ROGUE_LOG_DEBUG("after inventory load active=%d", rogue_items_active_count());
    /* Ensure global app view points at the active pool for tests that scan g_app directly. */
    rogue_items_sync_app_view();
    /* Cross-check against g_app pointer/cap view used by some tests */
    int cap_dbg = g_app.item_instance_cap;
    void* ptr_dbg = (void*) g_app.item_instances;
    int cnt_dbg = 0;
    if (g_app.item_instances && cap_dbg > 0)
    {
        for (int i = 0; i < cap_dbg; i++)
        {
            if (g_app.item_instances[i].active)
                cnt_dbg++;
        }
    }
    ROGUE_LOG_DEBUG("g_app.item_instances=%p cap=%d active_via_g_app=%d", ptr_dbg, cap_dbg,
                    cnt_dbg);
    return 0;
}

/* SKILLS: ranks + cooldown state (id ordered) */
/* PHASE 7.2: Extended skill state (backward-compatible). We always write the extended record, but
 * reader detects legacy minimal form by payload size. */
static int write_skills_component(FILE* f)
{
    if (g_active_write_version >= 4)
    {
        if (write_varuint(f, (uint32_t) g_app.skill_count) != 0)
            return -1;
    }
    else
        fwrite(&g_app.skill_count, sizeof g_app.skill_count, 1, f);
    for (int i = 0; i < g_app.skill_count; i++)
    {
        const RogueSkillState* st = rogue_skill_get_state(i);
        int rank = st ? st->rank : 0;
        double cd = st ? st->cooldown_end_ms : 0.0;
        fwrite(&rank, sizeof rank, 1, f);
        fwrite(&cd, sizeof cd, 1, f);
        /* Extended fields (Phase 7.2) */
        double cast_progress = st ? st->cast_progress_ms : 0.0;
        double channel_end = st ? st->channel_end_ms : 0.0;
        double next_charge_ready = st ? st->next_charge_ready_ms : 0.0;
        int charges_cur = st ? st->charges_cur : 0;
        unsigned char casting_active = st ? st->casting_active : 0;
        unsigned char channel_active = st ? st->channel_active : 0;
        fwrite(&cast_progress, sizeof cast_progress, 1, f);
        fwrite(&channel_end, sizeof channel_end, 1, f);
        fwrite(&next_charge_ready, sizeof next_charge_ready, 1, f);
        fwrite(&charges_cur, sizeof charges_cur, 1, f);
        fwrite(&casting_active, sizeof casting_active, 1, f);
        fwrite(&channel_active, sizeof channel_active, 1, f);
    }
    return 0;
}
static int read_skills_component(FILE* f, size_t size)
{
    long section_start = ftell(f);
    int count = 0;
    size_t count_bytes = 0;
    if (g_active_read_version >= 4)
    {
        uint32_t c = 0;
        if (read_varuint(f, &c) != 0)
            return -1;
        count = (int) c;
    }
    else
    {
        if (fread(&count, sizeof count, 1, f) != 1)
            return -1;
    }
    count_bytes = (size_t) (ftell(f) - section_start);
    if (count < 0 || count > 4096)
        return -1; /* sanity */
    size_t remaining = (size_t) size - count_bytes;
    size_t minimal_rec = sizeof(int) + sizeof(double); /* legacy */
    size_t extended_extra =
        sizeof(double) * 3 + sizeof(int) + 2 * sizeof(unsigned char); /* new appended fields */
    int has_extended = 0;
    if (count > 0)
    {
        if (remaining >= (size_t) count * (minimal_rec + extended_extra))
            has_extended = 1;
    }
    int limit = (count < g_app.skill_count) ? count : g_app.skill_count;
    for (int i = 0; i < count; i++)
    {
        int rank = 0;
        double cd = 0.0;
        if (fread(&rank, sizeof rank, 1, f) != 1)
            return -1;
        if (fread(&cd, sizeof cd, 1, f) != 1)
            return -1;
        double cast_progress = 0.0, channel_end = 0.0, next_charge_ready = 0.0;
        int charges_cur = 0;
        unsigned char casting_active = 0, channel_active = 0;
        if (has_extended)
        {
            if (fread(&cast_progress, sizeof cast_progress, 1, f) != 1)
                return -1;
            if (fread(&channel_end, sizeof channel_end, 1, f) != 1)
                return -1;
            if (fread(&next_charge_ready, sizeof next_charge_ready, 1, f) != 1)
                return -1;
            if (fread(&charges_cur, sizeof charges_cur, 1, f) != 1)
                return -1;
            if (fread(&casting_active, sizeof casting_active, 1, f) != 1)
                return -1;
            if (fread(&channel_active, sizeof channel_active, 1, f) != 1)
                return -1;
        }
        if (i < limit)
        {
            const RogueSkillDef* d = rogue_skill_get_def(i);
            struct RogueSkillState* st = (struct RogueSkillState*) rogue_skill_get_state(i);
            if (d && st)
            {
                if (rank > d->max_rank)
                    rank = d->max_rank;
                st->rank = rank;
                st->cooldown_end_ms = cd;
                if (has_extended)
                {
                    st->cast_progress_ms = cast_progress;
                    st->channel_end_ms = channel_end;
                    st->next_charge_ready_ms = next_charge_ready;
                    st->charges_cur = charges_cur;
                    st->casting_active = casting_active;
                    st->channel_active = channel_active;
                }
            }
        }
        else
        { /* skip unneeded extended already consumed */
        }
    }
    /* If legacy (no extended) and there are extra bytes (unexpected), skip them */
    return 0;
}

/* BUFFS: active buffs list */
/* PHASE 7.3: Buff serialization stores remaining duration (relative) instead of raw struct with
   absolute end time. Backward compatible: detect legacy record size and convert. */
static int write_buffs_component(FILE* f)
{
    int active_count = rogue_buffs_active_count();
    if (g_active_write_version >= 4)
    {
        if (write_varuint(f, (uint32_t) active_count) != 0)
            return -1;
    }
    else
        fwrite(&active_count, sizeof active_count, 1, f);
    for (int i = 0; i < active_count; i++)
    {
        RogueBuff tmp;
        if (!rogue_buffs_get_active(i, &tmp))
            break;
        double now = g_app.game_time_ms;
        double remaining_ms = (tmp.end_ms > now) ? (tmp.end_ms - now) : 0.0;
        int type = tmp.type;
        int magnitude = tmp.magnitude;
        fwrite(&type, sizeof type, 1, f);
        fwrite(&magnitude, sizeof magnitude, 1, f);
        fwrite(&remaining_ms, sizeof remaining_ms, 1, f);
    }
    return 0;
}
static int read_buffs_component(FILE* f, size_t size)
{
    long start = ftell(f);
    int count = 0;
    if (g_active_read_version >= 4)
    {
        uint32_t c = 0;
        if (read_varuint(f, &c) != 0)
            return -1;
        count = (int) c;
    }
    else if (fread(&count, sizeof count, 1, f) != 1)
        return -1;
    if (count < 0 || count > 512)
        return -1;
    size_t count_bytes = (size_t) (ftell(f) - start);
    size_t remaining = size - count_bytes;
    if (count == 0)
        return 0;
    size_t rec_size = remaining / (size_t) count; /* approximate */
    for (int i = 0; i < count; i++)
    {
        if (rec_size >= sizeof(int) * 3 + sizeof(double))
        { /* legacy struct layout (active,int type,double end_ms,int magnitude) -> we read full
             struct */
            struct LegacyBuff
            {
                int active;
                int type;
                double end_ms;
                int magnitude;
            } lb;
            if (fread(&lb, sizeof lb, 1, f) != 1)
                return -1;
            double now = g_app.game_time_ms;
            double remaining_ms = (lb.end_ms > now) ? (lb.end_ms - now) : 0.0;
            rogue_buffs_apply((RogueBuffType) lb.type, lb.magnitude, remaining_ms, now,
                              ROGUE_BUFF_STACK_ADD, 1);
        }
        else
        { /* new compact form: type,int magnitude,double remaining */
            int type = 0;
            int magnitude = 0;
            double remaining_ms = 0.0;
            if (fread(&type, sizeof type, 1, f) != 1)
                return -1;
            if (fread(&magnitude, sizeof magnitude, 1, f) != 1)
                return -1;
            if (fread(&remaining_ms, sizeof remaining_ms, 1, f) != 1)
                return -1;
            double now = g_app.game_time_ms;
            rogue_buffs_apply((RogueBuffType) type, magnitude, remaining_ms, now,
                              ROGUE_BUFF_STACK_ADD, 1);
        }
    }
    return 0;
}

/* VENDOR: seed + restock timers */
static int write_vendor_component(FILE* f)
{
    fwrite(&g_app.vendor_seed, sizeof g_app.vendor_seed, 1, f);
    fwrite(&g_app.vendor_time_accum_ms, sizeof g_app.vendor_time_accum_ms, 1, f);
    fwrite(&g_app.vendor_restock_interval_ms, sizeof g_app.vendor_restock_interval_ms, 1, f);
    /* Phase 7.5: serialize current vendor inventory (def_index, rarity, price) */
    int count = rogue_vendor_item_count();
    if (count < 0)
        count = 0;
    if (count > ROGUE_VENDOR_SLOT_CAP)
        count = ROGUE_VENDOR_SLOT_CAP;
    fwrite(&count, sizeof count, 1, f);
    for (int i = 0; i < count; i++)
    {
        const RogueVendorItem* it = rogue_vendor_get(i);
        if (!it)
        {
            int zero = 0;
            fwrite(&zero, sizeof zero, 1, f);
            fwrite(&zero, sizeof zero, 1, f);
            fwrite(&zero, sizeof zero, 1, f);
        }
        else
        {
            fwrite(&it->def_index, sizeof it->def_index, 1, f);
            fwrite(&it->rarity, sizeof it->rarity, 1, f);
            fwrite(&it->price, sizeof it->price, 1, f);
        }
    }
    return 0;
}
static int read_vendor_component(FILE* f, size_t size)
{
    (void) size;
    fread(&g_app.vendor_seed, sizeof g_app.vendor_seed, 1, f);
    fread(&g_app.vendor_time_accum_ms, sizeof g_app.vendor_time_accum_ms, 1, f);
    fread(&g_app.vendor_restock_interval_ms, sizeof g_app.vendor_restock_interval_ms, 1, f);
    int count = 0;
    if (fread(&count, sizeof count, 1, f) != 1)
        return 0;
    if (count < 0 || count > ROGUE_VENDOR_SLOT_CAP)
        count = 0;
    rogue_vendor_reset();
    for (int i = 0; i < count; i++)
    {
        int def = 0, rar = 0, price = 0;
        if (fread(&def, sizeof def, 1, f) != 1)
            return -1;
        if (fread(&rar, sizeof rar, 1, f) != 1)
            return -1;
        if (fread(&price, sizeof price, 1, f) != 1)
            return -1;
        if (def >= 0)
        {
            int recomputed = rogue_vendor_price_formula(def, rar);
            rogue_vendor_append(def, rar, recomputed);
        }
    }
    return 0;
}

/* STRING INTERN TABLE (component id 7) */
static int write_strings_component(FILE* f)
{
    int count = g_intern_count;
    if (g_active_write_version >= 4)
    {
        if (write_varuint(f, (uint32_t) count) != 0)
            return -1;
    }
    else
        fwrite(&count, sizeof count, 1, f);
    for (int i = 0; i < count; i++)
    {
        const char* s = g_intern_strings[i];
        uint32_t len = (uint32_t) strlen(s);
        if (g_active_write_version >= 4)
        {
            if (write_varuint(f, len) != 0)
                return -1;
        }
        else
            fwrite(&len, sizeof len, 1, f);
        if (fwrite(s, 1, len, f) != len)
            return -1;
    }
    return 0;
}
static int read_strings_component(FILE* f, size_t size)
{
    (void) size;
    int count = 0;
    if (g_active_read_version >= 4)
    {
        uint32_t c = 0;
        if (read_varuint(f, &c) != 0)
            return -1;
        count = (int) c;
    }
    else if (fread(&count, sizeof count, 1, f) != 1)
        return -1;
    if (count > ROGUE_SAVE_MAX_STRINGS)
        return -1;
    for (int i = 0; i < count; i++)
    {
        uint32_t len = 0;
        if (g_active_read_version >= 4)
        {
            if (read_varuint(f, &len) != 0)
                return -1;
        }
        else if (fread(&len, sizeof len, 1, f) != 1)
            return -1;
        if (len > 4096)
            return -1;
        char* buf = (char*) malloc(len + 1);
        if (!buf)
            return -1;
        if (fread(buf, 1, len, f) != len)
        {
            free(buf);
            return -1;
        }
        buf[len] = '\0';
        g_intern_strings[i] = buf;
    }
    g_intern_count = count;
    return 0;
}

/* WORLD META: world seed + generation params subset */
static int write_world_meta_component(FILE* f)
{
    fwrite(&g_app.pending_seed, sizeof g_app.pending_seed, 1, f);
    fwrite(&g_app.gen_water_level, sizeof g_app.gen_water_level, 1, f);
    fwrite(&g_app.gen_cave_thresh, sizeof g_app.gen_cave_thresh, 1, f);
    /* Phase 7.6 extended world gen params */
    fwrite(&g_app.gen_noise_octaves, sizeof g_app.gen_noise_octaves, 1, f);
    fwrite(&g_app.gen_noise_gain, sizeof g_app.gen_noise_gain, 1, f);
    fwrite(&g_app.gen_noise_lacunarity, sizeof g_app.gen_noise_lacunarity, 1, f);
    fwrite(&g_app.gen_river_sources, sizeof g_app.gen_river_sources, 1, f);
    fwrite(&g_app.gen_river_max_length, sizeof g_app.gen_river_max_length, 1, f);
    return 0;
}
static int read_world_meta_component(FILE* f, size_t size)
{
    /* Backward compatible: legacy record had 3 fields; new has 8. Use size to gate reads. */
    size_t remain = size;
    if (remain < sizeof(unsigned int) + sizeof(double) * 2)
        return -1; /* must have at least first three */
    fread(&g_app.pending_seed, sizeof g_app.pending_seed, 1, f);
    remain -= sizeof g_app.pending_seed;
    fread(&g_app.gen_water_level, sizeof g_app.gen_water_level, 1, f);
    remain -= sizeof g_app.gen_water_level;
    fread(&g_app.gen_cave_thresh, sizeof g_app.gen_cave_thresh, 1, f);
    remain -= sizeof g_app.gen_cave_thresh;
    if (remain >= sizeof g_app.gen_noise_octaves)
    {
        fread(&g_app.gen_noise_octaves, sizeof g_app.gen_noise_octaves, 1, f);
        remain -= sizeof g_app.gen_noise_octaves;
    }
    if (remain >= sizeof g_app.gen_noise_gain)
    {
        fread(&g_app.gen_noise_gain, sizeof g_app.gen_noise_gain, 1, f);
        remain -= sizeof g_app.gen_noise_gain;
    }
    if (remain >= sizeof g_app.gen_noise_lacunarity)
    {
        fread(&g_app.gen_noise_lacunarity, sizeof g_app.gen_noise_lacunarity, 1, f);
        remain -= sizeof g_app.gen_noise_lacunarity;
    }
    if (remain >= sizeof g_app.gen_river_sources)
    {
        fread(&g_app.gen_river_sources, sizeof g_app.gen_river_sources, 1, f);
        remain -= sizeof g_app.gen_river_sources;
    }
    if (remain >= sizeof g_app.gen_river_max_length)
    {
        fread(&g_app.gen_river_max_length, sizeof g_app.gen_river_max_length, 1, f);
        remain -= sizeof g_app.gen_river_max_length;
    }
    return 0;
}

/* (Removed duplicate PLAYER_COMP and deferred equipment declarations; single definition earlier) */
static RogueSaveComponent PLAYER_COMP = {ROGUE_SAVE_COMP_PLAYER, write_player_component,
                                         read_player_component, "player"};
static RogueSaveComponent INVENTORY_COMP = {ROGUE_SAVE_COMP_INVENTORY, write_inventory_component,
                                            read_inventory_component, "inventory"};
/* Inventory entries (unified def->quantity model) persistence (Phase 1.6). We currently write a
   full snapshot each save. Record layout (per entry): int def_index, uint64_t quantity, unsigned
   labels. Section payload: varuint entry_count, then that many records. */
static int write_inv_entries_component(FILE* f)
{
    /* Enumerate all entries by iterating over the public API: there is no direct iterator yet; use
     * a dirty enumeration hack by forcing all as dirty. */
    /* Simpler: since module lacks iterator, we snapshot via brute force def_index scan (bounded by
     * ROGUE_INV_MAX_ENTRIES logical ids not exposed). For Phase 1 we assume item def indices are
     * within 0..4095 similar to ROGUE_ITEM_DEF_CAP. */
    /* We will scan a reasonable range (0..4095). Future optimization: expose iterator. */
    uint32_t count = 0;
    for (int i = 0; i < 4096; i++)
    {
        if (rogue_inventory_quantity(i) > 0)
            count++;
    }
    if (write_varuint(f, count) != 0)
        return -1;
    for (int i = 0; i < 4096; i++)
    {
        uint64_t q = rogue_inventory_quantity(i);
        if (q > 0)
        {
            unsigned lbl = rogue_inventory_entry_labels(i);
            int def = i;
            fwrite(&def, sizeof(def), 1, f);
            fwrite(&q, sizeof(q), 1, f);
            fwrite(&lbl, sizeof(lbl), 1, f);
        }
    }
    /* Reset dirty baseline after full snapshot */
    rogue_inventory_entries_dirty_pairs(NULL, NULL, 0);
    return 0;
}
static int read_inv_entries_component(FILE* f, size_t size)
{
    /* Robust read: need at least varuint present */
    uint32_t count = 0;
    if (read_varuint(f, &count) != 0)
        return -1; /* minimal: remaining bytes must fit */
    /* Each record min size = sizeof(int)+sizeof(uint64_t)+sizeof(unsigned) */
    size_t need = (size_t) count * (sizeof(int) + sizeof(uint64_t) + sizeof(unsigned));
    if (size < need)
        return -1; /* malformed */
    rogue_inventory_entries_init();
    for (uint32_t k = 0; k < count; k++)
    {
        int def = 0;
        uint64_t qty = 0;
        unsigned lbl = 0;
        if (fread(&def, sizeof(def), 1, f) != 1)
            return -1;
        if (fread(&qty, sizeof(qty), 1, f) != 1)
            return -1;
        if (fread(&lbl, sizeof(lbl), 1, f) != 1)
            return -1;
        if (def >= 0)
        {
            rogue_inventory_register_pickup(def, qty);
            if (lbl)
                rogue_inventory_entry_set_labels(def, lbl);
        }
    }
    /* Baseline clean */
    rogue_inventory_entries_dirty_pairs(NULL, NULL, 0);
    return 0;
}
static RogueSaveComponent INV_ENTRIES_COMP = {ROGUE_SAVE_COMP_INV_ENTRIES,
                                              write_inv_entries_component,
                                              read_inv_entries_component, "inv_entries"};
/* Inventory tags component (Phase 3.1 favorites/locks + short tags)
   Layout: varuint record_count; for each: int def_index, uint32 flags, uint8 tag_count, for each
   tag: uint8 len, bytes (len)
*/
static int write_inv_tags_component(FILE* f)
{
    uint32_t count = 0;
    for (int i = 0; i < ROGUE_INV_TAG_MAX_DEFS; i++)
    {
        if (rogue_inv_tags_get_flags(i) || rogue_inv_tags_list(i, NULL, 0) > 0)
            count++;
    }
    if (write_varuint(f, count) != 0)
        return -1;
    if (count == 0)
        return 0;
    for (int i = 0; i < ROGUE_INV_TAG_MAX_DEFS; i++)
    {
        unsigned fl = rogue_inv_tags_get_flags(i);
        int tc = rogue_inv_tags_list(i, NULL, 0);
        if (fl == 0 && tc <= 0)
            continue;
        if (tc < 0)
            tc = 0;
        if (tc > ROGUE_INV_TAG_MAX_TAGS_PER_DEF)
            tc = ROGUE_INV_TAG_MAX_TAGS_PER_DEF;
        fwrite(&i, sizeof(int), 1, f);
        fwrite(&fl, sizeof(fl), 1, f);
        unsigned char tcc = (unsigned char) tc;
        fwrite(&tcc, 1, 1, f);
        if (tc > 0)
        {
            const char* tmp[ROGUE_INV_TAG_MAX_TAGS_PER_DEF];
            rogue_inv_tags_list(i, tmp, ROGUE_INV_TAG_MAX_TAGS_PER_DEF);
            for (int k = 0; k < tc; k++)
            {
                size_t len = strlen(tmp[k]);
                if (len > 255)
                    len = 255;
                unsigned char l = (unsigned char) len;
                fwrite(&l, 1, 1, f);
                fwrite(tmp[k], 1, len, f);
            }
        }
    }
    return 0;
}
static int read_inv_tags_component(FILE* f, size_t size)
{
    uint32_t count = 0;
    if (read_varuint(f, &count) != 0)
        return -1;
    size_t consumed = 0;
    rogue_inv_tags_init();
    for (uint32_t r = 0; r < count; r++)
    {
        int def = 0;
        unsigned flags = 0;
        unsigned char tcc = 0;
        if (fread(&def, sizeof(def), 1, f) != 1)
            return -1;
        if (fread(&flags, sizeof(flags), 1, f) != 1)
            return -1;
        if (fread(&tcc, 1, 1, f) != 1)
            return -1;
        consumed += sizeof(def) + sizeof(flags) + 1;
        rogue_inv_tags_set_flags(def, flags);
        for (unsigned char k = 0; k < tcc; k++)
        {
            unsigned char l = 0;
            if (fread(&l, 1, 1, f) != 1)
                return -1;
            consumed += 1;
            if (l > 0)
            {
                char buf[256];
                if (l >= sizeof(buf))
                    l = (unsigned char) (sizeof(buf) - 1);
                if (fread(buf, 1, l, f) != l)
                    return -1;
                buf[l] = '\0';
                rogue_inv_tags_add_tag(def, buf);
                consumed += l;
            }
            if (consumed > size)
                return -1;
        }
    }
    return 0;
}
static RogueSaveComponent INV_TAGS_COMP = {ROGUE_SAVE_COMP_INV_TAGS, write_inv_tags_component,
                                           read_inv_tags_component, "inv_tags"};
/* Inventory auto-tag rules component (Phase 3.3/3.4) */
static int write_inv_tag_rules_component(FILE* f) { return rogue_inv_tag_rules_write(f); }
static int read_inv_tag_rules_component(FILE* f, size_t size)
{
    return rogue_inv_tag_rules_read(f, size);
}
static RogueSaveComponent INV_TAG_RULES_COMP = {ROGUE_SAVE_COMP_INV_TAG_RULES,
                                                write_inv_tag_rules_component,
                                                read_inv_tag_rules_component, "inv_tag_rules"};
/* Inventory saved searches (Phase 4.4) */
static int write_inv_saved_searches_component(FILE* f)
{
    return rogue_inventory_saved_searches_write(f);
}
static int read_inv_saved_searches_component(FILE* f, size_t size)
{
    return rogue_inventory_saved_searches_read(f, size);
}
static RogueSaveComponent INV_SAVED_SEARCHES_COMP = {
    ROGUE_SAVE_COMP_INV_SAVED_SEARCHES, write_inv_saved_searches_component,
    read_inv_saved_searches_component, "inv_saved_searches"};
static RogueSaveComponent SKILLS_COMP = {ROGUE_SAVE_COMP_SKILLS, write_skills_component,
                                         read_skills_component, "skills"};
static RogueSaveComponent BUFFS_COMP = {ROGUE_SAVE_COMP_BUFFS, write_buffs_component,
                                        read_buffs_component, "buffs"};
static RogueSaveComponent VENDOR_COMP = {ROGUE_SAVE_COMP_VENDOR, write_vendor_component,
                                         read_vendor_component, "vendor"};
static RogueSaveComponent STRINGS_COMP = {ROGUE_SAVE_COMP_STRINGS, write_strings_component,
                                          read_strings_component, "strings"};
static RogueSaveComponent WORLD_META_COMP = {ROGUE_SAVE_COMP_WORLD_META, write_world_meta_component,
                                             read_world_meta_component, "world_meta"};
/* Replay component (v8) serializes event count + raw events + precomputed SHA256 of events (for
 * forward compatibility). */
static int write_replay_component(FILE* f)
{
    /* Compute and serialize replay hash (events may be empty). */
    rogue_replay_compute_hash();
    uint32_t count = g_replay_event_count;
    fwrite(&count, sizeof count, 1, f);
    if (count)
    {
        fwrite(g_replay_events, sizeof(RogueReplayEvent), count, f);
    }
    fwrite(g_last_replay_hash, 1, 32, f);
    return 0;
}
static int read_replay_component(FILE* f, size_t size)
{
    if (size < sizeof(uint32_t) + 32)
        return -1;
    uint32_t count = 0;
    fread(&count, sizeof count, 1, f);
    size_t need = (size_t) count * sizeof(RogueReplayEvent) + 32;
    if (size < sizeof(uint32_t) + need)
        return -1;
    if (count > ROGUE_REPLAY_MAX_EVENTS)
        return -1;
    if (count)
    {
        fread(g_replay_events, sizeof(RogueReplayEvent), count, f);
    }
    else
    { /* no events */
    }
    fread(g_last_replay_hash, 1, 32, f);
    g_replay_event_count = count; /* Recompute to verify (optional) */
    RogueSHA256Ctx sha;
    rogue_sha256_init(&sha);
    rogue_sha256_update(&sha, g_replay_events, count * sizeof(RogueReplayEvent));
    unsigned char chk[32];
    rogue_sha256_final(&sha, chk);
    if (memcmp(chk, g_last_replay_hash, 32) != 0)
    {
        return -1;
    }
    return 0;
}
static RogueSaveComponent REPLAY_COMP = {ROGUE_SAVE_COMP_REPLAY, write_replay_component,
                                         read_replay_component, "replay"};

void rogue_register_core_save_components(void)
{
    /* Register inventory-related components first so they load before player equipment indices. */
    rogue_save_manager_register(&WORLD_META_COMP);
    rogue_save_manager_register(&INVENTORY_COMP);
    rogue_save_manager_register(&INV_ENTRIES_COMP);        /* Phase 1.6 unified entries */
    rogue_save_manager_register(&INV_TAGS_COMP);           /* Phase 3 metadata */
    rogue_save_manager_register(&INV_TAG_RULES_COMP);      /* Phase 3.3/3.4 auto-tag rules */
    rogue_save_manager_register(&INV_SAVED_SEARCHES_COMP); /* Phase 4.4 saved searches */
    rogue_save_manager_register(&PLAYER_COMP);             /* after inventory to apply equips */
    rogue_save_manager_register(&SKILLS_COMP);
    rogue_save_manager_register(&BUFFS_COMP);
    rogue_save_manager_register(&VENDOR_COMP);
    rogue_save_manager_register(&STRINGS_COMP);
    /* v8+ replay component always registered when available */
#if ROGUE_SAVE_FORMAT_VERSION >= 8
    rogue_save_manager_register(&REPLAY_COMP);
#endif
}

/* (Deferred post-apply system removed for simplicity in Phase 1.6) */

/* Migration definitions */
static int migrate_v2_to_v3(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v3_to_v4(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v4_to_v5(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v5_to_v6(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v6_to_v7(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v7_to_v8(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v8_to_v9(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static RogueSaveMigration MIG_V2_TO_V3 = {2u, 3u, migrate_v2_to_v3, "v2_to_v3_tlv_header"};
static RogueSaveMigration MIG_V3_TO_V4 = {3u, 4u, migrate_v3_to_v4, "v3_to_v4_varint_counts"};
static RogueSaveMigration MIG_V4_TO_V5 = {4u, 5u, migrate_v4_to_v5, "v4_to_v5_string_intern"};
static RogueSaveMigration MIG_V5_TO_V6 = {5u, 6u, migrate_v5_to_v6, "v5_to_v6_section_compress"};
static RogueSaveMigration MIG_V6_TO_V7 = {6u, 7u, migrate_v6_to_v7, "v6_to_v7_integrity"};
static RogueSaveMigration MIG_V7_TO_V8 = {7u, 8u, migrate_v7_to_v8, "v7_to_v8_replay_hash"};
static RogueSaveMigration MIG_V8_TO_V9 = {8u, 9u, migrate_v8_to_v9, "v8_to_v9_signature_opt"};
static void rogue_register_core_migrations(void)
{
    if (!g_migrations_registered)
    {
        rogue_save_register_migration(&MIG_V2_TO_V3);
        rogue_save_register_migration(&MIG_V3_TO_V4);
        rogue_save_register_migration(&MIG_V4_TO_V5);
        rogue_save_register_migration(&MIG_V5_TO_V6);
        rogue_save_register_migration(&MIG_V6_TO_V7);
        rogue_save_register_migration(&MIG_V7_TO_V8);
        rogue_save_register_migration(&MIG_V8_TO_V9);
    }
}
