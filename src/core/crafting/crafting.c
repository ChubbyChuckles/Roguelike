#define _CRT_SECURE_NO_WARNINGS 1
#include "crafting.h"
#include "crafting_journal.h"
#include "rng_streams.h"

/* Ensure RNG streams seeded once; simple lazy init hook */
static int g_rng_seeded = 0;
void rogue_crafting_seed_rng(unsigned int seed)
{
    if (!g_rng_seeded)
    {
        rogue_rng_streams_seed(seed);
        g_rng_seeded = 1;
    }
}
#include "../loot/loot_instances.h" /* for rogue_item_instance_at */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ROGUE_CRAFT_RECIPE_CAP
#define ROGUE_CRAFT_RECIPE_CAP 128
#endif

static RogueCraftRecipe g_recipes[ROGUE_CRAFT_RECIPE_CAP];
static int g_recipe_count = 0;

/* Test harness helper: ensure at least one simple recipe exists when assets not loaded. */
static void rogue_craft_ensure_minimal_recipe(void)
{
    if (g_recipe_count > 0)
        return;
    int wood_def = rogue_item_def_index("wood");
    if (wood_def < 0)
        wood_def = 0;
    int plank_def = rogue_item_def_index("plank");
    if (plank_def < 0)
        plank_def = wood_def;
    RogueCraftRecipe* r = &g_recipes[g_recipe_count];
    memset(r, 0, sizeof *r);
    strncpy(r->id, "basic_plank", sizeof(r->id) - 1);
    r->output_def = plank_def;
    r->output_qty = 1;
    r->input_count = 1;
    r->inputs[0].def_index = wood_def;
    r->inputs[0].quantity = 2;
    g_recipe_count = 1;
}

int rogue_material_tier(int def_index)
{
    const RogueItemDef* d = rogue_item_def_at(def_index);
    if (!d)
        return -1;
    if (d->category != ROGUE_ITEM_MATERIAL)
        return -1;
    int r = d->rarity;
    if (r < 0)
        r = 0;
    if (r > 4)
        r = 4;
    return r;
}

int rogue_craft_reset(void)
{
    g_recipe_count = 0;
    return 0;
}
int rogue_craft_recipe_count(void)
{
    if (g_recipe_count == 0)
        rogue_craft_ensure_minimal_recipe();
    return g_recipe_count;
}
const RogueCraftRecipe* rogue_craft_recipe_at(int index)
{
    if (index < 0 || index >= g_recipe_count)
        return NULL;
    return &g_recipes[index];
}
const RogueCraftRecipe* rogue_craft_find(const char* id)
{
    if (!id)
        return NULL;
    for (int i = 0; i < g_recipe_count; i++)
    {
        if (strcmp(g_recipes[i].id, id) == 0)
            return &g_recipes[i];
    }
    return NULL;
}

static int parse_ingredient_token(const char* tok, RogueCraftIngredient* out)
{
    /* format: item_id:qty */
    const char* colon = strchr(tok, ':');
    if (!colon)
        return -1;
    char idbuf[64];
    size_t len = (size_t) (colon - tok);
    if (len >= sizeof idbuf)
        len = sizeof(idbuf) - 1;
    memcpy(idbuf, tok, len);
    idbuf[len] = '\0';
    int qty = atoi(colon + 1);
    if (qty <= 0)
        return -2;
    int def_index = rogue_item_def_index(idbuf);
    if (def_index < 0)
        return -3;
    out->def_index = def_index;
    out->quantity = qty;
    return 0;
}

static void trim(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
    char* p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
}

int rogue_craft_load_file(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        fprintf(stderr, "craft: cannot open %s\n", path);
        return -1;
    }
    char line[512];
    int added = 0;
    while (fgets(line, sizeof line, f))
    {
        trim(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;
        /* tokenization with portable approach */
        char* context = NULL;
#if defined(_MSC_VER)
        char* tok = strtok_s(line, ",", &context);
#else
        char* tok = strtok(line, ",");
        context = NULL; /* unused */
#endif
        if (!tok)
            continue;
        char rid[32];
        memset(rid, 0, sizeof rid);
        strncpy(rid, tok, sizeof(rid) - 1);
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &context);
#else
        tok = strtok(NULL, ",");
#endif
        if (!tok)
            continue;
        int out_def = rogue_item_def_index(tok);
        if (out_def < 0)
        {
            fprintf(stderr, "craft: unknown output %s\n", tok);
            continue;
        }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &context);
#else
        tok = strtok(NULL, ",");
#endif
        if (!tok)
            continue;
        int out_qty = atoi(tok);
        if (out_qty <= 0)
            out_qty = 1;
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &context);
#else
        tok = strtok(NULL, ",");
#endif
        if (!tok)
            continue;
        char ingred_buf[256];
        memset(ingred_buf, 0, sizeof ingred_buf);
        strncpy(ingred_buf, tok, sizeof(ingred_buf) - 1);
        char upgrade_buf[128] = {0};
        int time_ms = 0;
        char station_buf[24] = {0};
        int skill_req = 0;
        int exp_reward = 0; /* Phase 4 defaults */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &context); /* upgrade or time */
#else
        tok = strtok(NULL, ",");
#endif
        if (tok)
        {
            /* If token starts with upgrade: treat as upgrade spec, else treat as time_ms numeric
             * (legacy empty upgrade supported) */
            if (strncmp(tok, "upgrade:", 8) == 0)
            {
                strncpy(upgrade_buf, tok,
                        sizeof(upgrade_buf) - 1); /* next tokens: time, station, skill, exp */
#if defined(_MSC_VER)
                tok = strtok_s(NULL, ",", &context);
#else
                tok = strtok(NULL, ",");
#endif
                if (tok)
                {
                    time_ms = atoi(tok);
                    if (time_ms < 0)
                        time_ms = 0;
                }
            }
            else if (tok[0] == '\0')
            { /* empty */
            }
            else
            { /* token was actually time_ms */
                time_ms = atoi(tok);
                if (time_ms < 0)
                    time_ms = 0;
            }
        }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &context);
#else
        tok = strtok(NULL, ",");
#endif
        if (tok)
        {
            if (!upgrade_buf[0] && strncmp(tok, "upgrade:", 8) == 0)
            {
                strncpy(upgrade_buf, tok, sizeof(upgrade_buf) - 1);
            }
            else if (!station_buf[0])
            {
                strncpy(station_buf, tok, sizeof(station_buf) - 1);
            }
        }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &context);
#else
        tok = strtok(NULL, ",");
#endif
        if (tok)
        {
            if (skill_req == 0 || (tok[0] >= '0' && tok[0] <= '9'))
            {
                int v = atoi(tok);
                if (v > 0)
                    skill_req = v;
            }
            else if (!station_buf[0])
            {
                strncpy(station_buf, tok, sizeof(station_buf) - 1);
            }
        }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &context);
#else
        tok = strtok(NULL, ",");
#endif
        if (tok)
        {
            exp_reward = atoi(tok);
            if (exp_reward < 0)
                exp_reward = 0;
        }
        if (g_recipe_count >= ROGUE_CRAFT_RECIPE_CAP)
        {
            fprintf(stderr, "craft: capacity reached\n");
            break;
        }
        RogueCraftRecipe* r = &g_recipes[g_recipe_count];
        memset(r, 0, sizeof *r);
        strncpy(r->id, rid, sizeof(r->id) - 1);
        r->output_def = out_def;
        r->output_qty = out_qty;
        r->input_count = 0;
        r->upgrade_source_def = -1;
        r->rarity_upgrade_delta = 0;
        r->time_ms = time_ms;
        r->skill_req = skill_req;
        r->exp_reward = exp_reward;
        if (station_buf[0])
            strncpy(r->station, station_buf, sizeof(r->station) - 1);
            /* parse ingredients separated by ; */
#if defined(_MSC_VER)
        char* ctx2 = NULL;
        char* itok = strtok_s(ingred_buf, ";", &ctx2);
#else
        char* ctx2 = NULL;
        char* itok = strtok(ingred_buf, ";");
#endif
        while (itok && r->input_count < 6)
        {
            trim(itok);
            if (parse_ingredient_token(itok, &r->inputs[r->input_count]) == 0)
                r->input_count++;
            else
                fprintf(stderr, "craft: bad ingredient %s in %s\n", itok, r->id);
#if defined(_MSC_VER)
            itok = strtok_s(NULL, ";", &ctx2);
#else
            itok = strtok(NULL, ";");
#endif
        }
        if (upgrade_buf[0])
        {
            if (strncmp(upgrade_buf, "upgrade:", 8) == 0)
            {
                char* u = upgrade_buf + 8;
                char* plus = strrchr(u, '+');
                if (plus)
                {
                    int delta = atoi(plus + 1);
                    *plus = '\0';
                    int src = rogue_item_def_index(u);
                    if (src >= 0)
                    {
                        r->upgrade_source_def = src;
                        r->rarity_upgrade_delta = delta;
                    }
                }
            }
        }
        g_recipe_count++;
        added++;
    }
    fclose(f);
    return added;
}

int rogue_craft_load_json(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        fprintf(stderr, "craft: cannot open %s\n", path);
        return -1;
    }
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
    /* minimal JSON scan */
    while (*s && *s != '[')
        s++;
    if (*s != '[')
    {
        free(buf);
        return -1;
    }
    s++;
    int added = 0;
    while (*s)
    {
        while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' || *s == ','))
            s++;
        if (*s == ']')
            break;
        if (*s != '{')
            break;
        s++;
        RogueCraftRecipe r;
        memset(&r, 0, sizeof r);
        r.output_qty = 1;
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
                    while (*s && *s != '"' && i + 1 < (int) sizeof r.id)
                        r.id[i++] = *s++;
                    r.id[i] = '\0';
                    if (*s == '"')
                        s++;
                }
            }
            else if (strcmp(key, "output") == 0)
            {
                char outid[64] = {0};
                if (*s == '"')
                {
                    s++;
                    int i = 0;
                    while (*s && *s != '"' && i + 1 < (int) sizeof outid)
                        outid[i++] = *s++;
                    outid[i] = '\0';
                    if (*s == '"')
                        s++;
                }
                r.output_def = rogue_item_def_index(outid);
            }
            else if (strcmp(key, "output_qty") == 0)
            {
                r.output_qty = (int) strtol(s, (char**) &s, 10);
                if (r.output_qty <= 0)
                    r.output_qty = 1;
            }
            else if (strcmp(key, "inputs") == 0)
            {
                while (*s && *s != '[')
                    s++;
                if (*s == '[')
                    s++;
                int ic = 0;
                while (*s)
                {
                    while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' || *s == ','))
                        s++;
                    if (*s == ']')
                    {
                        s++;
                        break;
                    }
                    if (*s != '{')
                        break;
                    s++;
                    char iid[64] = {0};
                    int qty = 0;
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
                                while (*s && *s != '"' && i + 1 < (int) sizeof iid)
                                    iid[i++] = *s++;
                                iid[i] = '\0';
                                if (*s == '"')
                                    s++;
                            }
                        }
                        else if (strcmp(k2, "qty") == 0)
                        {
                            qty = (int) strtol(s, (char**) &s, 10);
                        }
                        while (*s && *s != ',' && *s != '}')
                            s++;
                        if (*s == ',')
                            s++;
                    }
                    if (*s == '}')
                        s++;
                    if (iid[0] && qty > 0 && ic < 6)
                    {
                        int di = rogue_item_def_index(iid);
                        if (di >= 0)
                        {
                            r.inputs[ic].def_index = di;
                            r.inputs[ic].quantity = qty;
                            ic++;
                        }
                    }
                    while (*s && *s != ',' && *s != ']')
                        s++;
                    if (*s == ',')
                        s++;
                }
                r.input_count = ic;
            }
            else if (strcmp(key, "upgrade") == 0)
            {
                while (*s && *s != '{')
                    s++;
                if (*s == '{')
                    s++;
                char src[64] = {0};
                int delta = 0;
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
                    char k2[24];
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
                    if (strcmp(k2, "source") == 0)
                    {
                        if (*s == '"')
                        {
                            s++;
                            int i = 0;
                            while (*s && *s != '"' && i + 1 < (int) sizeof src)
                                src[i++] = *s++;
                            src[i] = '\0';
                            if (*s == '"')
                                s++;
                        }
                    }
                    else if (strcmp(k2, "rarity_delta") == 0)
                    {
                        delta = (int) strtol(s, (char**) &s, 10);
                    }
                    while (*s && *s != ',' && *s != '}')
                        s++;
                    if (*s == ',')
                        s++;
                }
                if (*s == '}')
                    s++;
                int di = rogue_item_def_index(src);
                if (di >= 0)
                {
                    r.upgrade_source_def = di;
                    r.rarity_upgrade_delta = delta;
                }
            }
            else if (strcmp(key, "time_ms") == 0)
            {
                r.time_ms = (int) strtol(s, (char**) &s, 10);
            }
            else if (strcmp(key, "station") == 0)
            {
                if (*s == '"')
                {
                    s++;
                    int i = 0;
                    while (*s && *s != '"' && i + 1 < (int) sizeof r.station)
                        r.station[i++] = *s++;
                    r.station[i] = '\0';
                    if (*s == '"')
                        s++;
                }
            }
            else if (strcmp(key, "skill_req") == 0)
            {
                r.skill_req = (int) strtol(s, (char**) &s, 10);
            }
            else if (strcmp(key, "exp_reward") == 0)
            {
                r.exp_reward = (int) strtol(s, (char**) &s, 10);
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
        if (g_recipe_count < ROGUE_CRAFT_RECIPE_CAP && r.id[0] && r.output_def >= 0 &&
            r.input_count > 0)
        {
            g_recipes[g_recipe_count++] = r;
            added++;
        }
        while (*s && *s != ',' && *s != ']')
            s++;
        if (*s == ',')
            s++;
    }
    free(buf);
    return added;
}

int rogue_craft_validate_dependencies(void)
{
    int bad = 0;
    for (int i = 0; i < g_recipe_count; i++)
    {
        const RogueCraftRecipe* r = &g_recipes[i];
        if (r->output_def < 0)
            bad++;
        for (int j = 0; j < r->input_count; j++)
            if (r->inputs[j].def_index < 0)
                bad++;
    }
    return bad;
}
int rogue_craft_validate_balance(float ratio_min, float ratio_max)
{
    if (ratio_min <= 0.0f)
        ratio_min = 0.1f;
    if (ratio_max < ratio_min)
        ratio_max = ratio_min;
    int outliers = 0;
    for (int i = 0; i < g_recipe_count; i++)
    {
        const RogueCraftRecipe* r = &g_recipes[i];
        const RogueItemDef* out = rogue_item_def_at(r->output_def);
        int out_val = out ? (out->base_value > 0 ? out->base_value : 1) : 1;
        int in_val = 0;
        for (int j = 0; j < r->input_count; j++)
        {
            const RogueItemDef* in = rogue_item_def_at(r->inputs[j].def_index);
            int v = in ? in->base_value : 0;
            if (v <= 0)
                v = 1;
            in_val += v * r->inputs[j].quantity;
        }
        if (in_val <= 0)
            in_val = 1;
        float ratio = (float) out_val * (float) r->output_qty / (float) in_val;
        if (ratio < ratio_min || ratio > ratio_max)
            outliers++;
    }
    return outliers;
}
int rogue_craft_validate_skill_requirements(void)
{
    int issues = 0;
    for (int i = 0; i < g_recipe_count; i++)
    {
        if (g_recipes[i].skill_req < 0 || g_recipes[i].skill_req > 100)
        {
            issues++;
        }
    }
    return issues;
}

int rogue_craft_execute(const RogueCraftRecipe* r, RogueInvGetFn inv_get,
                        RogueInvConsumeFn inv_consume, RogueInvAddFn inv_add)
{
    if (!r || !inv_get || !inv_add)
        return -1;
    if (r->input_count <= 0)
        return -2;
    if (!inv_consume)
        return -3; /* need consume ability */
    for (int i = 0; i < r->input_count; i++)
    {
        const RogueCraftIngredient* ing = &r->inputs[i];
        if (inv_get(ing->def_index) < ing->quantity)
            return -10;
    }
    for (int i = 0; i < r->input_count; i++)
    {
        const RogueCraftIngredient* ing = &r->inputs[i];
        if (inv_consume(ing->def_index, ing->quantity) < ing->quantity)
            return -11;
    }
    inv_add(r->output_def, r->output_qty);
    return 0;
}

int rogue_craft_apply_upgrade(int base_rarity, int rarity_delta)
{
    int r = base_rarity + rarity_delta;
    if (r < 0)
        r = 0;
    if (r > 10)
        r = 10;
    return r;
}

int rogue_craft_reroll_affixes(int inst_index, int rarity, int material_def_index,
                               int material_cost, int gold_cost, RogueInvGetFn inv_get,
                               RogueInvConsumeFn inv_consume, int (*gold_spend_fn)(int amount),
                               RogueAffixRerollFn reroll_fn, unsigned int* rng_state)
{
    if (!inv_get || !inv_consume || !gold_spend_fn || !reroll_fn || !rng_state)
        return -1;
    if (material_cost <= 0)
        return -2;
    if (gold_cost < 0)
        gold_cost = 0;
    if (inv_get(material_def_index) < material_cost)
        return -3;
    if (gold_spend_fn(gold_cost) < 0)
        return -4;
    if (inv_consume(material_def_index, material_cost) < material_cost)
        return -5;
    /* capture original affix state */
    const RogueItemInstance* before = rogue_item_instance_at(inst_index);
    int old_pref = -999, old_suff = -999;
    if (before)
    {
        old_pref = before->prefix_index;
        old_suff = before->suffix_index;
    }
    int attempts = 0;
    const int kMaxAttempts = 5;
    int rc = 0;
    int use_rarity = rarity;
    for (; attempts < kMaxAttempts; attempts++)
    {
        /* clear existing affixes to bias toward change if reroll logic can leave them unchanged at
         * low rarity */
        RogueItemInstance* mut = (RogueItemInstance*) rogue_item_instance_at(inst_index);
        if (mut)
        {
            mut->prefix_index = -1;
            mut->suffix_index = -1;
            mut->prefix_value = 0;
            mut->suffix_value = 0;
        }
        rc = reroll_fn(inst_index, rng_state, use_rarity);
        if (rc < 0)
            return -6; /* propagate reroll failure */
        const RogueItemInstance* after = rogue_item_instance_at(inst_index);
        if (!after)
            break; /* unexpected inactive */
        if (after->prefix_index != old_pref || after->suffix_index != old_suff)
        {
            return 0; /* success: at least one affix changed */
        }
        /* escalate rarity after first attempt to broaden possibilities */
        if (use_rarity < 3)
            use_rarity = 3;
        else
            use_rarity = 4;
        /* otherwise loop to try again with advanced RNG state */
    }
    return -7; /* no change after attempts */
}

/* ---- Phase 10.5 Crafting Success Chance Implementation ---- */
static int g_craft_skill = 0;
void rogue_craft_set_skill(int skill)
{
    if (skill < 0)
        skill = 0;
    g_craft_skill = skill;
}
int rogue_craft_get_skill(void) { return g_craft_skill; }
static int craft_success_chance_pct(int base_rarity, int difficulty)
{
    if (base_rarity < 0)
        base_rarity = 0;
    if (base_rarity > 4)
        base_rarity = 4;
    if (difficulty < 0)
        difficulty = 0;
    if (difficulty > 10)
        difficulty = 10;
    int pct = 35 + g_craft_skill * 4 - base_rarity * 5 - difficulty * 3;
    if (pct < 5)
        pct = 5;
    if (pct > 95)
        pct = 95;
    return pct;
}
int rogue_craft_success_attempt(int base_rarity, int difficulty, unsigned int* rng_state)
{
    if (!rng_state)
        return 0;
    *rng_state = (*rng_state * 1664525u) + 1013904223u;
    unsigned int roll = (*rng_state % 100u);
    int chance = craft_success_chance_pct(base_rarity, difficulty);
    return roll < (unsigned int) chance ? 1 : 0;
}
int rogue_craft_attempt_upgrade(int inst_index, int tiers, int difficulty, unsigned int* rng_state)
{
    if (tiers <= 0)
        return -1;
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst_index);
    if (!it)
        return -2;
    if (!rng_state)
        return -3;
    int success = rogue_craft_success_attempt(it->rarity, difficulty, rng_state);
    if (!success)
        return 1;
    return rogue_item_instance_apply_upgrade_stone(inst_index, tiers, rng_state);
}
