#include "vendor_registry.h"
#include "../../util/path_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#define ROGUE_LOCAL_DISABLE_WARNINGS __pragma(warning(push)) __pragma(warning(disable : 4996))
#define ROGUE_LOCAL_RESTORE_WARNINGS __pragma(warning(pop))
#else
#define ROGUE_LOCAL_DISABLE_WARNINGS
#define ROGUE_LOCAL_RESTORE_WARNINGS
#endif

static RogueVendorDef g_vendors[ROGUE_MAX_VENDOR_DEFS];
static int g_vendor_count = 0;
static RoguePricePolicy g_policies[ROGUE_MAX_PRICE_POLICIES];
static int g_policy_count = 0;
static RogueRepTier g_rep_tiers[ROGUE_MAX_REP_TIERS];
static int g_rep_tier_count = 0;
static RogueNegotiationRule g_negotiation_rules[ROGUE_MAX_NEGOTIATION_RULES];
static int g_negotiation_rule_count = 0;

/* ---------------- JSON Helpers (lightweight, pattern-based) ---------------- */
static int read_entire_file(const char* path, char** out_buf, size_t* out_len)
{
    FILE* f = NULL;
#ifdef _MSC_VER
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return 0;
    }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }
    size_t n = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    buf[n] = '\0';
    if (out_buf)
        *out_buf = buf;
    if (out_len)
        *out_len = n;
    return 1;
}

static int json_find_string_local(const char* obj, const char* key, char* out, size_t out_sz)
{
    if (!obj || !key || !out || out_sz == 0)
        return 0;
    out[0] = '\0';
    const char* p = obj;
    size_t klen = strlen(key);
    for (;;)
    {
        const char* found = strstr(p, key);
        if (!found)
            break;
        p = found;
        const char* q = p + klen;
        while (*q && isspace((unsigned char) *q))
            q++;
        if (*q != ':')
        {
            p = p + 1;
            continue;
        }
        q++;
        while (*q && isspace((unsigned char) *q))
            q++;
        if (*q != '"')
        {
            p = p + 1;
            continue;
        }
        q++;
        size_t i = 0;
        while (*q && *q != '"' && i < out_sz - 1)
        {
            out[i++] = *q++;
        }
        out[i] = '\0';
        return i > 0;
    }
    return 0;
}
static int json_find_int_local(const char* obj, const char* key, int* out)
{
    if (!obj || !key || !out)
        return 0;
    const char* p = obj;
    size_t klen = strlen(key);
    for (;;)
    {
        const char* found = strstr(p, key);
        if (!found)
            break;
        p = found;
        const char* q = p + klen;
        while (*q && isspace((unsigned char) *q))
            q++;
        if (*q != ':')
        {
            p = p + 1;
            continue;
        }
        q++;
        while (*q && isspace((unsigned char) *q))
            q++;
        int neg = 0;
        if (*q == '-')
        {
            neg = 1;
            q++;
        }
        int val = 0;
        int digits = 0;
        while (*q && isdigit((unsigned char) *q))
        {
            val = val * 10 + (*q - '0');
            q++;
            digits++;
        }
        if (digits > 0)
        {
            *out = neg ? -val : val;
            return 1;
        }
        p = p + 1;
    }
    return 0;
}
static int json_find_int_array(const char* obj, const char* key, int* out, int expected, int fill)
{
    for (int i = 0; i < expected; i++)
        out[i] = fill;
    if (!obj || !key)
        return 0;
    const char* p = strstr(obj, key);
    if (!p)
        return 0;
    p += strlen(key);
    while (*p && *p != '[')
        p++;
    if (*p != '[')
        return 0;
    p++;
    int idx = 0;
    while (*p && *p != ']' && idx < expected)
    {
        while (*p && (isspace((unsigned char) *p) || *p == ','))
            p++;
        int neg = 0;
        if (*p == '-')
        {
            neg = 1;
            p++;
        }
        int val = 0;
        int digits = 0;
        while (*p && isdigit((unsigned char) *p))
        {
            val = val * 10 + (*p - '0');
            p++;
            digits++;
        }
        if (digits > 0)
        {
            out[idx++] = neg ? -val : val;
        }
        while (*p && *p != ',' && *p != ']')
            p++;
    }
    return 1;
}

static const char* find_next_object(const char* buf)
{
    const char* c = strchr(buf, '{');
    return c;
}
static const char* find_end_object(const char* start)
{
    if (!start)
        return NULL;
    const char* p = start + 1;
    int depth = 1;
    while (*p)
    {
        if (*p == '{')
            depth++;
        else if (*p == '}')
        {
            depth--;
            if (depth == 0)
                return p;
        }
        p++;
    }
    return NULL;
}

static int split_tokens(char* line, char** out, int max)
{
    int c = 0;
    char* start = line;
    while (*start && c < max)
    {
        char* comma = strchr(start, ',');
        if (comma)
        {
            *comma = '\0';
        }
        /* trim leading spaces */ while (*start == ' ' || *start == '\t')
            start++;
        /* trim trailing spaces */ char* end = start + strlen(start);
        while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
        {
            end--;
        }
        *end = '\0';
        out[c++] = start;
        if (!comma)
            break;
        start = comma + 1;
    }
    return c;
}

static void trim_newline(char* s)
{
    if (!s)
        return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
    {
        s[n - 1] = '\0';
        n--;
    }
}

/* forward decl for safe string copy used below */
static void copy_str(char* dst, size_t cap, const char* src);

static int load_file_generic(const char* rel, int (*handler)(char** tokens, int count))
{
    char path[256];
    if (!rogue_find_asset_path(rel, path, sizeof path))
        return 0;
    ROGUE_LOCAL_DISABLE_WARNINGS;
    FILE* f = fopen(path, "rb");
    ROGUE_LOCAL_RESTORE_WARNINGS;
    if (!f)
        return 0;
    char line[256];
    int ok = 1;
    while (fgets(line, sizeof line, f))
    {
        trim_newline(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;
        char temp[256];
        /* safe copy to avoid MSVC C4996 on strncpy */
        copy_str(temp, sizeof temp, line);
        char* toks[16];
        int ct = split_tokens(temp, toks, 16);
        if (ct > 0)
        {
            if (!handler(toks, ct))
            {
                ok = 0;
                break;
            }
        }
    }
    fclose(f);
    return ok;
}

static void copy_str(char* dst, size_t cap, const char* src)
{
    if (cap == 0)
        return;
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    size_t i = 0;
    for (; i < cap - 1 && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}
static void parse_space_ints(const char* src, int* out, int needed, int fill)
{
    int i = 0;
    if (src)
    {
        const char* p = src;
        while (*p && i < needed)
        {
            while (*p == ' ')
                p++;
            char buf[16];
            int bi = 0;
            while (*p && *p != ' ' && bi < (int) sizeof buf - 1)
            {
                buf[bi++] = *p++;
            }
            buf[bi] = '\0';
            if (bi > 0)
            {
                out[i++] = atoi(buf);
            }
            while (*p == ' ')
                p++;
        }
    }
    for (; i < needed; i++)
        out[i] = fill;
}
static int handle_vendor(char** t, int c)
{
    if (c < 5)
        return 0;
    if (g_vendor_count >= ROGUE_MAX_VENDOR_DEFS)
        return 0;
    RogueVendorDef* d = &g_vendors[g_vendor_count];
    memset(d, 0, sizeof *d);
    copy_str(d->id, sizeof d->id, t[0]);
    copy_str(d->archetype, sizeof d->archetype, t[1]);
    copy_str(d->biome_tags, sizeof d->biome_tags, t[2]);
    d->refresh_interval_ms = atoi(t[3]);
    const RoguePricePolicy* pol = NULL;
    for (int i = 0; i < g_policy_count; i++)
    {
        if (strcmp(g_policies[i].id, t[4]) == 0)
        {
            pol = &g_policies[i];
            break;
        }
    }
    d->price_policy_index = pol ? (int) (pol - g_policies) : -1;
    g_vendor_count++;
    return 1;
}
static int handle_policy(char** t, int c)
{
    if (c < 5)
        return 0;
    if (g_policy_count >= ROGUE_MAX_PRICE_POLICIES)
        return 0;
    RoguePricePolicy* p = &g_policies[g_policy_count];
    memset(p, 0, sizeof *p);
    copy_str(p->id, sizeof p->id, t[0]);
    p->base_buy_margin = atoi(t[1]);
    p->base_sell_margin = atoi(t[2]);
    parse_space_ints(t[3], p->rarity_mods, 5, 100);
    parse_space_ints(t[4], p->category_mods, 6, 100);
    g_policy_count++;
    return 1;
}
static int handle_rep(char** t, int c)
{
    if (c < 4)
        return 0;
    if (g_rep_tier_count >= ROGUE_MAX_REP_TIERS)
        return 0;
    RogueRepTier* r = &g_rep_tiers[g_rep_tier_count];
    memset(r, 0, sizeof *r);
    copy_str(r->id, sizeof r->id, t[0]);
    r->rep_min = atoi(t[1]);
    r->buy_discount_pct = atoi(t[2]);
    r->sell_bonus_pct = atoi(t[3]);
    const char* unlock = (c >= 5) ? t[4] : "";
    copy_str(r->unlock_tags, sizeof r->unlock_tags, unlock);
    g_rep_tier_count++;
    return 1;
}

/* ---------------- JSON loaders ---------------- */
static int load_price_policies_json(void)
{
    char path[256];
    if (!rogue_find_asset_path("vendors/price_policies.json", path, sizeof path))
        return 0;
    char* buf = NULL;
    size_t len = 0;
    if (!read_entire_file(path, &buf, &len))
        return 0;
    const char* section = strstr(buf, "\"price_policies\"");
    if (!section)
    {
        free(buf);
        return 0;
    }
    const char* p = section;
    int added = 0;
    while (g_policy_count < ROGUE_MAX_PRICE_POLICIES)
    {
        const char* obj = find_next_object(p);
        if (!obj)
            break;
        const char* end = find_end_object(obj);
        if (!end)
            break;
        size_t obj_len = (size_t) (end - obj + 1);
        if (obj_len > 1023)
            obj_len = 1023;
        char tmp[1024];
        memcpy(tmp, obj, obj_len);
        tmp[obj_len] = '\0';
        char id[32];
        if (!json_find_string_local(tmp, "\"id\"", id, sizeof id))
        {
            p = end + 1;
            continue;
        }
        RoguePricePolicy* pol = &g_policies[g_policy_count];
        memset(pol, 0, sizeof *pol);
        copy_str(pol->id, sizeof pol->id, id);
        pol->base_buy_margin = 100;
        pol->base_sell_margin = 50;
        json_find_int_local(tmp, "\"base_buy_margin\"", &pol->base_buy_margin);
        json_find_int_local(tmp, "\"base_sell_margin\"", &pol->base_sell_margin);
        json_find_int_array(tmp, "\"rarity_mods\"", pol->rarity_mods, 5, 100);
        json_find_int_array(tmp, "\"category_mods\"", pol->category_mods, 6, 100);
        g_policy_count++;
        added++;
        p = end + 1;
    }
    free(buf);
    return added > 0;
}

static int load_vendors_json(void)
{
    char path[256];
    if (!rogue_find_asset_path("vendors/vendors.json", path, sizeof path))
        return 0;
    char* buf = NULL;
    size_t len = 0;
    if (!read_entire_file(path, &buf, &len))
        return 0;
    const char* section = strstr(buf, "\"vendors\"");
    if (!section)
    {
        free(buf);
        return 0;
    }
    const char* p = section;
    int added = 0;
    while (g_vendor_count < ROGUE_MAX_VENDOR_DEFS)
    {
        const char* obj = find_next_object(p);
        if (!obj)
            break;
        const char* end = find_end_object(obj);
        if (!end)
            break;
        char tmp[1024];
        size_t obj_len = (size_t) (end - obj + 1);
        if (obj_len > 1023)
            obj_len = 1023;
        memcpy(tmp, obj, obj_len);
        tmp[obj_len] = '\0';
        char id[32];
        if (!json_find_string_local(tmp, "\"id\"", id, sizeof id))
        {
            p = end + 1;
            continue;
        }
        RogueVendorDef* vd = &g_vendors[g_vendor_count];
        memset(vd, 0, sizeof *vd);
        copy_str(vd->id, sizeof vd->id, id);
        json_find_string_local(tmp, "\"archetype\"", vd->archetype, sizeof vd->archetype);
        json_find_string_local(tmp, "\"biome_tags\"", vd->biome_tags, sizeof vd->biome_tags);
        vd->refresh_interval_ms = 600000;
        json_find_int_local(tmp, "\"refresh_interval_ms\"", &vd->refresh_interval_ms);
        char pol_id[32];
        if (json_find_string_local(tmp, "\"price_policy\"", pol_id, sizeof pol_id))
        {
            const RoguePricePolicy* pol = NULL;
            for (int i = 0; i < g_policy_count; i++)
            {
                if (strcmp(g_policies[i].id, pol_id) == 0)
                {
                    pol = &g_policies[i];
                    break;
                }
            }
            vd->price_policy_index = pol ? (int) (pol - g_policies) : -1;
        }
        else
            vd->price_policy_index = -1;
        g_vendor_count++;
        added++;
        p = end + 1;
    }
    free(buf);
    return added > 0;
}

static int load_rep_tiers_json(void)
{
    char path[256];
    if (!rogue_find_asset_path("vendors/reputation_tiers.json", path, sizeof path))
        return 0;
    char* buf = NULL;
    size_t len = 0;
    if (!read_entire_file(path, &buf, &len))
        return 0;
    const char* section = strstr(buf, "\"reputation_tiers\"");
    if (!section)
    {
        free(buf);
        return 0;
    }
    const char* p = section;
    int added = 0;
    while (g_rep_tier_count < ROGUE_MAX_REP_TIERS)
    {
        const char* obj = find_next_object(p);
        if (!obj)
            break;
        const char* end = find_end_object(obj);
        if (!end)
            break;
        char tmp[512];
        size_t obj_len = (size_t) (end - obj + 1);
        if (obj_len > 511)
            obj_len = 511;
        memcpy(tmp, obj, obj_len);
        tmp[obj_len] = '\0';
        char id[32];
        if (!json_find_string_local(tmp, "\"id\"", id, sizeof id))
        {
            p = end + 1;
            continue;
        }
        RogueRepTier* rt = &g_rep_tiers[g_rep_tier_count];
        memset(rt, 0, sizeof *rt);
        copy_str(rt->id, sizeof rt->id, id);
        json_find_int_local(tmp, "\"rep_min\"", &rt->rep_min);
        json_find_int_local(tmp, "\"buy_discount_pct\"", &rt->buy_discount_pct);
        json_find_int_local(tmp, "\"sell_bonus_pct\"", &rt->sell_bonus_pct);
        json_find_string_local(tmp, "\"unlock_tags\"", rt->unlock_tags, sizeof rt->unlock_tags);
        g_rep_tier_count++;
        added++;
        p = end + 1;
    }
    free(buf);
    return added > 0;
}

static int load_negotiation_rules_json(void)
{
    char path[256];
    if (!rogue_find_asset_path("vendors/negotiation_rules.json", path, sizeof path))
        return 0;
    char* buf = NULL;
    size_t len = 0;
    if (!read_entire_file(path, &buf, &len))
        return 0;
    const char* section = strstr(buf, "\"negotiation_rules\"");
    if (!section)
    {
        free(buf);
        return 0;
    }
    const char* p = section;
    int added = 0;
    while (g_negotiation_rule_count < ROGUE_MAX_NEGOTIATION_RULES)
    {
        const char* obj = find_next_object(p);
        if (!obj)
            break;
        const char* end = find_end_object(obj);
        if (!end)
            break;
        char tmp[512];
        size_t obj_len = (size_t) (end - obj + 1);
        if (obj_len > 511)
            obj_len = 511;
        memcpy(tmp, obj, obj_len);
        tmp[obj_len] = '\0';
        char id[32];
        if (!json_find_string_local(tmp, "\"id\"", id, sizeof id))
        {
            p = end + 1;
            continue;
        }
        RogueNegotiationRule* nr = &g_negotiation_rules[g_negotiation_rule_count];
        memset(nr, 0, sizeof *nr);
        copy_str(nr->id, sizeof nr->id, id);
        json_find_string_local(tmp, "\"skill_checks\"", nr->skill_checks, sizeof nr->skill_checks);
        json_find_int_local(tmp, "\"min_roll\"", &nr->min_roll);
        json_find_int_local(tmp, "\"discount_min_pct\"", &nr->discount_min_pct);
        json_find_int_local(tmp, "\"discount_max_pct\"", &nr->discount_max_pct);
        g_negotiation_rule_count++;
        added++;
        p = end + 1;
    }
    free(buf);
    return added > 0;
}

static int audit_uniqueness(void)
{
    /* vendors */
    for (int i = 0; i < g_vendor_count; i++)
        for (int j = i + 1; j < g_vendor_count; j++)
            if (strcmp(g_vendors[i].id, g_vendors[j].id) == 0)
            {
                fprintf(stderr, "VENDOR_REG_DUP vendor id=%s\n", g_vendors[i].id);
                return 0;
            }
    /* policies */
    for (int i = 0; i < g_policy_count; i++)
        for (int j = i + 1; j < g_policy_count; j++)
            if (strcmp(g_policies[i].id, g_policies[j].id) == 0)
            {
                fprintf(stderr, "VENDOR_REG_DUP policy id=%s\n", g_policies[i].id);
                return 0;
            }
    /* rep tiers */
    for (int i = 0; i < g_rep_tier_count; i++)
        for (int j = i + 1; j < g_rep_tier_count; j++)
            if (strcmp(g_rep_tiers[i].id, g_rep_tiers[j].id) == 0)
            {
                fprintf(stderr, "VENDOR_REG_DUP rep id=%s\n", g_rep_tiers[i].id);
                return 0;
            }
    /* negotiation rules */
    for (int i = 0; i < g_negotiation_rule_count; i++)
        for (int j = i + 1; j < g_negotiation_rule_count; j++)
            if (strcmp(g_negotiation_rules[i].id, g_negotiation_rules[j].id) == 0)
            {
                fprintf(stderr, "VENDOR_REG_DUP nego_rule id=%s\n", g_negotiation_rules[i].id);
                return 0;
            }
    return 1;
}

int rogue_vendor_registry_load_all(void)
{
    g_vendor_count = g_policy_count = g_rep_tier_count = g_negotiation_rule_count = 0;
    /* Try JSON first */
    int json_ok = 1;
    if (!load_price_policies_json())
        json_ok = 0;
    if (json_ok && !load_vendors_json())
        json_ok = 0;
    if (json_ok && !load_rep_tiers_json())
        json_ok = 0;
    if (json_ok && !load_negotiation_rules_json())
        json_ok = 0; /* negotiation rules optional? require for full success */
    if (json_ok && g_policy_count > 0 && g_vendor_count > 0 && g_rep_tier_count > 0 &&
        g_negotiation_rule_count > 0)
    {
        if (!audit_uniqueness())
        {
            fprintf(stderr, "VENDOR_REG_LOAD_FAIL dup (json)\n");
            json_ok = 0;
        }
        else
            return 1; /* success path */
    }
    /* Fallback to legacy cfg if JSON incomplete */
    g_vendor_count = g_policy_count = g_rep_tier_count = g_negotiation_rule_count = 0;
    if (!load_file_generic("vendors/price_policies.cfg", handle_policy))
    {
        fprintf(stderr, "VENDOR_REG_LOAD_FAIL policies (legacy)\n");
        return 0;
    }
    if (!load_file_generic("vendors/vendors.cfg", handle_vendor))
    {
        fprintf(stderr, "VENDOR_REG_LOAD_FAIL vendors (legacy)\n");
        return 0;
    }
    if (!load_file_generic("vendors/reputation_tiers.cfg", handle_rep))
    {
        fprintf(stderr, "VENDOR_REG_LOAD_FAIL rep (legacy)\n");
        return 0;
    }
    if (!audit_uniqueness())
    {
        fprintf(stderr, "VENDOR_REG_LOAD_FAIL dup (legacy)\n");
        return 0;
    }
    return 1;
}

int rogue_vendor_def_count(void) { return g_vendor_count; }
const RogueVendorDef* rogue_vendor_def_at(int idx)
{
    if (idx < 0 || idx >= g_vendor_count)
        return NULL;
    return &g_vendors[idx];
}
const RogueVendorDef* rogue_vendor_def_find(const char* id)
{
    if (!id)
        return NULL;
    for (int i = 0; i < g_vendor_count; i++)
        if (strcmp(g_vendors[i].id, id) == 0)
            return &g_vendors[i];
    return NULL;
}

int rogue_price_policy_count(void) { return g_policy_count; }
const RoguePricePolicy* rogue_price_policy_at(int idx)
{
    if (idx < 0 || idx >= g_policy_count)
        return NULL;
    return &g_policies[idx];
}
const RoguePricePolicy* rogue_price_policy_find(const char* id)
{
    if (!id)
        return NULL;
    for (int i = 0; i < g_policy_count; i++)
        if (strcmp(g_policies[i].id, id) == 0)
            return &g_policies[i];
    return NULL;
}

int rogue_rep_tier_count(void) { return g_rep_tier_count; }
const RogueRepTier* rogue_rep_tier_at(int idx)
{
    if (idx < 0 || idx >= g_rep_tier_count)
        return NULL;
    return &g_rep_tiers[idx];
}
const RogueRepTier* rogue_rep_tier_find(const char* id)
{
    if (!id)
        return NULL;
    for (int i = 0; i < g_rep_tier_count; i++)
        if (strcmp(g_rep_tiers[i].id, id) == 0)
            return &g_rep_tiers[i];
    return NULL;
}

int rogue_negotiation_rule_count(void) { return g_negotiation_rule_count; }
const RogueNegotiationRule* rogue_negotiation_rule_at(int idx)
{
    if (idx < 0 || idx >= g_negotiation_rule_count)
        return NULL;
    return &g_negotiation_rules[idx];
}
const RogueNegotiationRule* rogue_negotiation_rule_find(const char* id)
{
    if (!id)
        return NULL;
    for (int i = 0; i < g_negotiation_rule_count; i++)
        if (strcmp(g_negotiation_rules[i].id, id) == 0)
            return &g_negotiation_rules[i];
    return NULL;
}
