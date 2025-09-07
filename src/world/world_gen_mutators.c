#include "../content/json_io.h"
#include "world_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MUTATOR_MAX 128

static RogueMutatorDesc g_mutators[MUTATOR_MAX];
static int g_mutator_count = 0;

/* ---- Phase 9.3: stacking rules & incompatibility matrix ---- */
#define MUTATOR_RULE_MAX 128
#define MUTATOR_GROUP_MAX 32

typedef struct RogueMutatorIncompatPair
{
    int a, b; /* indices into registry; -1 if undefined */
} RogueMutatorIncompatPair;

static RogueMutatorIncompatPair g_incompat[MUTATOR_RULE_MAX];
static int g_incompat_count = 0;

typedef struct RogueMutatorGroupCap
{
    char group[24];
    int cap;
} RogueMutatorGroupCap;

static RogueMutatorGroupCap g_group_caps[MUTATOR_GROUP_MAX];
static int g_group_cap_count = 0;

typedef struct RogueRunCb
{
    RogueRunSummaryCallback cb;
    void* user;
} RogueRunCb;

static RogueRunCb g_run_cbs[16];
static int g_run_cb_count = 0;

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

void rogue_mutator_clear_rules(void)
{
    g_incompat_count = 0;
    g_group_cap_count = 0;
}

int rogue_mutator_define_incompatible(const char* id_a, const char* id_b)
{
    if (!id_a || !id_b || g_incompat_count >= MUTATOR_RULE_MAX)
        return 0;
    int ia = rogue_mutator_registry_find(id_a);
    int ib = rogue_mutator_registry_find(id_b);
    if (ia < 0 || ib < 0)
        return 0;
    /* avoid duplicate pairs */
    for (int i = 0; i < g_incompat_count; ++i)
    {
        if ((g_incompat[i].a == ia && g_incompat[i].b == ib) ||
            (g_incompat[i].a == ib && g_incompat[i].b == ia))
            return 1;
    }
    g_incompat[g_incompat_count].a = ia;
    g_incompat[g_incompat_count].b = ib;
    ++g_incompat_count;
    return 1;
}

int rogue_mutator_define_group_cap(const char* group, int max_count)
{
    if (!group || max_count < 0)
        return 0;
    for (int i = 0; i < g_group_cap_count; ++i)
    {
        if (strcmp(g_group_caps[i].group, group) == 0)
        {
            g_group_caps[i].cap = max_count;
            return 1;
        }
    }
    if (g_group_cap_count >= MUTATOR_GROUP_MAX)
        return 0;
#ifdef _MSC_VER
    strncpy_s(g_group_caps[g_group_cap_count].group, sizeof g_group_caps[g_group_cap_count].group,
              group, _TRUNCATE);
#else
    strncpy(g_group_caps[g_group_cap_count].group, group,
            sizeof g_group_caps[g_group_cap_count].group - 1);
#endif
    g_group_caps[g_group_cap_count].cap = max_count;
    ++g_group_cap_count;
    return 1;
}

static int rogue__mutator_is_compatible_set_impl(const int* indices, int count)
{
    /* incompatibility pairs */
    for (int i = 0; i < g_incompat_count; ++i)
    {
        int a = g_incompat[i].a, b = g_incompat[i].b;
        int has_a = 0, has_b = 0;
        for (int j = 0; j < count; ++j)
        {
            if (indices[j] == a)
                has_a = 1;
            if (indices[j] == b)
                has_b = 1;
        }
        if (has_a && has_b)
            return 0;
    }
    /* group caps */
    for (int g = 0; g < g_group_cap_count; ++g)
    {
        int cap = g_group_caps[g].cap;
        if (cap <= 0)
            continue;
        int cnt = 0;
        for (int j = 0; j < count; ++j)
        {
            const RogueMutatorDesc* d = rogue_mutator_get_desc(indices[j]);
            if (d && d->group[0] != '\0' && strcmp(d->group, g_group_caps[g].group) == 0)
                ++cnt;
        }
        if (cnt > cap)
            return 0;
    }
    return 1;
}

int rogue_mutator_is_compatible_set(const int* indices, int count)
{
    if (!indices || count < 0)
        return 0;
    return rogue__mutator_is_compatible_set_impl(indices, count);
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
            else if (strcmp(key, "group") == 0)
            {
                char tmp[24];
                if (!parse_string(&s, tmp, sizeof tmp))
                    return -11;
#ifdef _MSC_VER
                strncpy_s(d.group, sizeof d.group, tmp, _TRUNCATE);
#else
                strncpy(d.group, tmp, sizeof d.group - 1);
#endif
            }
            else
            {
                /* skip unknown simple value */
                if (*s == '"')
                {
                    char throwaway[64];
                    if (!parse_string(&s, throwaway, sizeof throwaway))
                        return -12;
                }
                else
                {
                    double v;
                    if (!parse_number(&s, &v))
                        return -13;
                }
            }
            skip_ws(&s);
            if (*s == ',')
                ++s;
        }
        if (d.id[0] == '\0' || d.weight <= 0.0f)
        {
            set_err(err, err_cap, "mutator missing id/weight");
            return -14;
        }
        if (rogue_mutator_register(&d) < 0)
        {
            set_err(err, err_cap, "register failed (dup or cap) ");
            return -15;
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

/* Small helper: try to find a compatible subset of size target_n from candidate list.
 * Backtracking over a tiny set (k is small in tests), returns 1 on success and fills out[]. */
static int select_compatible_subset(const int* candidates, int cand_count, int target_n, int* out)
{
    int stack_idx[MUTATOR_MAX];
    int stack_top = 0;
    int best_count = 0;
    int best_set[MUTATOR_MAX];
    /* Simple DFS */
    int idx = 0;
    int chosen_count = 0;
    int chosen[MUTATOR_MAX];
    while (1)
    {
        if (chosen_count == target_n)
        {
            /* success */
            for (int i = 0; i < target_n; ++i)
                out[i] = chosen[i];
            return 1;
        }
        if (idx >= cand_count)
        {
            /* backtrack */
            if (stack_top == 0)
                break;
            idx = stack_idx[--stack_top];
            /* skip this index next */
            ++idx;
            if (chosen_count > 0)
                --chosen_count;
            continue;
        }
        /* Heuristic pruning: if remaining candidates + chosen can't reach target, prune */
        if (chosen_count + (cand_count - idx) < target_n)
        {
            if (stack_top == 0)
                break;
            idx = stack_idx[--stack_top];
            ++idx;
            if (chosen_count > 0)
                --chosen_count;
            continue;
        }
        int next = candidates[idx];
        int tmp_count = chosen_count + 1;
        int tmp[MUTATOR_MAX];
        for (int i = 0; i < chosen_count; ++i)
            tmp[i] = chosen[i];
        tmp[chosen_count] = next;
        if (rogue__mutator_is_compatible_set_impl(tmp, tmp_count))
        {
            /* choose it */
            stack_idx[stack_top++] = idx; /* remember to backtrack here later */
            chosen[chosen_count++] = next;
            ++idx;
            continue;
        }
        else
        {
            /* skip it */
            ++idx;
            continue;
        }
    }
    (void) best_count;
    (void) best_set;
    return 0;
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

    /* Step 1: build an initial candidate list of up to k unique entries (weighted, no replacement)
     */
    int candidates[MUTATOR_MAX];
    int cand_count = 0;
    float w_work[MUTATOR_MAX];
    for (int i = 0; i < g_mutator_count; ++i)
        w_work[i] = w[i];
    for (int i = 0; i < k && cand_count < g_mutator_count; ++i)
    {
        int idx = weighted_pick(&ctx->micro_rng, w_work, g_mutator_count);
        if (idx < 0)
            break;
        candidates[cand_count++] = idx;
        w_work[idx] = 0.0f; /* without replacement */
    }

    /* Step 2: attempt to select n compatible from candidates; if not possible, expand pool */
    int success = 0;
    int attempt_rounds = 0;
    while (!success && attempt_rounds < 3)
    {
        if (select_compatible_subset(candidates, cand_count, n, out_indices))
        {
            success = 1;
            break;
        }
        /* Expand: add more unique candidates (weighted) and retry */
        ++attempt_rounds;
        for (int i = 0; i < g_mutator_count; ++i)
            w_work[i] = w[i];
        for (int c = 0; c < cand_count; ++c)
            w_work[candidates[c]] = 0.0f;
        int to_add = (k < g_mutator_count ? k : g_mutator_count);
        for (int i = 0; i < to_add && cand_count < g_mutator_count; ++i)
        {
            int idx = weighted_pick(&ctx->micro_rng, w_work, g_mutator_count);
            if (idx < 0)
                break;
            candidates[cand_count++] = idx;
            w_work[idx] = 0.0f;
        }
    }

    int chosen = 0;
    if (success)
        chosen = n;
    else
    {
        /* Fallback: greedily pick compatible from the (possibly expanded) candidates */
        for (int i = 0; i < cand_count && chosen < n; ++i)
        {
            int idx = candidates[i];
            int tmp[MUTATOR_MAX];
            for (int t = 0; t < chosen; ++t)
                tmp[t] = out_indices[t];
            tmp[chosen] = idx;
            if (rogue__mutator_is_compatible_set_impl(tmp, chosen + 1))
                out_indices[chosen++] = idx;
        }
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

/* ---- Run summary callbacks ---- */
int rogue_dungeon_register_run_summary_callback(RogueRunSummaryCallback cb, void* user_data)
{
    if (!cb)
        return 0;
    for (int i = 0; i < g_run_cb_count; ++i)
        if (g_run_cbs[i].cb == cb && g_run_cbs[i].user == user_data)
            return 1; /* already registered */
    if (g_run_cb_count >= (int) (sizeof g_run_cbs / sizeof g_run_cbs[0]))
        return 0;
    g_run_cbs[g_run_cb_count].cb = cb;
    g_run_cbs[g_run_cb_count].user = user_data;
    ++g_run_cb_count;
    return 1;
}

void rogue_dungeon_clear_run_summary_callbacks(void) { g_run_cb_count = 0; }

void rogue_dungeon_emit_run_summary(const int* mutator_indices, int count,
                                    float reward_multiplier_accum)
{
    char manifest[256];
    manifest[0] = '\0';
    (void) rogue_mutator_manifest_csv(mutator_indices, count, manifest, sizeof manifest);
    for (int i = 0; i < g_run_cb_count; ++i)
    {
        if (g_run_cbs[i].cb)
            g_run_cbs[i].cb(manifest, reward_multiplier_accum, g_run_cbs[i].user);
    }
}
