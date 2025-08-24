/* enemy_modifiers.c - Phase 2 Procedural Enemy Modifiers core implementation
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enemy_modifiers.h"

#ifndef ROGUE_ARRAY_LEN
#define ROGUE_ARRAY_LEN(a) (int) (sizeof(a) / sizeof((a)[0]))
#endif

static RogueEnemyModifierDef g_modifiers[ROGUE_ENEMY_MAX_MODIFIERS];
static int g_modifier_count = 0;

static char* rogue_strdup(const char* s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char* m = (char*) malloc(n);
    if (m)
        memcpy(m, s, n);
    return m;
}

void rogue_enemy_modifiers_reset(void)
{
    for (int i = 0; i < g_modifier_count; i++)
    {
        /* names and telegraph were allocated */
        char* name_mut = (char*) g_modifiers[i].name;
        if (name_mut)
            free(name_mut);
        char* tele_mut = (char*) g_modifiers[i].telegraph;
        if (tele_mut)
            free(tele_mut);
    }
    g_modifier_count = 0;
}

int rogue_enemy_modifier_count(void) { return g_modifier_count; }

const RogueEnemyModifierDef* rogue_enemy_modifier_at(int index)
{
    if (index < 0 || index >= g_modifier_count)
        return NULL;
    return &g_modifiers[index];
}

const RogueEnemyModifierDef* rogue_enemy_modifier_by_id(int id)
{
    for (int i = 0; i < g_modifier_count; i++)
        if (g_modifiers[i].id == id)
            return &g_modifiers[i];
    return NULL;
}

static int parse_int(const char* v, int* out)
{
    if (!v)
        return -1;
    *out = atoi(v);
    return 0;
}
static int parse_float(const char* v, float* out)
{
    if (!v)
        return -1;
    *out = (float) atof(v);
    return 0;
}
static int parse_mask(const char* v, unsigned int* out)
{
    if (!v)
        return -1;
    unsigned int m = 0;
    const char* p = v;
    while (*p)
    {
        if (isdigit((unsigned char) *p))
        {
            int id = *p - '0';
            if (id >= 0 && id < 32)
                m |= (1u << id);
        }
        p++;
    }
    *out = m;
    return 0;
}
static int parse_tiers(const char* v, RogueEnemyTierMask* out)
{
    if (!v)
        return -1;
    unsigned int m = 0;
    const char* p = v;
    while (*p)
    {
        if (isdigit((unsigned char) *p))
        {
            int t = *p - '0';
            if (t >= 0 && t < 32)
                m |= (1u << t);
        }
        p++;
    }
    *out = m;
    return 0;
}

/* Simple line parser: key=value pairs separated by ';' per modifier block.
 * Blocks separated by blank line or EOF.
 * Required keys: id, name
 * Optional: weight (float), tiers (digits), dps, control, mobility, incompat (digits), telegraph
 */
int rogue_enemy_modifiers_load_file(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0)
        return -1;
#else
    f = fopen(path, "rb");
    if (!f)
        return -1;
#endif
    rogue_enemy_modifiers_reset();
    char line[512];
    RogueEnemyModifierDef cur;
    memset(&cur, 0, sizeof(cur));
    while (fgets(line, sizeof(line), f))
    {
        /* blank line => commit current if valid */
        int blank = 1;
        for (char* c = line; *c; ++c)
        {
            if (!isspace((unsigned char) *c))
            {
                blank = 0;
                break;
            }
        }
        if (blank)
        {
            if (cur.name)
            {
                if (g_modifier_count < ROGUE_ENEMY_MAX_MODIFIERS)
                    g_modifiers[g_modifier_count++] = cur;
                memset(&cur, 0, sizeof(cur));
            }
            continue;
        }
        /* manual split at first '=' */
        char* eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char* key = line;
        char* value = eq + 1;
        /* trim trailing whitespace from key */
        char* ktend = key + strlen(key);
        while (ktend > key && isspace((unsigned char) ktend[-1]))
        {
            ktend--;
        }
        *ktend = '\0';
        while (*value && isspace((unsigned char) *value))
            value++;
        /* trim newline */
        char* vtend = value + strlen(value);
        while (vtend > value &&
               (vtend[-1] == '\n' || vtend[-1] == '\r' || isspace((unsigned char) vtend[-1])))
        {
            vtend--;
        }
        *vtend = '\0';
        if (strcmp(key, "id") == 0)
        {
            parse_int(value, &cur.id);
        }
        else if (strcmp(key, "name") == 0)
        {
            cur.name = rogue_strdup(value);
        }
        else if (strcmp(key, "weight") == 0)
        {
            parse_float(value, &cur.weight);
        }
        else if (strcmp(key, "tiers") == 0)
        {
            parse_tiers(value, &cur.tiers);
        }
        else if (strcmp(key, "dps") == 0)
        {
            parse_float(value, &cur.dps_cost);
        }
        else if (strcmp(key, "control") == 0)
        {
            parse_float(value, &cur.control_cost);
        }
        else if (strcmp(key, "mobility") == 0)
        {
            parse_float(value, &cur.mobility_cost);
        }
        else if (strcmp(key, "incompat") == 0)
        {
            parse_mask(value, &cur.incompat_mask);
        }
        else if (strcmp(key, "telegraph") == 0)
        {
            cur.telegraph = rogue_strdup(value);
        }
    }
    if (cur.name && g_modifier_count < ROGUE_ENEMY_MAX_MODIFIERS)
        g_modifiers[g_modifier_count++] = cur;
    fclose(f);
    /* Basic normalization and defaults */
    for (int i = 0; i < g_modifier_count; i++)
    {
        if (g_modifiers[i].weight <= 0)
            g_modifiers[i].weight = 1.0f;
        if (g_modifiers[i].tiers == 0)
            g_modifiers[i].tiers = 0xFFFFFFFFu; /* all */
    }
    return g_modifier_count;
}

unsigned int rogue_enemy_modifiers_rng_next(unsigned int* state)
{
    unsigned int x = *state; /* xorshift32 */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int rogue_enemy_modifiers_rng_range(unsigned int* state, int hi_exclusive)
{
    if (hi_exclusive <= 0)
        return 0;
    return (int) (rogue_enemy_modifiers_rng_next(state) % (unsigned int) hi_exclusive);
}

static int modifier_allowed_for_tier(const RogueEnemyModifierDef* d, int tier)
{
    if (!d)
        return 0;
    return (d->tiers & (1u << tier)) != 0;
}

int rogue_enemy_modifiers_roll(unsigned int seed, int tier_id, float max_fraction,
                               RogueEnemyModifierSet* out)
{
    if (!out)
        return -1;
    memset(out, 0, sizeof(*out));
    if (max_fraction <= 0)
        max_fraction = 0.6f;
    unsigned int state = seed ? seed : 0xA5F4321u;
    /* Weighted selection until budget caps reached or no selectable modifiers remain */
    for (int iter = 0; iter < ROGUE_ENEMY_MAX_ACTIVE_MODS * 4; iter++)
    {
        /* gather candidates */
        float total_w = 0.f;
        for (int i = 0; i < g_modifier_count; i++)
        {
            const RogueEnemyModifierDef* d = &g_modifiers[i];
            if (!modifier_allowed_for_tier(d, tier_id))
                continue;
            if (out->applied_mask & (1u << i))
                continue; /* already selected */
            if (d->incompat_mask & out->applied_mask)
                continue; /* incompat */
            /* would exceed budget? */
            if (out->total_dps_cost + d->dps_cost > max_fraction)
                continue;
            if (out->total_control_cost + d->control_cost > max_fraction)
                continue;
            if (out->total_mobility_cost + d->mobility_cost > max_fraction)
                continue;
            total_w += d->weight;
        }
        if (total_w <= 0.f)
            break; /* stop */
        float r = (float) (rogue_enemy_modifiers_rng_next(&state) & 0xFFFFFF) / (float) 0xFFFFFF *
                  total_w;
        const RogueEnemyModifierDef* chosen = NULL;
        int chosen_index = -1;
        for (int i = 0; i < g_modifier_count; i++)
        {
            const RogueEnemyModifierDef* d = &g_modifiers[i];
            if (!modifier_allowed_for_tier(d, tier_id))
                continue;
            if (out->applied_mask & (1u << i))
                continue;
            if (d->incompat_mask & out->applied_mask)
                continue;
            if (out->total_dps_cost + d->dps_cost > max_fraction)
                continue;
            if (out->total_control_cost + d->control_cost > max_fraction)
                continue;
            if (out->total_mobility_cost + d->mobility_cost > max_fraction)
                continue;
            r -= d->weight;
            if (r <= 0.f)
            {
                chosen = d;
                chosen_index = i;
                break;
            }
        }
        if (!chosen)
            break; /* fallback */
        out->defs[out->count++] = chosen;
        out->total_dps_cost += chosen->dps_cost;
        out->total_control_cost += chosen->control_cost;
        out->total_mobility_cost += chosen->mobility_cost;
        out->applied_mask |= (1u << chosen_index);
        if (out->count >= ROGUE_ENEMY_MAX_ACTIVE_MODS)
            break;
    }
    return 0;
}
