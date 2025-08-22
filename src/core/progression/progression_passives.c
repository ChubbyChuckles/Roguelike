#include "core/progression/progression_passives.h"
#include "core/progression/progression_maze.h"
#include "core/progression/progression_stats.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define PASSIVE_MAX_NODE_EFFECTS 8
#define PASSIVE_MAX_EFFECTS_TOTAL 4096

/* Phase 11.2: SoA-friendly effect storage. Retain AoS struct for parsing simplicity but mirror into
    parallel arrays for cache-friendly aggregation / future vectorization. */
typedef struct PassiveEffect
{
    int stat_id;
    int delta;
} PassiveEffect;

typedef struct PassiveNodeEffects
{
    int offset; /* into g_effects array */
    int count;  /* number of effects */
} PassiveNodeEffects;

static PassiveEffect g_effects[PASSIVE_MAX_EFFECTS_TOTAL];
static int g_effect_count = 0;
/* SoA mirrors */
static int g_effect_stat_ids[PASSIVE_MAX_EFFECTS_TOTAL];
static int g_effect_deltas[PASSIVE_MAX_EFFECTS_TOTAL];
static PassiveNodeEffects* g_node_effects = NULL; /* size = maze->base.node_count */
static unsigned char* g_unlocked = NULL;          /* bitset bytes per node (0/1) */
static int g_node_count = 0;
static int g_initialized = 0;
static double
    g_passive_stat_accum[512]; /* stat_id (index) -> total (supports fractional scaling) */
static const struct RogueProgressionMaze* g_maze_ref =
    NULL;                                             /* for keystone flag & future metadata */
static int g_keystone_category_counts[3] = {0, 0, 0}; /* offense, defense, utility */

/* Journal entry */
typedef struct PassiveJournalEntry
{
    int node_id;
    unsigned int ts;
} PassiveJournalEntry;
static PassiveJournalEntry* g_journal = NULL;
static int g_journal_count = 0;
static int g_journal_cap = 0;
static unsigned long long g_journal_hash = 1469598103934665603ull; /* FNV-1a */

static void journal_append(int node_id, unsigned int ts)
{
    if (g_journal_count == g_journal_cap)
    {
        int new_cap = g_journal_cap ? g_journal_cap * 2 : 32;
        PassiveJournalEntry* nre = (PassiveJournalEntry*) realloc(
            g_journal, sizeof(PassiveJournalEntry) * (size_t) new_cap);
        if (!nre)
            return;
        g_journal = nre;
        g_journal_cap = new_cap;
    }
    PassiveJournalEntry e;
    e.node_id = node_id;
    e.ts = ts;
    g_journal[g_journal_count++] = e;
    /* hash update */
    unsigned long long h = g_journal_hash;
    unsigned char* p = (unsigned char*) &e;
    for (size_t i = 0; i < sizeof e; i++)
    {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    g_journal_hash = h;
}

int rogue_progression_passives_init(const struct RogueProgressionMaze* maze)
{
    if (g_initialized)
        return 0;
    if (!maze)
        return -1;
    g_node_count = maze->base.node_count;
    if (g_node_count <= 0)
        return -1;
    g_maze_ref = maze;
    memset(g_keystone_category_counts, 0, sizeof g_keystone_category_counts);
    g_node_effects =
        (PassiveNodeEffects*) calloc((size_t) g_node_count, sizeof(PassiveNodeEffects));
    if (!g_node_effects)
        return -1;
    g_unlocked = (unsigned char*) calloc((size_t) g_node_count, 1);
    if (!g_unlocked)
    {
        free(g_node_effects);
        g_node_effects = NULL;
        return -1;
    }
    g_initialized = 1;
    g_effect_count = 0;
    memset(g_passive_stat_accum, 0, sizeof g_passive_stat_accum);
    g_journal_count = 0;
    g_journal_cap = 0;
    free(g_journal);
    g_journal = NULL;
    g_journal_hash = 1469598103934665603ull;
    return 0;
}

void rogue_progression_passives_shutdown(void)
{
    free(g_node_effects);
    g_node_effects = NULL;
    free(g_unlocked);
    g_unlocked = NULL;
    free(g_journal);
    g_journal = NULL;
    g_journal_count = 0;
    g_journal_cap = 0;
    g_initialized = 0;
    g_effect_count = 0;
    g_node_count = 0;
    g_maze_ref = NULL;
}

static int stat_code_to_id(const char* code)
{
    size_t count = 0;
    const RogueStatDef* defs = rogue_stat_def_all(&count);
    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(defs[i].code, code) == 0)
            return defs[i].id;
    }
    return -1;
}

#if defined(_MSC_VER)
#define ROGUE_STRTOK(str, delim, ctx) strtok_s((str), (delim), (ctx))
#else
#define ROGUE_STRTOK(str, delim, ctx) strtok_r((str), (delim), (ctx))
#endif

int rogue_progression_passives_load_dsl(const char* text)
{
    if (!g_initialized || !text)
        return -1; /* reset previous effects */
    g_effect_count = 0;
    if (g_node_effects)
        memset(g_node_effects, 0, sizeof(PassiveNodeEffects) * (size_t) g_node_count);
    const char* p = text;
    int line = 0;
    char linebuf[256];
    while (*p)
    {
        int len = 0;
        while (p[len] && p[len] != '\n' && len < 255)
        {
            linebuf[len] = p[len];
            len++;
        }
        linebuf[len] = '\0';
        if (p[len] == '\n')
            p += len + 1;
        else
            p += len;
        line++;
        /* trim */ char* s = linebuf;
        while (isspace((unsigned char) *s))
            s++;
        if (*s == '#' || *s == '\0')
            continue; /* comment/blank */
        /* tokenize */ char* ctx = NULL;
        char* tok = ROGUE_STRTOK(s, " \t", &ctx);
        if (!tok)
            continue;
        int node_id = atoi(tok);
        if (node_id < 0 || node_id >= g_node_count)
            continue;
        PassiveNodeEffects* pne = &g_node_effects[node_id];
        pne->offset = g_effect_count;
        pne->count = 0;
        char* eff;
        for (;;)
        {
            eff = ROGUE_STRTOK(NULL, " \t", &ctx);
            if (!eff)
                break;
            char* plus = strchr(eff, '+');
            if (!plus)
                continue;
            *plus = '\0';
            const char* scode = eff;
            int delta = atoi(plus + 1);
            int sid = stat_code_to_id(scode);
            if (sid < 0)
                continue;
            if (pne->count < PASSIVE_MAX_NODE_EFFECTS && g_effect_count < PASSIVE_MAX_EFFECTS_TOTAL)
            {
                g_effects[g_effect_count].stat_id = sid;
                g_effects[g_effect_count].delta = delta;
                g_effect_stat_ids[g_effect_count] = sid;
                g_effect_deltas[g_effect_count] = delta;
                g_effect_count++;
                pne->count++;
            }
        }
    }
    return 0;
}

static int classify_keystone_effect(const PassiveNodeEffects* pne)
{
    /* simplistic classification: offense if any damage/crit stat, defense if any resist/toughness,
     * else utility */
    for (int i = 0; i < pne->count; i++)
    {
        PassiveEffect* pe = &g_effects[pne->offset + i];
        int sid = pe->stat_id;
        if (sid == 300 || sid == 100 || sid == 101)
        {
            return 0;
        } /* offense */
        if ((sid >= 120 && sid <= 125) || sid == 104)
        {
            return 1;
        } /* defense */
    }
    return 2; /* utility */
}

int rogue_progression_passive_unlock(int node_id, unsigned int timestamp_ms, int level, int str,
                                     int dex, int intel, int vit)
{
    if (!g_initialized)
        return -1;
    if (node_id < 0 || node_id >= g_node_count)
        return -1;
    if (g_unlocked[node_id])
        return -2;
    /* Phase 3.6.3: gate unlocks using progression maze thresholds (level & attributes). */
    if (g_maze_ref && g_maze_ref->meta && node_id < g_maze_ref->base.node_count)
    {
        if (!rogue_progression_maze_node_unlockable(g_maze_ref, node_id, level, str, dex, intel,
                                                    vit))
        {
            /* Not unlockable yet; caller can retry later when requirements met. */
            return -1;
        }
    }
    g_unlocked[node_id] = 1;
    PassiveNodeEffects* pne = &g_node_effects[node_id];
    int is_keystone = 0;
    int kcat = -1;
    if (g_maze_ref && g_maze_ref->meta && node_id < g_maze_ref->base.node_count)
    {
        if (g_maze_ref->meta[node_id].flags & 0x4u)
        {
            is_keystone = 1;
            kcat = classify_keystone_effect(pne);
        }
    }
    double coeff = 1.0;
    if (is_keystone)
    {
        int kcount = ++g_keystone_category_counts[kcat];
        coeff = 1.0 / (1.0 + 0.15 * (double) (kcount - 1));
    }
    /* Use SoA mirrors to improve cache predictability */
    int off = pne->offset;
    int cnt = pne->count;
    for (int i = 0; i < cnt; i++)
    {
        int sid = g_effect_stat_ids[off + i];
        int delta = g_effect_deltas[off + i];
        if (sid >= 0 && sid < 512)
        {
            double d = (double) delta;
            if (is_keystone)
                d *= coeff;
            g_passive_stat_accum[sid] += d;
        }
    }
    journal_append(node_id, timestamp_ms);
    (void) level;
    (void) str;
    (void) dex;
    (void) intel;
    (void) vit;
    return pne->count ? 0 : -3;
}

int rogue_progression_passives_stat_total(int stat_id)
{
    if (stat_id < 0 || stat_id >= 512)
        return 0;
    double v = g_passive_stat_accum[stat_id];
    if (v < 0)
        v = 0;
    return (int) (v + 0.5);
}
int rogue_progression_passives_is_unlocked(int node_id)
{
    if (node_id < 0 || node_id >= g_node_count)
        return 0;
    return g_unlocked && g_unlocked[node_id] ? 1 : 0;
}
unsigned long long rogue_progression_passives_journal_hash(void) { return g_journal_hash; }

int rogue_progression_passives_keystone_count_offense(void)
{
    return g_keystone_category_counts[0];
}
int rogue_progression_passives_keystone_count_defense(void)
{
    return g_keystone_category_counts[1];
}
int rogue_progression_passives_keystone_count_utility(void)
{
    return g_keystone_category_counts[2];
}

int rogue_progression_passives_reload(const struct RogueProgressionMaze* maze, const char* text,
                                      int level, int str, int dex, int intel, int vit)
{
    if (!g_initialized)
        return -1;
    g_maze_ref = maze;
    memset(g_keystone_category_counts, 0,
           sizeof g_keystone_category_counts); /* capture prior journal */
    int saved_count = g_journal_count;
    PassiveJournalEntry* saved =
        (PassiveJournalEntry*) malloc(sizeof(PassiveJournalEntry) * (size_t) saved_count);
    if (!saved)
        return -1;
    memcpy(saved, g_journal, sizeof(PassiveJournalEntry) * (size_t) saved_count);
    unsigned long long old_hash = g_journal_hash;
    /* reset unlock state & accumulators (keep journal separate) */ memset(g_unlocked, 0,
                                                                           (size_t) g_node_count);
    memset(g_passive_stat_accum, 0, sizeof g_passive_stat_accum);
    g_journal_count = 0;
    g_journal_cap = 0;
    free(g_journal);
    g_journal = NULL;
    g_journal_hash = 1469598103934665603ull;
    if (rogue_progression_passives_load_dsl(text) < 0)
    {
        free(saved);
        return -1;
    }
    for (int i = 0; i < saved_count; i++)
    {
        PassiveJournalEntry* e = &saved[i];
        rogue_progression_passive_unlock(e->node_id, e->ts, level, str, dex, intel, vit);
    }
    free(saved);
    return (g_journal_hash == old_hash) ? 0 : -1;
}
