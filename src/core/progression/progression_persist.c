/* Phase 12: Versioned progression persistence & migration */
#include "progression_persist.h"
#include "../save_manager.h"
#include "progression_attributes.h"
#include "progression_passives.h"
#include "progression_stats.h"
#include <string.h>

#define ROGUE_PROG_SAVE_VERSION 3u

typedef struct ProgHeaderV1
{
    uint32_t version;
    uint32_t level;
    uint64_t xp_total;
    uint32_t attr_str, attr_dex, attr_vit, attr_int;
    uint32_t unspent_pts;
    uint32_t respec_tokens;
    uint64_t attr_journal_hash;
    uint64_t passive_journal_hash;
    uint32_t passive_entry_count;
} ProgHeaderV1;
typedef struct PassiveEntryDisk
{
    int32_t node_id;
    uint32_t timestamp_ms;
} PassiveEntryDisk;
typedef struct ProgHeaderV2
{
    uint32_t version;
    uint32_t level;
    uint64_t xp_total;
    uint64_t stat_registry_fp;
    uint32_t maze_node_count;
    uint32_t attr_str, attr_dex, attr_vit, attr_int;
    uint32_t unspent_pts;
    uint32_t respec_tokens;
    uint64_t attr_journal_hash;
    uint64_t passive_journal_hash;
    uint32_t passive_entry_count;
} ProgHeaderV2;
typedef struct ProgHeaderV3
{
    uint32_t version;
    uint32_t level;
    uint64_t xp_total;
    uint64_t stat_registry_fp;
    uint32_t maze_node_count;
    uint32_t attr_str, attr_dex, attr_vit, attr_int;
    uint32_t unspent_pts;
    uint32_t respec_tokens;
    uint64_t attr_journal_hash;
    uint64_t passive_journal_hash;
    uint32_t passive_entry_count;
    uint32_t attr_op_count;
} ProgHeaderV3;

static unsigned long long g_chain_hash = 0;
static unsigned int g_last_migration_flags = 0;
static unsigned long long fold64(unsigned long long h, unsigned long long v)
{
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}
unsigned long long rogue_progression_persist_chain_hash(void) { return g_chain_hash; }
unsigned int rogue_progression_persist_last_migration_flags(void) { return g_last_migration_flags; }
extern struct AppState
{
    int level;
    unsigned long long xp_total_accum;
} g_app;
extern RogueAttributeState g_attr_state;
extern unsigned long long rogue_progression_passives_journal_hash(void);
int rogue_attr_journal_count(void);
int rogue_attr_journal_entry(int i, char* code_out, int* kind_out);
void rogue_attr__journal_append(char code, int kind);

/* Passive unlock journal interop (Phase 12.2) */
extern int rogue_progression_passives_is_unlocked(int node_id);
/* We directly walk internal journal by replaying from unlock states; since internal journal isn't
 * exposed, we persist only unlocked nodes with a synthetic timestamp (order preserved by node id).
 */
static void write_unlocked_passives(FILE* f, uint32_t* count_out)
{
    if (!f || !count_out)
    {
        return;
    }
    *count_out = 0; /* naive: iterate possible node ids (bounded) */
    for (int nid = 0; nid < 4096; nid++)
    {
        if (rogue_progression_passives_is_unlocked(nid))
        {
            PassiveEntryDisk e;
            e.node_id = nid;
            e.timestamp_ms = (uint32_t) (nid & 0xFFFFFFFFu);
            fwrite(&e, sizeof e, 1, f);
            (*count_out)++;
        }
    }
}
extern int rogue_progression_passive_unlock(int node_id, unsigned int timestamp_ms, int level,
                                            int str, int dex, int intel, int vit);
static int read_unlocked_passives(FILE* f, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        PassiveEntryDisk e;
        if (fread(&e, sizeof e, 1, f) != 1)
            return -1;
        rogue_progression_passive_unlock(e.node_id, e.timestamp_ms, g_app.level,
                                         g_attr_state.strength, g_attr_state.dexterity,
                                         g_attr_state.intelligence, g_attr_state.vitality);
    }
    return 0;
}

/* Attribute operation journal serialization (Phase 12.3) */
static int write_attr_ops(FILE* f, uint32_t* op_count_out)
{
    if (!f || !op_count_out)
        return -1;
    *op_count_out = (uint32_t) rogue_attr_journal_count();
    for (int i = 0; i < rogue_attr_journal_count(); i++)
    {
        char code = 'S';
        int kind = 0;
        if (rogue_attr_journal_entry(i, &code, &kind) != 0)
            return -2;
        unsigned char rec[2];
        rec[0] = (unsigned char) code;
        rec[1] = (unsigned char) kind;
        if (fwrite(rec, 1, 2, f) != 2)
            return -3;
    }
    return 0;
}
static int read_attr_ops(FILE* f, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        unsigned char rec[2];
        if (fread(rec, 1, 2, f) != 2)
            return -1;
        char code = (char) rec[0];
        int kind = (int) rec[1];
        rogue_attr__journal_append(code, kind);
        g_last_migration_flags |= ROGUE_PROG_MIG_ATTR_REPLAY;
    }
    return 0;
}
int rogue_progression_persist_write(FILE* f)
{
    if (!f)
        return -1;
    g_chain_hash = 0xcbf29ce484222325ULL;
    g_last_migration_flags = 0;
    ProgHeaderV3 h;
    memset(&h, 0, sizeof h);
    h.version = ROGUE_PROG_SAVE_VERSION;
    h.level = (uint32_t) g_app.level;
    h.xp_total = g_app.xp_total_accum;
    h.stat_registry_fp = rogue_stat_registry_fingerprint();
    h.maze_node_count = 0;
    h.attr_str = (uint32_t) g_attr_state.strength;
    h.attr_dex = (uint32_t) g_attr_state.dexterity;
    h.attr_vit = (uint32_t) g_attr_state.vitality;
    h.attr_int = (uint32_t) g_attr_state.intelligence;
    h.unspent_pts = (uint32_t) g_attr_state.spent_points;
    h.respec_tokens = (uint32_t) g_attr_state.respec_tokens;
    h.attr_journal_hash = g_attr_state.journal_hash;
    h.passive_journal_hash = rogue_progression_passives_journal_hash();
    long after_header = 0; /* reserve space for header */
    if (fwrite(&h, sizeof h, 1, f) != 1)
        return -2; /* passive entries */
    uint32_t entry_count = 0;
    write_unlocked_passives(f, &entry_count); /* attribute ops */
    uint32_t attr_ops = 0;
    if (write_attr_ops(f, &attr_ops) != 0)
        return -3; /* rewrite header with counts */
    h.passive_entry_count = entry_count;
    h.attr_op_count = attr_ops;
    after_header = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fwrite(&h, sizeof h, 1, f) != 1)
        return -4;
    fseek(f, after_header, SEEK_SET);
    g_chain_hash = fold64(g_chain_hash, h.version);
    g_chain_hash = fold64(g_chain_hash, h.level);
    g_chain_hash = fold64(g_chain_hash, h.xp_total);
    g_chain_hash = fold64(g_chain_hash, h.stat_registry_fp);
    g_chain_hash = fold64(g_chain_hash, h.passive_journal_hash);
    return 0;
}
int rogue_progression_persist_read(FILE* f)
{
    if (!f)
        return -1;
    g_chain_hash = 0xcbf29ce484222325ULL;
    g_last_migration_flags = 0;
    long start = ftell(f);
    uint32_t version = 0;
    if (fread(&version, sizeof version, 1, f) != 1)
        return -2;
    fseek(f, start, SEEK_SET);
    if (version == 1)
    {
        ProgHeaderV1 h;
        if (fread(&h, sizeof h, 1, f) != 1)
            return -3;
        g_app.level = (int) h.level;
        g_app.xp_total_accum = h.xp_total;
        g_attr_state.strength = (int) h.attr_str;
        g_attr_state.dexterity = (int) h.attr_dex;
        g_attr_state.vitality = (int) h.attr_vit;
        g_attr_state.intelligence = (int) h.attr_int;
        g_attr_state.spent_points = (int) h.unspent_pts;
        g_attr_state.respec_tokens = (int) h.respec_tokens;
        g_attr_state.journal_hash = h.attr_journal_hash;
        if (h.passive_journal_hash != rogue_progression_passives_journal_hash())
            g_last_migration_flags |= ROGUE_PROG_MIG_STAT_REG_CHANGED;
        if (read_unlocked_passives(f, h.passive_entry_count) != 0)
            return -4;
        g_chain_hash = fold64(g_chain_hash, h.version);
        g_chain_hash = fold64(g_chain_hash, h.level);
        g_chain_hash = fold64(g_chain_hash, h.xp_total);
        g_chain_hash = fold64(g_chain_hash, 0ULL);
        g_chain_hash = fold64(g_chain_hash, h.passive_journal_hash);
    }
    else if (version == 2)
    {
        ProgHeaderV2 h;
        if (fread(&h, sizeof h, 1, f) != 1)
            return -5;
        g_app.level = (int) h.level;
        g_app.xp_total_accum = h.xp_total;
        g_attr_state.strength = (int) h.attr_str;
        g_attr_state.dexterity = (int) h.attr_dex;
        g_attr_state.vitality = (int) h.attr_vit;
        g_attr_state.intelligence = (int) h.attr_int;
        g_attr_state.spent_points = (int) h.unspent_pts;
        g_attr_state.respec_tokens = (int) h.respec_tokens;
        g_attr_state.journal_hash = h.attr_journal_hash;
        unsigned long long cur_fp = rogue_stat_registry_fingerprint();
        if (cur_fp != h.stat_registry_fp)
            g_last_migration_flags |= ROGUE_PROG_MIG_STAT_REG_CHANGED;
        if (read_unlocked_passives(f, h.passive_entry_count) != 0)
            return -6;
        g_chain_hash = fold64(g_chain_hash, h.version);
        g_chain_hash = fold64(g_chain_hash, h.level);
        g_chain_hash = fold64(g_chain_hash, h.xp_total);
        g_chain_hash = fold64(g_chain_hash, h.stat_registry_fp);
        g_chain_hash = fold64(g_chain_hash, h.passive_journal_hash);
    }
    else if (version == 3)
    {
        ProgHeaderV3 h;
        if (fread(&h, sizeof h, 1, f) != 1)
            return -8;
        g_app.level = (int) h.level;
        g_app.xp_total_accum = h.xp_total;
        g_attr_state.strength = (int) h.attr_str;
        g_attr_state.dexterity = (int) h.attr_dex;
        g_attr_state.vitality = (int) h.attr_vit;
        g_attr_state.intelligence = (int) h.attr_int;
        g_attr_state.spent_points = (int) h.unspent_pts;
        g_attr_state.respec_tokens = (int) h.respec_tokens;
        g_attr_state.journal_hash = h.attr_journal_hash;
        unsigned long long cur_fp = rogue_stat_registry_fingerprint();
        if (cur_fp != h.stat_registry_fp)
            g_last_migration_flags |= ROGUE_PROG_MIG_STAT_REG_CHANGED;
        if (read_unlocked_passives(f, h.passive_entry_count) != 0)
            return -9;
        if (read_attr_ops(f, h.attr_op_count) != 0)
            return -10;
        g_chain_hash = fold64(g_chain_hash, h.version);
        g_chain_hash = fold64(g_chain_hash, h.level);
        g_chain_hash = fold64(g_chain_hash, h.xp_total);
        g_chain_hash = fold64(g_chain_hash, h.stat_registry_fp);
        g_chain_hash = fold64(g_chain_hash, h.passive_journal_hash);
    }
    else
    {
        return -7;
    }
    return 0;
}
static int save_component_write(FILE* f) { return rogue_progression_persist_write(f); }
static int save_component_read(FILE* f, size_t size)
{
    (void) size;
    return rogue_progression_persist_read(f);
}
int rogue_progression_persist_register(void)
{
    RogueSaveComponent comp;
    memset(&comp, 0, sizeof comp);
    comp.id = 27;
    comp.name = "progression";
    comp.write_fn = save_component_write;
    comp.read_fn = save_component_read;
    rogue_save_manager_register(&comp);
    return 0;
}
void rogue_progression_persist_reset_state_for_tests(void)
{
    g_chain_hash = 0;
    g_last_migration_flags = 0;
}
unsigned int rogue_progression_persist_version(void) { return ROGUE_PROG_SAVE_VERSION; }
