#include "skill_talents.h"
#include "../app/app_state.h"
#include "../progression/progression_maze.h"
#include <stdlib.h>
#include <string.h>

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
    g_tal.any_threshold = 0;
    g_tal.hash = 1469598103934665603ull;
    return g_tal.unlocked ? 0 : -1;
}
void rogue_talents_shutdown(void)
{
    free(g_tal.unlocked);
    g_tal.unlocked = NULL;
    free(g_tal.mods);
    g_tal.mods = NULL;
    g_tal.mod_count = g_tal.mod_cap = 0;
    g_tal.maze = NULL;
    g_tal.node_count = 0;
}

void rogue_talents_set_any_threshold(int threshold) { g_tal.any_threshold = threshold; }

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

int rogue_talents_is_unlocked(int node_id)
{
    if (node_id < 0 || node_id >= g_tal.node_count || !g_tal.unlocked)
        return 0;
    return g_tal.unlocked[node_id] ? 1 : 0;
}

int rogue_talents_can_unlock(int node_id, int level, int str, int dex, int intel, int vit)
{
    if (!g_tal.maze)
        return 0;
    if (node_id < 0 || node_id >= g_tal.node_count)
        return 0;
    if (!rogue_progression_maze_node_unlockable(g_tal.maze, node_id, level, str, dex, intel, vit))
        return 0;
    /* Open allocation: allow if any_threshold == 0 -> must be adjacent; if >0, allow when total
     * unlocked >= threshold. */
    int unlocked_total = 0;
    for (int i = 0; i < g_tal.node_count; ++i)
        if (g_tal.unlocked && g_tal.unlocked[i])
            unlocked_total++;
    if (g_tal.any_threshold > 0 && unlocked_total >= g_tal.any_threshold)
        return 1;
    /* adjacency check: require at least one neighbor already unlocked unless it's node 0 (root) */
    if (node_id == 0)
        return 1;
    const RogueProgressionMazeNodeMeta* meta = &g_tal.maze->meta[node_id];
    for (int k = 0; k < meta->adj_count; ++k)
    {
        int nb = g_tal.maze->adjacency[meta->adj_start + k];
        if (nb >= 0 && nb < g_tal.node_count && g_tal.unlocked[nb])
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

int rogue_talents_unlock(int node_id, unsigned int timestamp_ms, int level, int str, int dex,
                         int intel, int vit)
{
    (void) timestamp_ms; /* reserved */
    if (!g_tal.unlocked)
        return 0;
    if (!rogue_talents_can_unlock(node_id, level, str, dex, intel, vit))
        return 0;
    if (g_tal.unlocked[node_id])
        return 0;
    if (g_app.talent_points <= 0)
        return 0;
    g_tal.unlocked[node_id] = 1;
    g_app.talent_points -= 1;
    hash_fold_u32(&g_tal.hash, (unsigned int) node_id);
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
    for (int i = 0; i < g_tal.node_count; ++i)
        if (g_tal.unlocked[i])
            hash_fold_u32(&g_tal.hash, (unsigned int) i);
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
