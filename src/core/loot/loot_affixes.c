#include "loot_affixes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif

static RogueAffixDef g_affixes[ROGUE_MAX_AFFIXES];
static int g_affix_count = 0;

int rogue_affixes_reset(void)
{
    g_affix_count = 0;
    return 0;
}
int rogue_affix_count(void) { return g_affix_count; }
const RogueAffixDef* rogue_affix_at(int index)
{
    if (index < 0 || index >= g_affix_count)
        return NULL;
    return &g_affixes[index];
}
int rogue_affix_index(const char* id)
{
    if (!id)
        return -1;
    for (int i = 0; i < g_affix_count; i++)
    {
        if (strcmp(g_affixes[i].id, id) == 0)
            return i;
    }
    return -1;
}

static char* next_field(char** cur)
{
    if (!*cur)
        return NULL;
    char* s = *cur;
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
    {
    }
    *cur = (*p) ? p : NULL;
    return s;
}
static RogueAffixType parse_type(const char* s)
{
    if (strcmp(s, "PREFIX") == 0)
        return ROGUE_AFFIX_PREFIX;
    return ROGUE_AFFIX_SUFFIX;
}
static RogueAffixStat parse_stat(const char* s)
{
    if (strcmp(s, "damage_flat") == 0)
        return ROGUE_AFFIX_STAT_DAMAGE_FLAT;
    if (strcmp(s, "agility_flat") == 0)
        return ROGUE_AFFIX_STAT_AGILITY_FLAT; /* legacy */
    if (strcmp(s, "strength_flat") == 0)
        return ROGUE_AFFIX_STAT_STRENGTH_FLAT;
    if (strcmp(s, "dexterity_flat") == 0)
        return ROGUE_AFFIX_STAT_DEXTERITY_FLAT;
    if (strcmp(s, "vitality_flat") == 0)
        return ROGUE_AFFIX_STAT_VITALITY_FLAT;
    if (strcmp(s, "intelligence_flat") == 0)
        return ROGUE_AFFIX_STAT_INTELLIGENCE_FLAT;
    if (strcmp(s, "armor_flat") == 0)
        return ROGUE_AFFIX_STAT_ARMOR_FLAT;
    if (strcmp(s, "resist_physical") == 0)
        return ROGUE_AFFIX_STAT_RESIST_PHYSICAL;
    if (strcmp(s, "resist_fire") == 0)
        return ROGUE_AFFIX_STAT_RESIST_FIRE;
    if (strcmp(s, "resist_cold") == 0)
        return ROGUE_AFFIX_STAT_RESIST_COLD;
    if (strcmp(s, "resist_lightning") == 0)
        return ROGUE_AFFIX_STAT_RESIST_LIGHTNING;
    if (strcmp(s, "resist_poison") == 0)
        return ROGUE_AFFIX_STAT_RESIST_POISON;
    if (strcmp(s, "resist_status") == 0)
        return ROGUE_AFFIX_STAT_RESIST_STATUS;
    if (strcmp(s, "block_chance") == 0)
        return ROGUE_AFFIX_STAT_BLOCK_CHANCE; /* Phase 7 */
    if (strcmp(s, "block_value") == 0)
        return ROGUE_AFFIX_STAT_BLOCK_VALUE; /* Phase 7 */
    if (strcmp(s, "phys_conv_fire_pct") == 0)
        return ROGUE_AFFIX_STAT_PHYS_CONV_FIRE_PCT; /* Phase 7.2 */
    if (strcmp(s, "phys_conv_frost_pct") == 0)
        return ROGUE_AFFIX_STAT_PHYS_CONV_FROST_PCT; /* Phase 7.2 */
    if (strcmp(s, "phys_conv_arcane_pct") == 0)
        return ROGUE_AFFIX_STAT_PHYS_CONV_ARCANE_PCT; /* Phase 7.2 */
    if (strcmp(s, "guard_recovery_pct") == 0)
        return ROGUE_AFFIX_STAT_GUARD_RECOVERY_PCT; /* Phase 7.3 */
    if (strcmp(s, "thorns_percent") == 0)
        return ROGUE_AFFIX_STAT_THORNS_PERCENT; /* Phase 7.5 */
    if (strcmp(s, "thorns_cap") == 0)
        return ROGUE_AFFIX_STAT_THORNS_CAP; /* Phase 7.5 */
    return ROGUE_AFFIX_STAT_NONE;
}

static int parse_line(char* line)
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
    char* f0 = next_field(&cursor);
    char* f1 = next_field(&cursor);
    char* f2 = next_field(&cursor);
    char* f3 = next_field(&cursor);
    char* f4 = next_field(&cursor);
    char* f5 = next_field(&cursor);
    char* f6 = next_field(&cursor);
    char* f7 = next_field(&cursor);
    char* f8 = next_field(&cursor);
    char* f9 = next_field(&cursor);
    if (!f0 || !f1 || !f2 || !f3 || !f4 || !f5 || !f6 || !f7 || !f8 || !f9)
        return -1;
    /* Two formats supported:
       A) Default: type,id,stat,min,max,w0..w4
       B) ExamplePack: id,type_num(0=prefix,1=suffix),stat,min,max,w0..w4 */
    const char* f_type;
    const char* f_id;
    const char* f_stat;
    const char* f_min;
    const char* f_max;
    const char* f_w0;
    const char* f_w1;
    const char* f_w2;
    const char* f_w3;
    const char* f_w4;
    int example_pack = 0;
    if (strcmp(f0, "PREFIX") == 0 || strcmp(f0, "SUFFIX") == 0)
    {
        f_type = f0; /* Default format */
        f_id = f1;
        f_stat = f2;
        f_min = f3;
        f_max = f4;
        f_w0 = f5;
        f_w1 = f6;
        f_w2 = f7;
        f_w3 = f8;
        f_w4 = f9;
    }
    else
    {
        /* Example pack: first field is id, second numeric type 0/1 */
        example_pack = 1;
        f_id = f0;
        f_type = (f1 && strcmp(f1, "1") == 0) ? "SUFFIX" : "PREFIX";
        f_stat = f2;
        f_min = f3;
        f_max = f4;
        f_w0 = f5;
        f_w1 = f6;
        f_w2 = f7;
        f_w3 = f8;
        f_w4 = f9;
    }
    if (g_affix_count >= ROGUE_MAX_AFFIXES)
        return -1; /* bounds guard */
    RogueAffixDef d;
    memset(&d, 0, sizeof d);
    d.type = parse_type(f_type);
#if defined(_MSC_VER)
    strncpy_s(d.id, sizeof d.id, f_id, _TRUNCATE);
#else
    strncpy(d.id, f_id, sizeof d.id - 1);
#endif
    d.stat = parse_stat(f_stat);
    d.min_value = (int) strtol(f_min, NULL, 10);
    d.max_value = (int) strtol(f_max, NULL, 10);
    if (d.max_value < d.min_value)
        d.max_value = d.min_value;
    d.weight_per_rarity[0] = (int) strtol(f_w0, NULL, 10);
    d.weight_per_rarity[1] = (int) strtol(f_w1, NULL, 10);
    d.weight_per_rarity[2] = (int) strtol(f_w2, NULL, 10);
    d.weight_per_rarity[3] = (int) strtol(f_w3, NULL, 10);
    d.weight_per_rarity[4] = (int) strtol(f_w4, NULL, 10);
    g_affixes[g_affix_count++] = d;
    return 1;
}

/* Try opening a file at candidate; returns FILE* or NULL. */
static FILE* try_open_rb(const char* candidate)
{
#if defined(_MSC_VER)
    FILE* f = NULL;
    if (fopen_s(&f, candidate, "rb") == 0)
        return f;
    return NULL;
#else
    return fopen(candidate, "rb");
#endif
}

/* Append b to a into out using forward slashes, ensuring at most one slash. */
static void join_path(char* out, size_t cap, const char* a, const char* b)
{
    if (!out || cap == 0)
        return;
    out[0] = '\0';
    if (!a || !*a)
    {
#if defined(_MSC_VER)
        strncpy_s(out, cap, b ? b : "", _TRUNCATE);
#else
        strncpy(out, b ? b : "", cap - 1);
        out[cap - 1] = '\0';
#endif
        return;
    }
    size_t la = strlen(a);
    int need_slash = (la > 0 && a[la - 1] != '/' && a[la - 1] != '\\');
    if (need_slash)
    {
#if defined(_MSC_VER)
        snprintf(out, cap, "%s/%s", a, b ? b : "");
#else
        snprintf(out, cap, "%s/%s", a, b ? b : "");
#endif
    }
    else
    {
#if defined(_MSC_VER)
        snprintf(out, cap, "%s%s", a, b ? b : "");
#else
        snprintf(out, cap, "%s%s", a, b ? b : "");
#endif
    }
}

/* Walk up directory levels from a base directory trying to open rel (e.g., "assets/affixes.cfg").
 */
static FILE* try_open_upwards(const char* base_dir, const char* rel, int max_levels)
{
    if (!base_dir || !*base_dir || !rel || !*rel)
        return NULL;
    char cur[512];
#if defined(_MSC_VER)
    strncpy_s(cur, sizeof cur, base_dir, _TRUNCATE);
#else
    strncpy(cur, base_dir, sizeof cur - 1);
    cur[sizeof cur - 1] = '\0';
#endif
    for (int i = 0; i <= max_levels; ++i)
    {
        char candidate[768];
        join_path(candidate, sizeof candidate, cur, rel);
        FILE* f = try_open_rb(candidate);
        if (f)
            return f;
        /* move one level up */
        char* last_slash = NULL;
        for (char* p = cur + strlen(cur) - 1; p >= cur; --p)
        {
            if (*p == '/' || *p == '\\')
            {
                last_slash = p;
                break;
            }
        }
        if (!last_slash)
            break;
        *last_slash = '\0';
    }
    return NULL;
}

int rogue_affixes_load_from_cfg(const char* path)
{
    FILE* f = NULL;
    int added = 0;
    const char* in_path = path ? path : "";
    /* Try the given path first */
    f = try_open_rb(in_path);
    if (!f)
    {
        /* Best-effort relative fallbacks for common test working directories. */
        const char* prefixes[] = {"../", "../../", "../../../", "../../../../"};
        char candidate[512];
        int opened = 0;
        /* If caller passed a relative path, try prefixing it. If it looks like the canonical
           path 'assets/affixes.cfg', try that same filename with prefixes as well. */
        int is_relative =
            in_path[0] != '\\' && in_path[0] != '/' && !(in_path[0] && in_path[1] == ':');
        for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
        {
            if (is_relative)
            {
                int n = snprintf(candidate, sizeof candidate, "%s%s", prefixes[i], in_path);
                if (n > 0 && n < (int) sizeof candidate)
                {
                    f = try_open_rb(candidate);
                    if (f)
                    {
                        opened = 1;
                        break;
                    }
                }
            }
        }

        /* If still not opened and caller didn't pass the canonical affixes path, attempt
           prefixes against the canonical path. */
        if (!opened)
        {
            const char* canonical = "assets/affixes.cfg";
            for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
            {
                int n = snprintf(candidate, sizeof candidate, "%s%s", prefixes[i], canonical);
                if (n > 0 && n < (int) sizeof candidate)
                {
                    f = try_open_rb(candidate);
                    if (f)
                    {
                        opened = 1;
                        break;
                    }
                }
            }
        }

        /* If still not opened, scan upwards from current working directory for assets/affixes.cfg
         */
        if (!opened)
        {
            const char* rel = "assets/affixes.cfg";
#if defined(_WIN32)
            char cwd[512];
            DWORD len = GetCurrentDirectoryA((DWORD) sizeof cwd, cwd);
            if (len > 0 && len < sizeof cwd)
            {
                FILE* f2 = try_open_upwards(cwd, rel, 8);
                if (f2)
                {
                    f = f2;
                    opened = 1;
                }
            }
#else
            /* POSIX: use getcwd if available */
            char cwd[512];
            if (getcwd(cwd, sizeof cwd))
            {
                FILE* f2 = try_open_upwards(cwd, rel, 8);
                if (f2)
                {
                    f = f2;
                    opened = 1;
                }
            }
#endif
        }

        /* Still not opened? Try scanning upwards from the executable directory. */
        if (!opened)
        {
#if defined(_WIN32)
            char exe_path[512];
            DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD) sizeof exe_path);
            if (n > 0 && n < sizeof exe_path)
            {
                /* strip filename */
                char* last = NULL;
                for (char* p = exe_path + strlen(exe_path) - 1; p >= exe_path; --p)
                {
                    if (*p == '/' || *p == '\\')
                    {
                        last = p;
                        break;
                    }
                }
                if (last)
                {
                    *last = '\0';
                    FILE* f2 = try_open_upwards(exe_path, "assets/affixes.cfg", 8);
                    if (f2)
                    {
                        f = f2;
                        opened = 1;
                    }
                }
            }
#endif
        }

        if (!opened)
        {
            /* Give up – keep the original error behavior (return -1). */
            /* Emit a single failure log only when all strategies failed. */
            fprintf(stderr, "AFFIX_OPEN_FAIL %s\n", in_path[0] ? in_path : "(null)");
            return -1;
        }
    }
    char line[512];
    while (fgets(line, sizeof line, f))
    {
        char work[512];
#if defined(_MSC_VER)
        strncpy_s(work, sizeof work, line, _TRUNCATE);
#else
        strncpy(work, line, sizeof work - 1);
        work[sizeof work - 1] = '\0';
#endif
        int r = parse_line(work);
        if (r > 0)
            added++;
        else if (r < 0)
        { /* ignore malformed */
        }
    }
    fclose(f);
    return added;
}

int rogue_affix_roll(RogueAffixType type, int rarity, unsigned int* rng_state)
{
    if (rarity < 0 || rarity > 4)
        return -1;
    if (!rng_state)
        return -1;
    int total = 0;
    for (int i = 0; i < g_affix_count; i++)
    {
        if (g_affixes[i].type == type)
        {
            int w = g_affixes[i].weight_per_rarity[rarity];
            if (w > 0)
                total += w;
        }
    }
    if (total <= 0)
        return -1;
    *rng_state = (*rng_state * 1664525u) + 1013904223u;
    unsigned int pick = *rng_state % (unsigned) total;
    int acc = 0;
    for (int i = 0; i < g_affix_count; i++)
    {
        if (g_affixes[i].type == type)
        {
            int w = g_affixes[i].weight_per_rarity[rarity];
            if (w <= 0)
                continue;
            acc += w;
            if (pick < (unsigned) acc)
                return i;
        }
    }
    return -1;
}

int rogue_affix_roll_value(int affix_index, unsigned int* rng_state)
{
    if (!rng_state)
        return -1;
    if (affix_index < 0 || affix_index >= g_affix_count)
        return -1;
    const RogueAffixDef* d = &g_affixes[affix_index];
    int span = d->max_value - d->min_value + 1;
    if (span <= 0)
        return d->min_value;
    *rng_state = (*rng_state * 1664525u) + 1013904223u;
    unsigned int r = *rng_state % (unsigned) span;
    return d->min_value + (int) r;
}

int rogue_affix_roll_value_scaled(int affix_index, unsigned int* rng_state, float quality_scalar)
{
    if (!rng_state)
        return -1;
    if (affix_index < 0 || affix_index >= g_affix_count)
        return -1;
    if (quality_scalar < 0.0f)
        quality_scalar = 0.0f; /* clamp */
    const RogueAffixDef* d = &g_affixes[affix_index];
    int span = d->max_value - d->min_value + 1;
    if (span <= 0)
        return d->min_value;
    /* Derive an exponent shaping factor from quality_scalar: qc=1 -> exp=1 (uniform). Larger qc ->
     * smaller decay -> higher values. */
    float exp = (quality_scalar <= 1.0f)
                    ? 1.0f
                    : (1.0f / quality_scalar); /* >1 quality compresses lower region */
    *rng_state = (*rng_state * 1664525u) + 1013904223u;
    /* Convert RNG to [0,1) */
    unsigned int raw = *rng_state & 0x00FFFFFFu; /* 24 bits */
    float u = (float) raw / (float) 0x01000000u; /* [0,1) */
    /* Skew distribution: y = u^(exp) then map to integer. Since exp<1 for quality>1, pushes upward.
     */
    float y = 0.0f;
    if (u <= 0.0f)
        y = 0.0f;
    else
    {
        /* simple powf replacement (avoid math.h pow potential differences) using exp/log series for
         * small set; fallback linear if unavailable */
        /* For portability without powf, approximate via expf(exp*log(u)). We'll include math.h. */
    }
    /* Because we avoided powf above for portability, implement manual fast approximation: use 3rd
     * order polynomial around (u). For exp in [0.25,1], approximate u^exp ~ u * (1 +
     * (1-exp)*(1-u)). */
    if (exp >= 0.25f && exp <= 1.0f)
    {
        y = u * (1.0f + (1.0f - exp) * (1.0f - u));
    }
    else
    {
        /* fallback to uniform if outside expected range */
        y = u;
    }
    int offset = (int) (y * (float) span);
    if (offset >= span)
        offset = span - 1;
    return d->min_value + offset;
}

int rogue_affixes_export_json(char* buf, int cap)
{
    if (!buf || cap <= 0)
        return -1;
    int w = 0;
    buf[0] = '\0';
    int n = snprintf(buf + w, (w < cap ? cap - w : 0), "[");
    if (n > 0)
        w += n;
    int first = 1;
    for (int i = 0; i < g_affix_count; i++)
    {
        const RogueAffixDef* a = &g_affixes[i];
        n = snprintf(buf + (w < cap ? w : cap), (w < cap ? cap - w : 0),
                     "%s{\"id\":\"%s\",\"type\":%d,\"stat\":%d,\"min\":%d,\"max\":%d,\"w\":[%d,%d,%"
                     "d,%d,%d]}",
                     first ? "" : ",", a->id, a->type, a->stat, a->min_value, a->max_value,
                     a->weight_per_rarity[0], a->weight_per_rarity[1], a->weight_per_rarity[2],
                     a->weight_per_rarity[3], a->weight_per_rarity[4]);
        if (n > 0)
            w += n;
        first = 0;
    }
    n = snprintf(buf + (w < cap ? w : cap), (w < cap ? cap - w : 0), "]");
    if (n > 0)
        w += n;
    if (w >= cap)
        buf[cap - 1] = '\0';
    return w;
}
