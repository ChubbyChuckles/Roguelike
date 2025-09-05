#include "content/json_io.h"
#include "world_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MUTATOR_MAX 128

static RogueMutatorDesc g_mutators[MUTATOR_MAX];
static int g_mutator_count = 0;

void rogue_mutator_clear_registry(void) { g_mutator_count = 0; }

int rogue_mutator_register(const RogueMutatorDesc* d)
{
    if (!d || d->id[0] == '\0' || d->weight <= 0.0f || g_mutator_count >= MUTATOR_MAX)
        return -1;
    /* reject dup id */
    for (int i = 0; i < g_mutator_count; ++i)
        if (strcmp(g_mutators[i].id, d->id) == 0)
            return -1;
    g_mutators[g_mutator_count] = *d;
    return g_mutator_count++;
}

int rogue_mutator_registry_count(void) { return g_mutator_count; }

const RogueMutatorDesc* rogue_mutator_get_desc(int index)
{
    if (index < 0 || index >= g_mutator_count)
        return NULL;
    return &g_mutators[index];
}

int rogue_mutator_registry_find(const char* id)
{
    if (!id)
        return -1;
    for (int i = 0; i < g_mutator_count; ++i)
        if (strcmp(g_mutators[i].id, id) == 0)
            return i;
    return -1;
}

/* Minimal JSON reader for an array of objects with id, weight, reward_multiplier, effect */
static void set_err(char* err, size_t cap, const char* msg)
{
    if (!err || cap == 0)
        return;
#ifdef _MSC_VER
    strncpy_s(err, cap, msg ? msg : "", _TRUNCATE);
#else
    if (!msg)
        msg = "";
    strncpy(err, msg, cap - 1);
    err[cap - 1] = '\0';
#endif
}

static void skip_ws(const char** ps)
{
    const char* s = *ps;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        ++s;
    *ps = s;
}

static int parse_string(const char** ps, char* out, size_t cap)
{
    const char* s = *ps;
    if (*s != '"')
        return 0;
    ++s;
    size_t i = 0;
    while (*s && *s != '"')
    {
        if (i + 1 < cap)
            out[i++] = *s;
        ++s;
    }
    if (*s != '"')
        return 0;
    out[i] = '\0';
    *ps = s + 1;
    return 1;
}

static int parse_number(const char** ps, double* out)
{
    char* e = NULL;
    double v = strtod(*ps, &e);
    if (e == *ps)
        return 0;
    *out = v;
    *ps = e;
    return 1;
}

int rogue_mutator_registry_load_json_text(const char* json_text, char* err, size_t err_cap)
{
    if (!json_text)
    {
        set_err(err, err_cap, "invalid args");
        return -1;
    }
    const char* s = json_text;
    skip_ws(&s);
    if (*s != '[')
    {
        set_err(err, err_cap, "expected array");
        return -2;
    }
    ++s;
    int added = 0;
    while (1)
    {
        skip_ws(&s);
        if (*s == ']')
        {
            ++s;
            break;
        }
        if (*s != '{')
        {
            set_err(err, err_cap, "expected object");
            return -3;
        }
        ++s;
        RogueMutatorDesc d;
        memset(&d, 0, sizeof d);
        d.weight = 1.0f;
        d.reward_multiplier = 1.0f;
        while (1)
        {
            skip_ws(&s);
            if (*s == '}')
            {
                ++s;
                break;
            }
            if (*s != '"')
            {
                set_err(err, err_cap, "expected key");
                return -4;
            }
            char key[32];
            if (!parse_string(&s, key, sizeof key))
                return -5;
            skip_ws(&s);
            if (*s != ':')
                return -6;
            ++s;
            skip_ws(&s);
            if (strcmp(key, "id") == 0)
            {
                char tmp[32];
                if (!parse_string(&s, tmp, sizeof tmp))
                    return -7;
#ifdef _MSC_VER
                strncpy_s(d.id, sizeof d.id, tmp, _TRUNCATE);
#else
                strncpy(d.id, tmp, sizeof d.id - 1);
#endif
            }
            else if (strcmp(key, "weight") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -8;
                d.weight = (float) ((v < 0) ? 0.0 : v);
            }
            else if (strcmp(key, "reward_multiplier") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -9;
                d.reward_multiplier = (float) ((v < 0) ? 0.0 : v);
            }
            else if (strcmp(key, "effect") == 0)
            {
                char tmp[128];
                if (!parse_string(&s, tmp, sizeof tmp))
                    return -10;
#ifdef _MSC_VER
                strncpy_s(d.effect_dsl, sizeof d.effect_dsl, tmp, _TRUNCATE);
#else
                strncpy(d.effect_dsl, tmp, sizeof d.effect_dsl - 1);
#endif
            }
            else
            {
                /* skip unknown simple value */
                if (*s == '"')
                {
                    char throwaway[64];
                    if (!parse_string(&s, throwaway, sizeof throwaway))
                        return -11;
                }
                else
                {
                    double v;
                    if (!parse_number(&s, &v))
                        return -12;
                }
            }
            skip_ws(&s);
            if (*s == ',')
                ++s;
        }
        if (d.id[0] == '\0' || d.weight <= 0.0f)
        {
            set_err(err, err_cap, "mutator missing id/weight");
            return -13;
        }
        if (rogue_mutator_register(&d) < 0)
        {
            set_err(err, err_cap, "register failed (dup or cap) ");
            return -14;
        }
        ++added;
        skip_ws(&s);
        if (*s == ',')
        {
            ++s;
            continue;
        }
        if (*s == ']')
        {
            ++s;
            break;
        }
    }
    (void) s;
    return added;
}

int rogue_mutator_registry_load_json_file(const char* path, char* err, size_t err_cap)
{
    char* buf = NULL;
    size_t len = 0;
    int rc = json_io_read_file(path, &buf, &len, err, (int) err_cap);
    if (rc != 0)
        return -1;
    int added = rogue_mutator_registry_load_json_text(buf, err, err_cap);
    free(buf);
    return added;
}

/* Weighted roll without replacement: draw k candidates (may include repeats in roll step),
 * then select n unique by best-of or reservoir? We'll implement simple without-replacement
 * selection by iteratively drawing based on remaining weights. */
static int weighted_pick(RogueRngChannel* rng, const float* w, int count)
{
    double sum = 0.0;
    for (int i = 0; i < count; ++i)
        sum += (w[i] > 0 ? w[i] : 0.0f);
    if (sum <= 0.0)
        return -1;
    double r = rogue_worldgen_rand_norm(rng) * sum;
    if (r < 0)
        r = 0;
    for (int i = 0; i < count; ++i)
    {
        double wi = (w[i] > 0 ? w[i] : 0.0f);
        if (r < wi)
            return i;
        r -= wi;
    }
    return count - 1;
}

int rogue_mutator_roll_k_choose_n(RogueWorldGenContext* ctx, int k, int n, int* out_indices,
                                  int cap)
{
    if (!ctx || !out_indices || cap <= 0 || k <= 0 || n <= 0 || g_mutator_count <= 0)
        return -1;
    if (n > k)
        n = k;
    if (n > g_mutator_count)
        n = g_mutator_count;
    float w[MUTATOR_MAX];
    for (int i = 0; i < g_mutator_count; ++i)
        w[i] = g_mutators[i].weight;
    int chosen = 0;
    int picked_flags[MUTATOR_MAX] = {0};
    for (int iter = 0; iter < k && chosen < n; ++iter)
    {
        int idx = weighted_pick(&ctx->micro_rng, w, g_mutator_count);
        if (idx < 0)
            break;
        if (!picked_flags[idx])
        {
            picked_flags[idx] = 1;
            out_indices[chosen++] = idx;
        }
        /* reduce weight to discourage repeats even if we keep iterating */
        w[idx] = 0.0f;
    }
    /* Sort chosen by id for manifest stability */
    for (int i = 0; i < chosen; ++i)
    {
        for (int j = i + 1; j < chosen; ++j)
        {
            const char* ai = g_mutators[out_indices[i]].id;
            const char* aj = g_mutators[out_indices[j]].id;
            if (strcmp(ai, aj) > 0)
            {
                int t = out_indices[i];
                out_indices[i] = out_indices[j];
                out_indices[j] = t;
            }
        }
    }
    return chosen;
}

int rogue_mutator_manifest_csv(const int* indices, int count, char* out_csv, size_t cap)
{
    if (!indices || !out_csv || cap == 0)
        return 0;
    size_t pos = 0;
    for (int i = 0; i < count; ++i)
    {
        const RogueMutatorDesc* d = rogue_mutator_get_desc(indices[i]);
        if (!d)
            return 0;
        const char* id = d->id;
        size_t idlen = strlen(id);
        if (i > 0)
        {
            if (pos + 1 >= cap)
                return 0;
            out_csv[pos++] = ',';
        }
        if (pos + idlen >= cap)
            return 0;
        memcpy(out_csv + pos, id, idlen);
        pos += idlen;
    }
    if (pos >= cap)
        return 0;
    out_csv[pos] = '\0';
    return 1;
}
