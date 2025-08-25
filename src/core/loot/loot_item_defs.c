#include "loot_item_defs.h"
#include "loot_affixes.h" /* ensure affixes present for tests that roll immediately */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ROGUE_ITEM_DEF_CAP
#define ROGUE_ITEM_DEF_CAP 512
#endif

static RogueItemDef g_item_defs[ROGUE_ITEM_DEF_CAP];
static int g_item_def_count = 0;
/* Phase 17.4: open-address hash index (power-of-two sized) for cache-friendly id->index lookup */
static int g_hash_cap = 0;       /* number of slots */
static int* g_hash_slots = NULL; /* -1 empty, -2 tombstone (unused) or index into g_item_defs */

static unsigned int hash_str(const char* s)
{
    unsigned int h = 2166136261u;
    while (*s)
    {
        unsigned char c = (unsigned char) *s++;
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

int rogue_item_defs_build_index(void)
{
    if (g_hash_slots)
    {
        free(g_hash_slots);
        g_hash_slots = NULL;
        g_hash_cap = 0;
    }
    if (g_item_def_count == 0)
        return 0;
    int target = 1;
    while (target < g_item_def_count * 2)
        target <<= 1;
    g_hash_cap = target;
    g_hash_slots = (int*) malloc(sizeof(int) * g_hash_cap);
    if (!g_hash_slots)
    {
        g_hash_cap = 0;
        return -1;
    }
    for (int i = 0; i < g_hash_cap; i++)
        g_hash_slots[i] = -1;
    for (int i = 0; i < g_item_def_count; i++)
    {
        unsigned int h = hash_str(g_item_defs[i].id);
        int mask = g_hash_cap - 1;
        int pos = (int) (h & mask);
        int probes = 0;
        while (g_hash_slots[pos] >= 0)
        {
            pos = (pos + 1) & mask;
            if (++probes > g_hash_cap)
            {
                return -2;
            }
        }
        g_hash_slots[pos] = i;
    }
    return 0;
}

int rogue_item_def_index_fast(const char* id)
{
    if (!id)
        return -1;
    if (!g_hash_slots || g_hash_cap == 0)
        return rogue_item_def_index(id);
    unsigned int h = hash_str(id);
    int mask = g_hash_cap - 1;
    int pos = (int) (h & mask);
    int probes = 0;
    while (probes < g_hash_cap)
    {
        int v = g_hash_slots[pos];
        if (v == -1)
            return -1;
        if (v >= 0)
        {
            if (strcmp(g_item_defs[v].id, id) == 0)
                return v;
        }
        pos = (pos + 1) & mask;
        probes++;
    }
    return -1;
}

void rogue_item_defs_reset(void) { g_item_def_count = 0; }
int rogue_item_defs_count(void) { return g_item_def_count; }

static char* next_field(char** cur)
{
    if (!*cur)
        return NULL;
    char* start = *cur;
    char* p = *cur;
    while (*p && *p != ',')
    {
        p++;
    }
    if (*p == ',')
    {
        *p = '\0';
        p++;
    }
    else if (*p == '\0')
    { /* end */
    }
    *cur = (*p) ? p : NULL;
    return start;
}

static int parse_line(char* line, RogueItemDef* out)
{
    for (char* p = line; *p; ++p)
    {
        if (*p == '\r' || *p == '\n')
        {
            *p = '\0';
            break;
        }
    }
    if (line[0] == '#' || line[0] == '\0')
        return 0;
    char* cursor = line;
    char* fields[32];
    int nf = 0;
    while (cursor && nf < 32)
    {
        fields[nf++] = next_field(&cursor);
    }
    if (nf < 14)
        return -1; /* rarity (14/15th) optional, flags optional (16th), implicit/set fields optional
                      (post-16) */
    RogueItemDef d;
    memset(&d, 0, sizeof d);
#if defined(_MSC_VER)
    strncpy_s(d.id, sizeof d.id, fields[0], _TRUNCATE);
    strncpy_s(d.name, sizeof d.name, fields[1], _TRUNCATE);
    strncpy_s(d.sprite_sheet, sizeof d.sprite_sheet, fields[9], _TRUNCATE);
#else
    strncpy(d.id, fields[0], sizeof d.id - 1);
    strncpy(d.name, fields[1], sizeof d.name - 1);
    strncpy(d.sprite_sheet, fields[9], sizeof d.sprite_sheet - 1);
#endif
    d.category = (RogueItemCategory) strtol(fields[2], NULL, 10);
    d.level_req = (int) strtol(fields[3], NULL, 10);
    d.stack_max = (int) strtol(fields[4], NULL, 10);
    if (d.stack_max <= 0)
        d.stack_max = 1;
    d.base_value = (int) strtol(fields[5], NULL, 10);
    d.base_damage_min = (int) strtol(fields[6], NULL, 10);
    d.base_damage_max = (int) strtol(fields[7], NULL, 10);
    d.base_armor = (int) strtol(fields[8], NULL, 10);
    d.sprite_tx = (int) strtol(fields[10], NULL, 10);
    d.sprite_ty = (int) strtol(fields[11], NULL, 10);
    d.sprite_tw = (int) strtol(fields[12], NULL, 10);
    if (d.sprite_tw <= 0)
        d.sprite_tw = 1;
    d.sprite_th = (int) strtol(fields[13], NULL, 10);
    if (d.sprite_th <= 0)
        d.sprite_th = 1;
    d.rarity = 0;
    if (nf >= 15)
    {
        d.rarity = (int) strtol(fields[14], NULL, 10);
        if (d.rarity < 0)
            d.rarity = 0;
    }
    d.flags = 0;
    if (nf >= 16)
    {
        d.flags = (int) strtol(fields[15], NULL, 10);
    }
    /* Implicit stat columns (Phase 4.1) start at index 16 if present:
        16: implicit_strength
        17: implicit_dexterity
        18: implicit_vitality
        19: implicit_intelligence
        20: implicit_armor_flat
        21: implicit_resist_physical
        22: implicit_resist_fire
        23: implicit_resist_cold
        24: implicit_resist_lightning
        25: implicit_resist_poison
        26: implicit_resist_status
        27: set_id (Phase 4.3)
        28: socket_min (Phase 5.1 optional)
        29: socket_max (Phase 5.1 optional) */
    if (nf > 16)
    {
        int idx = 16;
        if (idx < nf)
            d.implicit_strength = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_dexterity = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_vitality = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_intelligence = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_armor_flat = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_resist_physical = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_resist_fire = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_resist_cold = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_resist_lightning = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_resist_poison = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.implicit_resist_status = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.set_id = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.socket_min = (int) strtol(fields[idx++], NULL, 10);
        if (idx < nf)
            d.socket_max = (int) strtol(fields[idx++], NULL, 10);
        if (d.socket_min < 0)
            d.socket_min = 0;
        if (d.socket_max < d.socket_min)
            d.socket_max = d.socket_min;
        if (d.socket_max > 6)
            d.socket_max = 6;
    }
    *out = d;
    return 1;
}

int rogue_item_defs_validate_file(const char* path, int* out_lines, int cap)
{
    FILE* f = NULL;
    int collected = 0;
    int malformed = 0;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return -1;
    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof line, f))
    {
        lineno++;
        char work[512];
#if defined(_MSC_VER)
        strncpy_s(work, sizeof work, line, _TRUNCATE);
#else
        strncpy(work, line, sizeof work - 1);
        work[sizeof work - 1] = '\0';
#endif
        RogueItemDef tmp;
        int r = parse_line(work, &tmp);
        if (r < 0)
        {
            malformed++;
            if (out_lines && collected < cap)
            {
                out_lines[collected++] = lineno;
            }
        }
    }
    fclose(f);
    return malformed;
}

int rogue_item_defs_load_from_cfg(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0)
        f = NULL;
#else
    f = fopen(path, "rb");
#endif
    /* If the provided path cannot be opened, try common relative fallbacks so tests running
       from build subfolders can still locate repo-root assets. */
    if (!f)
    {
        const char* prefixes[] = {"../", "../../", "../../../"};
        char candidate[512];
        for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
        {
            int n = snprintf(candidate, sizeof candidate, "%s%s", prefixes[i], path);
            if (n <= 0 || n >= (int) sizeof candidate)
                continue;
#if defined(_MSC_VER)
            if (fopen_s(&f, candidate, "rb") == 0 && f)
                break;
#else
            f = fopen(candidate, "rb");
            if (f)
                break;
#endif
        }
        if (!f)
            return -1;
    }
    char line[512];
    int added = 0;
    int lineno = 0;
    while (fgets(line, sizeof line, f))
    {
        lineno++;
        char work[512];
#if defined(_MSC_VER)
        strncpy_s(work, sizeof work, line, _TRUNCATE);
#else
        strncpy(work, line, sizeof work - 1);
        work[sizeof work - 1] = '\0';
#endif
        RogueItemDef d;
        int r = parse_line(work, &d);
        if (r < 0)
        { /* malformed */
            fprintf(stderr, "item_defs: malformed line %d in %s\n", lineno, path);
            continue;
        }
        if (r == 0)
            continue; /* skip */
        if (g_item_def_count >= ROGUE_ITEM_DEF_CAP)
        {
            fprintf(stderr, "item_defs: cap reached (%d)\n", ROGUE_ITEM_DEF_CAP);
            break;
        }
        g_item_defs[g_item_def_count++] = d;
        added++;
    }
    fclose(f);
    /* Rebuild hash index after each file load to keep fast path current (cost acceptable for small
     * counts) */
    rogue_item_defs_build_index();
    /* Tests often roll affixes right after loading item defs. If affixes aren't loaded yet,
       opportunistically load the default affix set using the same relative fallback scheme. */
    if (added > 0 && rogue_affix_count() == 0)
    {
        const char* affix_path = "assets/affixes.cfg";
        int affix_added = rogue_affixes_load_from_cfg(affix_path);
        if (affix_added <= 0)
        {
            const char* prefixes[] = {"../", "../../", "../../../"};
            char candidate[512];
            for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
            {
                int n = snprintf(candidate, sizeof candidate, "%s%s", prefixes[i], affix_path);
                if (n <= 0 || n >= (int) sizeof candidate)
                    continue;
                affix_added = rogue_affixes_load_from_cfg(candidate);
                if (affix_added > 0)
                    break;
            }
        }
        (void) affix_added; /* best-effort; proceed even if missing */
    }
    return added;
}

/* ---- Phase 16.1: JSON Import / Export ---- */
static const char* skip_ws(const char* s)
{
    while (*s && (unsigned char) *s <= 32)
        ++s;
    return s;
}
static const char* parse_string(const char* s, char* out, int cap)
{
    s = skip_ws(s);
    if (*s != '"')
        return NULL;
    ++s;
    int i = 0;
    while (*s && *s != '"')
    {
        if (*s == '\\' && s[1])
            s++;
        if (i + 1 < cap)
            out[i++] = *s;
        ++s;
    }
    if (*s != '"')
        return NULL;
    out[i] = '\0';
    return s + 1;
}
static const char* parse_number(const char* s, int* out)
{
    s = skip_ws(s);
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s)
        return NULL;
    *out = (int) v;
    return end;
}

int rogue_item_defs_load_from_json(const char* path)
{
    if (!path)
        return -1;
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0)
        f = NULL;
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return -1;
    }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        return -1;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    buf[rd] = '\0';
    fclose(f);
    const char* s = skip_ws(buf);
    if (*s != '[')
    {
        free(buf);
        return -1;
    }
    ++s;
    int added = 0;
    char key[64];
    char sval[256];
    while (1)
    {
        s = skip_ws(s);
        if (*s == ']')
        {
            ++s;
            break;
        }
        if (*s != '{')
        {
            break;
        }
        ++s;
        RogueItemDef d;
        memset(&d, 0, sizeof d);
        d.stack_max = 1;
        int have_id = 0, have_name = 0;
        int done_obj = 0;
        while (!done_obj)
        {
            s = skip_ws(s);
            if (*s == '}')
            {
                ++s;
                break;
            }
            const char* ns = parse_string(s, key, sizeof key);
            if (!ns)
            {
                done_obj = 1;
                break;
            }
            s = skip_ws(ns);
            if (*s != ':')
            {
                done_obj = 1;
                break;
            }
            ++s;
            s = skip_ws(s);
            if (*s == '"')
            {
                const char* vs = parse_string(s, sval, sizeof sval);
                if (!vs)
                {
                    done_obj = 1;
                    break;
                }
                s = skip_ws(vs);
                if (strcmp(key, "id") == 0)
                {
#if defined(_MSC_VER)
                    strncpy_s(d.id, sizeof d.id, sval, _TRUNCATE);
#else
                    strncpy(d.id, sval, sizeof d.id - 1);
#endif
                    have_id = 1;
                }
                else if (strcmp(key, "name") == 0)
                {
#if defined(_MSC_VER)
                    strncpy_s(d.name, sizeof d.name, sval, _TRUNCATE);
#else
                    strncpy(d.name, sval, sizeof d.name - 1);
#endif
                    have_name = 1;
                }
                else if (strcmp(key, "sprite_sheet") == 0)
                {
#if defined(_MSC_VER)
                    strncpy_s(d.sprite_sheet, sizeof d.sprite_sheet, sval, _TRUNCATE);
#else
                    strncpy(d.sprite_sheet, sval, sizeof d.sprite_sheet - 1);
#endif
                }
            }
            else if ((*s >= '0' && *s <= '9') || *s == '-')
            {
                int num;
                const char* vs = parse_number(s, &num);
                if (!vs)
                {
                    done_obj = 1;
                    break;
                }
                s = skip_ws(vs);
                if (strcmp(key, "category") == 0)
                    d.category = (RogueItemCategory) num;
                else if (strcmp(key, "level_req") == 0)
                    d.level_req = num;
                else if (strcmp(key, "stack_max") == 0)
                    d.stack_max = num > 0 ? num : 1;
                else if (strcmp(key, "base_value") == 0)
                    d.base_value = num;
                else if (strcmp(key, "base_damage_min") == 0)
                    d.base_damage_min = num;
                else if (strcmp(key, "base_damage_max") == 0)
                    d.base_damage_max = num;
                else if (strcmp(key, "base_armor") == 0)
                    d.base_armor = num;
                else if (strcmp(key, "sprite_tx") == 0)
                    d.sprite_tx = num;
                else if (strcmp(key, "sprite_ty") == 0)
                    d.sprite_ty = num;
                else if (strcmp(key, "sprite_tw") == 0)
                    d.sprite_tw = num > 0 ? num : 1;
                else if (strcmp(key, "sprite_th") == 0)
                    d.sprite_th = num > 0 ? num : 1;
                else if (strcmp(key, "rarity") == 0)
                    d.rarity = num < 0 ? 0 : num;
                else if (strcmp(key, "flags") == 0)
                    d.flags = num;
                else if (strcmp(key, "implicit_strength") == 0)
                    d.implicit_strength = num;
                else if (strcmp(key, "implicit_dexterity") == 0)
                    d.implicit_dexterity = num;
                else if (strcmp(key, "implicit_vitality") == 0)
                    d.implicit_vitality = num;
                else if (strcmp(key, "implicit_intelligence") == 0)
                    d.implicit_intelligence = num;
                else if (strcmp(key, "implicit_armor_flat") == 0)
                    d.implicit_armor_flat = num;
                else if (strcmp(key, "implicit_resist_physical") == 0)
                    d.implicit_resist_physical = num;
                else if (strcmp(key, "implicit_resist_fire") == 0)
                    d.implicit_resist_fire = num;
                else if (strcmp(key, "implicit_resist_cold") == 0)
                    d.implicit_resist_cold = num;
                else if (strcmp(key, "implicit_resist_lightning") == 0)
                    d.implicit_resist_lightning = num;
                else if (strcmp(key, "implicit_resist_poison") == 0)
                    d.implicit_resist_poison = num;
                else if (strcmp(key, "implicit_resist_status") == 0)
                    d.implicit_resist_status = num;
                else if (strcmp(key, "set_id") == 0)
                    d.set_id = num;
                else if (strcmp(key, "socket_min") == 0)
                    d.socket_min = num;
                else if (strcmp(key, "socket_max") == 0)
                    d.socket_max = num;
            }
            s = skip_ws(s);
            if (*s == ',')
            {
                ++s;
                continue;
            }
        }
        if (have_id && have_name && g_item_def_count < ROGUE_ITEM_DEF_CAP)
        {
            if (d.socket_min < 0)
                d.socket_min = 0;
            if (d.socket_max < d.socket_min)
                d.socket_max = d.socket_min;
            if (d.socket_max > 6)
                d.socket_max = 6;
            g_item_defs[g_item_def_count++] = d;
            added++;
        }
        s = skip_ws(s);
        if (*s == ',')
        {
            ++s;
            continue;
        }
    }
    free(buf);
    rogue_item_defs_build_index();
    return added;
}

int rogue_item_defs_export_json(char* buf, int cap)
{
    if (!buf || cap <= 2)
        return -1;
    int off = 0;
    buf[off++] = '[';
    for (int i = 0; i < g_item_def_count; i++)
    {
        if (off + 2 >= cap)
            return -1;
        if (i > 0)
            buf[off++] = ',';
        if (off + 1 >= cap)
            return -1;
        buf[off++] = '{';
        const RogueItemDef* d = &g_item_defs[i];
        char tmp[256];
#define APPEND_FMT(fmt, ...)                                                                       \
    do                                                                                             \
    {                                                                                              \
        int n = snprintf(tmp, sizeof tmp, fmt, __VA_ARGS__);                                       \
        if (n < 0)                                                                                 \
            return -1;                                                                             \
        if (off + n >= cap)                                                                        \
            return -1;                                                                             \
        memcpy(buf + off, tmp, (size_t) n);                                                        \
        off += n;                                                                                  \
    } while (0)
        APPEND_FMT("\"id\":\"%s\",", d->id);
        APPEND_FMT("\"name\":\"%s\",", d->name);
        APPEND_FMT("\"category\":%d,", d->category);
        APPEND_FMT("\"level_req\":%d,", d->level_req);
        APPEND_FMT("\"stack_max\":%d,", d->stack_max);
        APPEND_FMT("\"base_value\":%d,", d->base_value);
        APPEND_FMT("\"base_damage_min\":%d,", d->base_damage_min);
        APPEND_FMT("\"base_damage_max\":%d,", d->base_damage_max);
        APPEND_FMT("\"base_armor\":%d,", d->base_armor);
        APPEND_FMT("\"sprite_sheet\":\"%s\",", d->sprite_sheet);
        APPEND_FMT("\"sprite_tx\":%d,\"sprite_ty\":%d,\"sprite_tw\":%d,\"sprite_th\":%d,",
                   d->sprite_tx, d->sprite_ty, d->sprite_tw, d->sprite_th);
        APPEND_FMT("\"rarity\":%d,\"flags\":%d,", d->rarity, d->flags);
        APPEND_FMT("\"implicit_strength\":%d,\"implicit_dexterity\":%d,\"implicit_vitality\":%d,"
                   "\"implicit_intelligence\":%d,",
                   d->implicit_strength, d->implicit_dexterity, d->implicit_vitality,
                   d->implicit_intelligence);
        APPEND_FMT("\"implicit_armor_flat\":%d,\"implicit_resist_physical\":%d,\"implicit_resist_"
                   "fire\":%d,\"implicit_resist_cold\":%d,\"implicit_resist_lightning\":%d,"
                   "\"implicit_resist_poison\":%d,\"implicit_resist_status\":%d,",
                   d->implicit_armor_flat, d->implicit_resist_physical, d->implicit_resist_fire,
                   d->implicit_resist_cold, d->implicit_resist_lightning, d->implicit_resist_poison,
                   d->implicit_resist_status);
        APPEND_FMT("\"set_id\":%d,\"socket_min\":%d,\"socket_max\":%d", d->set_id, d->socket_min,
                   d->socket_max);
        if (off + 2 >= cap)
            return -1;
        buf[off++] = '}';
    }
    if (off + 2 >= cap)
        return -1;
    buf[off++] = ']';
    buf[off] = '\0';
    return off;
}

const RogueItemDef* rogue_item_def_by_id(const char* id)
{
    if (!id)
        return NULL;
    for (int i = 0; i < g_item_def_count; i++)
    {
        if (strcmp(g_item_defs[i].id, id) == 0)
            return &g_item_defs[i];
    }
    return NULL;
}
int rogue_item_def_index(const char* id)
{
    if (!id)
        return -1;
    for (int i = 0; i < g_item_def_count; i++)
    {
        if (strcmp(g_item_defs[i].id, id) == 0)
            return i;
    }
    return -1;
}
const RogueItemDef* rogue_item_def_at(int index)
{
    if (index < 0 || index >= g_item_def_count)
        return NULL;
    return &g_item_defs[index];
}

/* Internal helper: load item defs from a concrete directory path. Returns number added. */
static int rogue_item_defs_load_from_dir_internal(const char* dir_path)
{
    /* Static list of expected category files; user can remove or add (silently skipped if missing).
     */
    const char* files[] = {"swords.cfg", "potions.cfg",   "armor.cfg",
                           "gems.cfg",   "materials.cfg", "misc.cfg"};
    char path[512];
    int total = 0;
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
    {
        int n = snprintf(path, sizeof path, "%s/%s", dir_path, files[i]);
        if (n <= 0 || n >= (int) sizeof path)
            continue; /* skip overly long */
        int before = g_item_def_count;
        int added = rogue_item_defs_load_from_cfg(path);
        if (added > 0)
            total += (g_item_def_count - before); /* ensure we count only actually added lines */
    }
    return total;
}

int rogue_item_defs_load_directory(const char* dir_path)
{
    if (!dir_path)
        return -1;

    /* Try the provided directory first; if nothing loads, try common relative fallbacks.
       This mirrors how tests invoke the loader from different working directories. */
    const char* candidates[] = {/* as provided */ dir_path,
                                /* fallbacks */ "../", "../../", "../../../"};

    int total = 0;
    char candidate[512];

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        if (i == 0)
        {
            /* Use dir_path as-is */
            total = rogue_item_defs_load_from_dir_internal(dir_path);
        }
        else
        {
            int n = snprintf(candidate, sizeof candidate, "%s%s", candidates[i], dir_path);
            if (n <= 0 || n >= (int) sizeof candidate)
                continue;
            total = rogue_item_defs_load_from_dir_internal(candidate);
        }
        if (total > 0)
            break; /* stop at first successful load */
    }

    rogue_item_defs_build_index();
    return total;
}
