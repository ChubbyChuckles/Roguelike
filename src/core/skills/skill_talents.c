#include "skill_talents.h"
#include "../app/app_state.h"
#include "../progression/progression_maze.h"
#include <stdlib.h>
#include <string.h>

/* Capture preview context so commit can reuse unlock attributes. */
typedef struct PreviewUnlockEntry
{
    int node_id;
    int level, str, dex, intel, vit;
} PreviewUnlockEntry;

typedef struct TalentState
{
    const struct RogueProgressionMaze* maze;
    unsigned char* unlocked; /* bitset per node */
    int node_count;
    int any_threshold; /* open allocation threshold (ANY) */
    RogueTalentModifier* mods;
    int mod_count;
    int mod_cap;
    unsigned long long hash; /* FNV-1a over unlock journal (node ids) */
    /* DAG model (1B.1) */
    unsigned char* node_types;  /* RogueTalentNodeType per node (u8) */
    int* skill_unlock_for_node; /* per node: skill id or -1 */
    int* prereq_counts;         /* per node count */
    int** prereqs;              /* per node dynamic arrays of prereq node ids */
    /* Respec support: store unlock order as a simple journal (stack). */
    int* journal;
    int journal_len;
    int journal_cap;
    /* Preview (staged unlocks) */
    unsigned char in_preview;
    unsigned char* preview_unlocked; /* bitset overlay */
    PreviewUnlockEntry* preview_journal;
    int preview_journal_len;
    int preview_journal_cap;
} TalentState;

static TalentState g_tal = {0};

int rogue_talents_init(const struct RogueProgressionMaze* maze)
{
    if (!maze)
        return -1;
    memset(&g_tal, 0, sizeof g_tal);
    g_tal.maze = maze;
    g_tal.node_count = maze->base.node_count;
    g_tal.unlocked = (unsigned char*) calloc((size_t) g_tal.node_count, 1);
    g_tal.node_types = (unsigned char*) calloc((size_t) g_tal.node_count, 1);
    g_tal.skill_unlock_for_node = (int*) malloc(sizeof(int) * (size_t) g_tal.node_count);
    g_tal.prereq_counts = (int*) calloc((size_t) g_tal.node_count, sizeof(int));
    g_tal.prereqs = (int**) calloc((size_t) g_tal.node_count, sizeof(int*));
    g_tal.any_threshold = 0;
    g_tal.hash = 1469598103934665603ull;
    if (!g_tal.unlocked || !g_tal.node_types || !g_tal.skill_unlock_for_node ||
        !g_tal.prereq_counts || !g_tal.prereqs)
        return -1;
    for (int i = 0; i < g_tal.node_count; ++i)
        g_tal.skill_unlock_for_node[i] = -1;
    g_tal.journal_cap = 64;
    g_tal.journal = (int*) malloc(sizeof(int) * (size_t) g_tal.journal_cap);
    if (!g_tal.journal)
        return -1;
    g_tal.journal_len = 0;
    g_tal.preview_unlocked = NULL;
    g_tal.preview_journal = NULL;
    g_tal.preview_journal_len = 0;
    g_tal.preview_journal_cap = 0;
    return 0;
}
void rogue_talents_shutdown(void)
{
    free(g_tal.unlocked);
    g_tal.unlocked = NULL;
    if (g_tal.node_types)
        free(g_tal.node_types);
    g_tal.node_types = NULL;
    if (g_tal.skill_unlock_for_node)
        free(g_tal.skill_unlock_for_node);
    g_tal.skill_unlock_for_node = NULL;
    if (g_tal.prereqs)
    {
        for (int i = 0; i < g_tal.node_count; ++i)
            if (g_tal.prereqs[i])
                free(g_tal.prereqs[i]);
        free(g_tal.prereqs);
    }
    g_tal.prereqs = NULL;
    if (g_tal.prereq_counts)
        free(g_tal.prereq_counts);
    g_tal.prereq_counts = NULL;
    free(g_tal.mods);
    g_tal.mods = NULL;
    g_tal.mod_count = g_tal.mod_cap = 0;
    g_tal.maze = NULL;
    g_tal.node_count = 0;
    free(g_tal.journal);
    g_tal.journal = NULL;
    g_tal.journal_len = g_tal.journal_cap = 0;
    if (g_tal.preview_unlocked)
        free(g_tal.preview_unlocked);
    g_tal.preview_unlocked = NULL;
    if (g_tal.preview_journal)
        free(g_tal.preview_journal);
    g_tal.preview_journal = NULL;
    g_tal.preview_journal_len = g_tal.preview_journal_cap = 0;
    g_tal.in_preview = 0;
}

void rogue_talents_set_any_threshold(int threshold) { g_tal.any_threshold = threshold; }

void rogue_talents_set_node_type(int node_id, RogueTalentNodeType type)
{
    if (node_id >= 0 && node_id < g_tal.node_count && g_tal.node_types)
        g_tal.node_types[node_id] = (unsigned char) type;
}

int rogue_talents_set_prerequisites(int node_id, const int* prereq_node_ids, int count)
{
    if (!g_tal.prereqs || !g_tal.prereq_counts)
        return 0;
    if (node_id < 0 || node_id >= g_tal.node_count || count < 0)
        return 0;
    if (g_tal.prereqs[node_id])
    {
        free(g_tal.prereqs[node_id]);
        g_tal.prereqs[node_id] = NULL;
        g_tal.prereq_counts[node_id] = 0;
    }
    if (count == 0)
        return 1;
    int* arr = (int*) malloc(sizeof(int) * (size_t) count);
    if (!arr)
        return 0;
    for (int i = 0; i < count; ++i)
        arr[i] = prereq_node_ids[i];
    g_tal.prereqs[node_id] = arr;
    g_tal.prereq_counts[node_id] = count;
    return 1;
}

int rogue_talents_set_skill_unlock(int node_id, int skill_id)
{
    if (!g_tal.skill_unlock_for_node)
        return 0;
    if (node_id < 0 || node_id >= g_tal.node_count)
        return 0;
    g_tal.skill_unlock_for_node[node_id] = skill_id;
    return 1;
}

int rogue_talents_is_skill_unlocked(int skill_id)
{
    if (skill_id < 0 || !g_tal.skill_unlock_for_node || !g_tal.unlocked)
        return 0;
    for (int n = 0; n < g_tal.node_count; ++n)
        if (g_tal.unlocked[n] && g_tal.skill_unlock_for_node[n] == skill_id)
            return 1;
    return 0;
}

int rogue_talents_unlocked_count(void)
{
    int total = 0;
    if (!g_tal.unlocked)
        return 0;
    for (int i = 0; i < g_tal.node_count; ++i)
        if (g_tal.unlocked[i])
            total++;
    return total;
}

int rogue_talents_register_modifier(const RogueTalentModifier* mod)
{
    if (!mod || mod->node_id < 0 || mod->skill_id < 0)
        return 0;
    if (g_tal.mod_count == g_tal.mod_cap)
    {
        int nc = g_tal.mod_cap ? g_tal.mod_cap * 2 : 64;
        void* np = realloc(g_tal.mods, sizeof(RogueTalentModifier) * (size_t) nc);
        if (!np)
            return 0;
        g_tal.mods = (RogueTalentModifier*) np;
        g_tal.mod_cap = nc;
    }
    g_tal.mods[g_tal.mod_count++] = *mod;
    return 1;
}

static int is_unlocked_live(int node_id)
{
    if (node_id < 0 || node_id >= g_tal.node_count || !g_tal.unlocked)
        return 0;
    return g_tal.unlocked[node_id] ? 1 : 0;
}
int rogue_talents_is_unlocked(int node_id) { return is_unlocked_live(node_id); }

static int is_unlocked_effective(int node_id)
{
    if (node_id < 0 || node_id >= g_tal.node_count)
        return 0;
    if (g_tal.in_preview && g_tal.preview_unlocked)
        return g_tal.unlocked[node_id] || g_tal.preview_unlocked[node_id];
    return g_tal.unlocked && g_tal.unlocked[node_id];
}

static int total_unlocked_effective(void)
{
    int total = 0;
    for (int i = 0; i < g_tal.node_count; ++i)
        if (is_unlocked_effective(i))
            total++;
    return total;
}

int rogue_talents_can_unlock(int node_id, int level, int str, int dex, int intel, int vit)
{
    if (!g_tal.maze)
        return 0;
    if (node_id < 0 || node_id >= g_tal.node_count)
        return 0;
    if (!rogue_progression_maze_node_unlockable(g_tal.maze, node_id, level, str, dex, intel, vit))
        return 0;
    /* Open allocation: allow if any_threshold == 0 -> use prerequisites/adjacency; if >0, allow
     * when total unlocked >= threshold regardless of graph locality. */
    int unlocked_total = total_unlocked_effective();
    if (g_tal.any_threshold > 0 && unlocked_total >= g_tal.any_threshold)
        return 1;
    /* If explicit prerequisites were provided for this node, require ALL of them. */
    if (g_tal.prereq_counts && g_tal.prereq_counts[node_id] > 0 && g_tal.prereqs &&
        g_tal.prereqs[node_id])
    {
        for (int i = 0; i < g_tal.prereq_counts[node_id]; ++i)
        {
            int pre = g_tal.prereqs[node_id][i];
            if (pre < 0 || pre >= g_tal.node_count || !is_unlocked_effective(pre))
                return 0;
        }
        return 1; /* all prerequisites satisfied */
    }
    /* adjacency check: require at least one neighbor already unlocked unless it's node 0 (root) */
    if (node_id == 0)
        return 1;
    const RogueProgressionMazeNodeMeta* meta = &g_tal.maze->meta[node_id];
    for (int k = 0; k < meta->adj_count; ++k)
    {
        int nb = g_tal.maze->adjacency[meta->adj_start + k];
        if (nb >= 0 && nb < g_tal.node_count && is_unlocked_effective(nb))
            return 1;
    }
    return 0;
}

static void hash_fold_u32(unsigned long long* h, unsigned int v)
{
    unsigned char b[4];
    b[0] = (unsigned char) (v & 0xFF);
    b[1] = (unsigned char) ((v >> 8) & 0xFF);
    b[2] = (unsigned char) ((v >> 16) & 0xFF);
    b[3] = (unsigned char) ((v >> 24) & 0xFF);
    for (int i = 0; i < 4; ++i)
    {
        *h ^= b[i];
        *h *= 1099511628211ull;
    }
}

static void journal_push(int node_id)
{
    if (g_tal.journal_len == g_tal.journal_cap)
    {
        int nc = g_tal.journal_cap ? g_tal.journal_cap * 2 : 64;
        int* np = (int*) realloc(g_tal.journal, sizeof(int) * (size_t) nc);
        if (np)
        {
            g_tal.journal = np;
            g_tal.journal_cap = nc;
        }
    }
    if (g_tal.journal && g_tal.journal_len < g_tal.journal_cap)
        g_tal.journal[g_tal.journal_len++] = node_id;
}

static void preview_journal_push(int node_id, int level, int str, int dex, int intel, int vit)
{
    if (g_tal.preview_journal_len == g_tal.preview_journal_cap)
    {
        int nc = g_tal.preview_journal_cap ? g_tal.preview_journal_cap * 2 : 64;
        PreviewUnlockEntry* np = (PreviewUnlockEntry*) realloc(
            g_tal.preview_journal, sizeof(PreviewUnlockEntry) * (size_t) nc);
        if (np)
        {
            g_tal.preview_journal = np;
            g_tal.preview_journal_cap = nc;
        }
    }
    if (g_tal.preview_journal && g_tal.preview_journal_len < g_tal.preview_journal_cap)
    {
        g_tal.preview_journal[g_tal.preview_journal_len].node_id = node_id;
        g_tal.preview_journal[g_tal.preview_journal_len].level = level;
        g_tal.preview_journal[g_tal.preview_journal_len].str = str;
        g_tal.preview_journal[g_tal.preview_journal_len].dex = dex;
        g_tal.preview_journal[g_tal.preview_journal_len].intel = intel;
        g_tal.preview_journal[g_tal.preview_journal_len].vit = vit;
        g_tal.preview_journal_len++;
    }
}

int rogue_talents_unlock(int node_id, unsigned int timestamp_ms, int level, int str, int dex,
                         int intel, int vit)
{
    (void) timestamp_ms; /* reserved */
    if (!g_tal.unlocked)
        return 0;
    if (!rogue_talents_can_unlock(node_id, level, str, dex, intel, vit))
        return 0;
    if (is_unlocked_live(node_id))
        return 0;
    /* Determine point cost from progression meta (default 1 if absent). */
    int cost_points = 1;
    if (g_tal.maze && node_id >= 0 && node_id < g_tal.node_count && g_tal.maze->meta)
        cost_points =
            g_tal.maze->meta[node_id].cost_points > 0 ? g_tal.maze->meta[node_id].cost_points : 1;
    if (g_app.talent_points < cost_points)
        return 0;
    g_tal.unlocked[node_id] = 1;
    g_app.talent_points -= cost_points;
    hash_fold_u32(&g_tal.hash, (unsigned int) node_id);
    journal_push(node_id);
    return 1;
}

int rogue_talents_serialize(void* buffer, size_t buf_size)
{
    if (!g_tal.unlocked)
        return -1;
    size_t bytes = (size_t) g_tal.node_count;
    if (buf_size < bytes + 4)
        return -1;
    unsigned char* p = (unsigned char*) buffer;
    p[0] = 1; /* version */
    p[1] = (unsigned char) (g_tal.node_count & 0xFF);
    p[2] = (unsigned char) ((g_tal.node_count >> 8) & 0xFF);
    p[3] = (unsigned char) ((g_tal.node_count >> 16) & 0xFF);
    memcpy(p + 4, g_tal.unlocked, bytes);
    return (int) (bytes + 4);
}
int rogue_talents_deserialize(const void* buffer, size_t buf_size)
{
    if (!g_tal.unlocked || buf_size < 4)
        return -1;
    const unsigned char* p = (const unsigned char*) buffer;
    unsigned int ver = p[0];
    (void) ver;
    int count = (int) p[1] | ((int) p[2] << 8) | ((int) p[3] << 16);
    size_t bytes = (size_t) count;
    if (count != g_tal.node_count || buf_size < bytes + 4)
        return -1;
    memcpy(g_tal.unlocked, p + 4, bytes);
    /* recompute hash deterministically */
    g_tal.hash = 1469598103934665603ull;
    g_tal.journal_len = 0;
    for (int i = 0; i < g_tal.node_count; ++i)
        if (g_tal.unlocked[i])
        {
            hash_fold_u32(&g_tal.hash, (unsigned int) i);
            journal_push(i);
        }
    return (int) (bytes + 4);
}

unsigned long long rogue_talents_hash(void) { return g_tal.hash; }

int rogue_skill_get_effective_def(int id, struct RogueSkillDef* out)
{
    const RogueSkillDef* base = rogue_skill_get_def(id);
    if (!base || !out)
        return 0;
    *out = *base; /* copy */
    for (int i = 0; i < g_tal.mod_count; ++i)
    {
        RogueTalentModifier* m = &g_tal.mods[i];
        if (m->skill_id != id)
            continue;
        if (m->node_id < 0 || m->node_id >= g_tal.node_count)
            continue;
        if (!g_tal.unlocked[m->node_id])
            continue;
        if (m->cd_scalar > 0.0f)
            out->base_cooldown_ms *= m->cd_scalar;
        out->action_point_cost += m->ap_delta;
        if (out->action_point_cost < 0)
            out->action_point_cost = 0;
        out->tags |= m->add_tags;
        out->max_charges += m->charges_delta;
        if (m->add_effect_spec_id > 0)
            out->effect_spec_id = m->add_effect_spec_id;
        /* proc chance reserved for future pipeline */
    }
    return 1;
}

int rogue_talents_respec_last(int n)
{
    if (n <= 0 || !g_tal.unlocked || !g_tal.journal)
        return 0;
    int undone = 0;
    while (n-- > 0 && g_tal.journal_len > 0)
    {
        int node_id = g_tal.journal[--g_tal.journal_len];
        if (node_id >= 0 && node_id < g_tal.node_count && g_tal.unlocked[node_id])
        {
            g_tal.unlocked[node_id] = 0;
            g_app.talent_points += 1;
            undone++;
        }
    }
    /* Recompute hash from current unlocked set */
    g_tal.hash = 1469598103934665603ull;
    for (int i = 0; i < g_tal.node_count; ++i)
        if (g_tal.unlocked[i])
            hash_fold_u32(&g_tal.hash, (unsigned int) i);
    return undone;
}

int rogue_talents_full_respec(void)
{
    if (!g_tal.unlocked)
        return 0;
    int refunded = 0;
    for (int i = 0; i < g_tal.node_count; ++i)
    {
        if (g_tal.unlocked[i])
        {
            g_tal.unlocked[i] = 0;
            refunded++;
        }
    }
    g_app.talent_points += refunded;
    g_tal.journal_len = 0;
    g_tal.hash = 1469598103934665603ull;
    return refunded;
}

int rogue_talents_preview_begin(void)
{
    if (g_tal.in_preview)
        return 0;
    g_tal.preview_unlocked = (unsigned char*) calloc((size_t) g_tal.node_count, 1);
    if (!g_tal.preview_unlocked)
        return 0;
    g_tal.preview_journal_cap = 64;
    g_tal.preview_journal = (PreviewUnlockEntry*) malloc(sizeof(PreviewUnlockEntry) *
                                                         (size_t) g_tal.preview_journal_cap);
    if (!g_tal.preview_journal)
    {
        free(g_tal.preview_unlocked);
        g_tal.preview_unlocked = NULL;
        return 0;
    }
    g_tal.preview_journal_len = 0;
    g_tal.in_preview = 1;
    return 1;
}

int rogue_talents_preview_unlock(int node_id, int level, int str, int dex, int intel, int vit)
{
    if (!g_tal.in_preview || !g_tal.preview_unlocked)
        return 0;
    if (node_id < 0 || node_id >= g_tal.node_count)
        return 0;
    if (is_unlocked_effective(node_id))
        return 0;
    /* Temporarily mark as unlocked in preview for rule evaluation of subsequent unlocks. */
    if (!rogue_talents_can_unlock(node_id, level, str, dex, intel, vit))
        return 0;
    g_tal.preview_unlocked[node_id] = 1;
    preview_journal_push(node_id, level, str, dex, intel, vit);
    return 1;
}

int rogue_talents_preview_cancel(void)
{
    if (!g_tal.in_preview)
        return 0;
    if (g_tal.preview_unlocked)
        free(g_tal.preview_unlocked);
    g_tal.preview_unlocked = NULL;
    if (g_tal.preview_journal)
        free(g_tal.preview_journal);
    g_tal.preview_journal = NULL;
    g_tal.preview_journal_len = g_tal.preview_journal_cap = 0;
    g_tal.in_preview = 0;
    return 1;
}

int rogue_talents_preview_commit(unsigned int timestamp_ms)
{
    if (!g_tal.in_preview || !g_tal.preview_unlocked)
        return 0;
    int committed = 0;
    for (int i = 0; i < g_tal.preview_journal_len; ++i)
    {
        int node_id = g_tal.preview_journal[i].node_id;
        if (g_tal.preview_unlocked[node_id])
        {
            /* Re-validate and apply to live state (spend points). */
            PreviewUnlockEntry* e = &g_tal.preview_journal[i];
            if (rogue_talents_unlock(node_id, timestamp_ms, e->level, e->str, e->dex, e->intel,
                                     e->vit))
                committed++;
        }
    }
    rogue_talents_preview_cancel();
    return committed;
}
