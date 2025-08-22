#include "core/crafting/gathering.h"
#include "core/crafting/material_registry.h"
#include "core/loot/loot_item_defs.h"
#include "core/path_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RogueGatherNodeDef g_defs[ROGUE_GATHER_NODE_CAP];
static int g_def_count = 0;
static RogueGatherNodeInstance g_nodes[ROGUE_GATHER_NODE_CAP];
static int g_node_count = 0;
static int g_player_tool_tier = 0;
static unsigned long long g_total_harvests = 0, g_total_rare = 0;

void rogue_gather_defs_reset(void) { g_def_count = 0; }
int rogue_gather_def_count(void) { return g_def_count; }
const RogueGatherNodeDef* rogue_gather_def_at(int idx)
{
    if (idx < 0 || idx >= g_def_count)
        return NULL;
    return &g_defs[idx];
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
    size_t n = strlen(src);
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int parse_material_table(const char* s, RogueGatherNodeDef* d)
{
    if (!s || !d)
        return 0;
    char buf[256];
    copy_str(buf, sizeof buf, s);
    char* ctx = NULL;
    int count = 0;
#if defined(_MSC_VER)
    char* tok = strtok_s(buf, ";", &ctx);
#else
    char* tok = strtok(buf, ";");
#endif
    while (tok && count < 8)
    {
        char* colon = strchr(tok, ':');
        if (!colon)
            goto next;
        *colon = '\0';
        int mat_def = -1;
        const RogueMaterialDef* md = rogue_material_find(tok);
        if (!md)
        { /* try item def id mapping */
            int item_def = rogue_item_def_index(tok);
            const RogueMaterialDef* md2 = rogue_material_find_by_item(item_def);
            if (md2)
                md = md2;
        }
        if (md)
        {
            mat_def = (int) (md - rogue_material_find(md->id));
        }
        int weight = atoi(colon + 1);
        if (weight <= 0)
            weight = 1;
        if (mat_def >= 0)
        {
            d->material_defs[count] = mat_def;
            d->material_weights[count] = weight;
            count++;
        }
    next:
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ";", &ctx);
#else
        tok = strtok(NULL, ";");
#endif
    }
    d->material_count = count;
    return count > 0;
}

int rogue_gather_defs_load_path(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        fprintf(stderr, "gather: cannot open %s\n", path);
        return -1;
    }
    /* JSON support: detect by extension */
    {
        size_t lp = strlen(path);
        if (lp >= 5 && (strcmp(path + lp - 5, ".json") == 0))
        {
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
            const char* s = buf;
            while (*s && *s != '[')
                s++;
            if (*s != '[')
            {
                free(buf);
                return -1;
            }
            s++;
            int added_json = 0;
            while (*s)
            {
                while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' || *s == ','))
                    s++;
                if (*s == ']')
                    break;
                if (*s != '{')
                    break;
                s++;
                RogueGatherNodeDef def;
                memset(&def, 0, sizeof def);
                def.respawn_ms = 60000.0f;
                def.rare_bonus_multiplier = 2.0f;
                def.spawn_chance_pct = 100;
                def.rare_proc_chance_pct = 5;
                while (*s && *s != '}')
                {
                    while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t'))
                        s++;
                    if (*s != '"')
                    {
                        s++;
                        continue;
                    }
                    s++;
                    char key[32];
                    int ki = 0;
                    while (*s && *s != '"' && ki + 1 < (int) sizeof key)
                        key[ki++] = *s++;
                    key[ki] = '\0';
                    if (*s == '"')
                        s++;
                    while (*s && *s != ':')
                        s++;
                    if (*s == ':')
                        s++;
                    while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t'))
                        s++;
                    if (strcmp(key, "id") == 0)
                    {
                        if (*s == '"')
                        {
                            s++;
                            int i = 0;
                            while (*s && *s != '"' && i + 1 < (int) sizeof def.id)
                                def.id[i++] = *s++;
                            def.id[i] = '\0';
                            if (*s == '"')
                                s++;
                        }
                    }
                    else if (strcmp(key, "materials") == 0)
                    {
                        while (*s && *s != '[')
                            s++;
                        if (*s == '[')
                            s++;
                        int mc = 0;
                        while (*s)
                        {
                            while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' ||
                                          *s == ','))
                                s++;
                            if (*s == ']')
                            {
                                s++;
                                break;
                            }
                            if (*s != '{')
                                break;
                            s++;
                            char mid[64] = {0};
                            int w = 1;
                            while (*s && *s != '}')
                            {
                                while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t'))
                                    s++;
                                if (*s != '"')
                                {
                                    s++;
                                    continue;
                                }
                                s++;
                                char k2[16];
                                int kj = 0;
                                while (*s && *s != '"' && kj + 1 < (int) sizeof k2)
                                    k2[kj++] = *s++;
                                k2[kj] = '\0';
                                if (*s == '"')
                                    s++;
                                while (*s && *s != ':')
                                    s++;
                                if (*s == ':')
                                    s++;
                                while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t'))
                                    s++;
                                if (strcmp(k2, "id") == 0)
                                {
                                    if (*s == '"')
                                    {
                                        s++;
                                        int i = 0;
                                        while (*s && *s != '"' && i + 1 < (int) sizeof mid)
                                            mid[i++] = *s++;
                                        mid[i] = '\0';
                                        if (*s == '"')
                                            s++;
                                    }
                                }
                                else if (strcmp(k2, "weight") == 0)
                                {
                                    w = (int) strtol(s, (char**) &s, 10);
                                    if (w <= 0)
                                        w = 1;
                                }
                                while (*s && *s != ',' && *s != '}')
                                    s++;
                                if (*s == ',')
                                    s++;
                            }
                            if (*s == '}')
                                s++;
                            if (mid[0] && mc < 8)
                            {
                                const RogueMaterialDef* md = rogue_material_find(mid);
                                if (!md)
                                {
                                    int item_di = rogue_item_def_index(mid);
                                    md = rogue_material_find_by_item(item_di);
                                }
                                if (md)
                                {
                                    /* compute material index by scanning registry */
                                    int mat_index = -1;
                                    int mcount = rogue_material_count();
                                    for (int mi = 0; mi < mcount; mi++)
                                    {
                                        if (rogue_material_get(mi) == md)
                                        {
                                            mat_index = mi;
                                            break;
                                        }
                                    }
                                    if (mat_index >= 0)
                                    {
                                        def.material_defs[mc] = mat_index;
                                        def.material_weights[mc] = w;
                                        mc++;
                                    }
                                }
                            }
                            while (*s && *s != ',' && *s != ']')
                                s++;
                            if (*s == ',')
                                s++;
                        }
                        def.material_count = mc;
                    }
                    else if (strcmp(key, "min_roll") == 0)
                    {
                        def.min_roll = (int) strtol(s, (char**) &s, 10);
                    }
                    else if (strcmp(key, "max_roll") == 0)
                    {
                        def.max_roll = (int) strtol(s, (char**) &s, 10);
                    }
                    else if (strcmp(key, "respawn_ms") == 0)
                    {
                        def.respawn_ms = (float) strtod(s, (char**) &s);
                    }
                    else if (strcmp(key, "tool_req_tier") == 0)
                    {
                        def.tool_req_tier = (int) strtol(s, (char**) &s, 10);
                    }
                    else if (strcmp(key, "biome_tags") == 0)
                    {
                        if (*s == '"')
                        {
                            s++;
                            int i = 0;
                            while (*s && *s != '"' && i + 1 < (int) sizeof def.biome_tags)
                                def.biome_tags[i++] = *s++;
                            def.biome_tags[i] = '\0';
                            if (*s == '"')
                                s++;
                        }
                    }
                    else if (strcmp(key, "spawn_chance_pct") == 0)
                    {
                        def.spawn_chance_pct = (int) strtol(s, (char**) &s, 10);
                    }
                    else if (strcmp(key, "rare_proc_chance_pct") == 0)
                    {
                        def.rare_proc_chance_pct = (int) strtol(s, (char**) &s, 10);
                    }
                    else if (strcmp(key, "rare_bonus_multiplier") == 0)
                    {
                        def.rare_bonus_multiplier = (float) strtod(s, (char**) &s);
                    }
                    while (*s && *s != ',' && *s != '}')
                        s++;
                    if (*s == ',')
                        s++;
                }
                while (*s && *s != '}')
                    s++;
                if (*s == '}')
                    s++;
                if (g_def_count < ROGUE_GATHER_NODE_CAP && def.id[0] && def.material_count > 0)
                {
                    if (def.max_roll < def.min_roll)
                        def.max_roll = def.min_roll;
                    g_defs[g_def_count++] = def;
                    added_json++;
                }
                while (*s && *s != ',' && *s != ']')
                    s++;
                if (*s == ',')
                    s++;
            }
            free(buf);
            return added_json;
        }
    }
    char line[512];
    int added = 0;
    while (fgets(line, sizeof line, f))
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (line[0] == '#' || line[0] == '\0')
            continue;
        if (g_def_count >= ROGUE_GATHER_NODE_CAP)
            break;
        /* tokenization */
        char buf[512];
        copy_str(buf, sizeof buf, line);
        char* ctx = NULL;
#if defined(_MSC_VER)
        char* tok = strtok_s(buf, ",", &ctx);
#else
        char* tok = strtok(buf, ",", &ctx);
#endif
        RogueGatherNodeDef def;
        memset(&def, 0, sizeof def);
        def.respawn_ms = 60000.0f;
        def.rare_bonus_multiplier = 2.0f;
        def.spawn_chance_pct = 100;
        def.rare_proc_chance_pct = 5;
        if (!tok)
            continue;
        copy_str(def.id, sizeof def.id, tok);
        /* material table */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &ctx);
#else
        tok = strtok(NULL, ",", &ctx);
#endif
        if (!tok)
        {
            continue;
        }
        parse_material_table(tok, &def);
        /* min_roll */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &ctx);
#else
        tok = strtok(NULL, ",", &ctx);
#endif
        if (!tok)
            continue;
        def.min_roll = atoi(tok);
        /* max_roll */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &ctx);
#else
        tok = strtok(NULL, ",", &ctx);
#endif
        if (!tok)
            continue;
        def.max_roll = atoi(tok);
        if (def.max_roll < def.min_roll)
            def.max_roll = def.min_roll;
            /* respawn */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &ctx);
#else
        tok = strtok(NULL, ",", &ctx);
#endif
        if (tok)
            def.respawn_ms = (float) atof(tok);
            /* tool tier */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &ctx);
#else
        tok = strtok(NULL, ",", &ctx);
#endif
        if (tok)
            def.tool_req_tier = atoi(tok);
        else
            def.tool_req_tier = 0;
            /* biome tags */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &ctx);
#else
        tok = strtok(NULL, ",", &ctx);
#endif
        if (tok)
            copy_str(def.biome_tags, sizeof def.biome_tags, tok);
            /* spawn chance */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &ctx);
#else
        tok = strtok(NULL, ",", &ctx);
#endif
        if (tok)
            def.spawn_chance_pct = atoi(tok);
            /* rare proc chance */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &ctx);
#else
        tok = strtok(NULL, ",", &ctx);
#endif
        if (tok)
            def.rare_proc_chance_pct = atoi(tok);
            /* rare bonus multiplier */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &ctx);
#else
        tok = strtok(NULL, ",", &ctx);
#endif
        if (tok)
            def.rare_bonus_multiplier = (float) atof(tok);
        g_defs[g_def_count++] = def;
        added++;
    }
    fclose(f);
    return added;
}

int rogue_gather_defs_load_default(void)
{
    char path[256];
    if (!rogue_find_asset_path("gathering/nodes.cfg", path, sizeof path))
        return -1;
    return rogue_gather_defs_load_path(path);
}

static unsigned int mix_hash(unsigned int seed, unsigned int v)
{
    seed ^= v + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

int rogue_gather_spawn_chunk(unsigned int world_seed, int chunk_id)
{
    if (chunk_id < 0)
        return 0;
    int spawned_before = g_node_count;
    for (int i = 0; i < g_def_count && g_node_count < ROGUE_GATHER_NODE_CAP; i++)
    {
        const RogueGatherNodeDef* d = &g_defs[i];
        unsigned int h = mix_hash(world_seed, (unsigned int) chunk_id * 73856093u ^
                                                  (unsigned int) i * 19349663u);
        if ((h % 100u) >= (unsigned int) (d->spawn_chance_pct <= 0 ? 0 : d->spawn_chance_pct))
            continue;
        RogueGatherNodeInstance inst;
        inst.def_index = i;
        inst.chunk_id = chunk_id;
        inst.state = 0;
        inst.respawn_timer_ms = 0.0f;
        inst.rare_last = 0;
        g_nodes[g_node_count++] = inst;
    }
    return g_node_count - spawned_before;
}
int rogue_gather_node_count(void) { return g_node_count; }
const RogueGatherNodeInstance* rogue_gather_node_at(int idx)
{
    if (idx < 0 || idx >= g_node_count)
        return NULL;
    return &g_nodes[idx];
}

void rogue_gather_update(float dt_ms)
{
    for (int i = 0; i < g_node_count; i++)
    {
        if (g_nodes[i].state == 1)
        {
            g_nodes[i].respawn_timer_ms -= dt_ms;
            if (g_nodes[i].respawn_timer_ms <= 0)
            {
                g_nodes[i].state = 0;
                g_nodes[i].respawn_timer_ms = 0;
            }
        }
    }
}

static int weighted_pick(const int* defs, const int* weights, int count, unsigned int* rng)
{
    int total = 0;
    for (int i = 0; i < count; i++)
        total += weights[i] > 0 ? weights[i] : 0;
    if (total <= 0)
        return -1;
    unsigned int r = (*rng % (unsigned int) total);
    int acc = 0;
    for (int i = 0; i < count; i++)
    {
        if (weights[i] <= 0)
            continue;
        acc += weights[i];
        if ((int) r < acc)
            return defs[i];
    }
    return -1;
}

int rogue_gather_harvest(int node_index, unsigned int* rng_state, int* out_material_def,
                         int* out_qty)
{
    if (node_index < 0 || node_index >= g_node_count || !rng_state || !out_material_def || !out_qty)
        return -1;
    RogueGatherNodeInstance* inst = &g_nodes[node_index];
    if (inst->state == 1)
        return -2;
    const RogueGatherNodeDef* def = &g_defs[inst->def_index];
    if (g_player_tool_tier < def->tool_req_tier)
        return -3;
    *rng_state = (*rng_state * 1664525u) + 1013904223u;
    unsigned int r = *rng_state;
    int mat_index =
        weighted_pick(def->material_defs, def->material_weights, def->material_count, &r);
    if (mat_index < 0)
        return -4;
    int range = def->max_roll - def->min_roll + 1;
    if (range <= 0)
        range = 1;
    int qty = def->min_roll + (int) (r % (unsigned int) range);
    inst->rare_last = 0; /* rare proc */
    if (def->rare_proc_chance_pct > 0)
    {
        unsigned int rp = (r / 97u) % 100u;
        if (rp < (unsigned int) def->rare_proc_chance_pct)
        {
            qty = (int) (qty * def->rare_bonus_multiplier);
            if (qty < 1)
                qty = 1;
            inst->rare_last = 1;
            g_total_rare++;
        }
    }
    *out_material_def = mat_index;
    *out_qty = qty;
    inst->state = 1;
    inst->respawn_timer_ms = def->respawn_ms;
    g_total_harvests++;
    return 0;
}

void rogue_gather_set_player_tool_tier(int tier)
{
    if (tier < 0)
        tier = 0;
    g_player_tool_tier = tier;
}
int rogue_gather_get_player_tool_tier(void) { return g_player_tool_tier; }
unsigned long long rogue_gather_total_harvests(void) { return g_total_harvests; }
unsigned long long rogue_gather_total_rare_procs(void) { return g_total_rare; }
