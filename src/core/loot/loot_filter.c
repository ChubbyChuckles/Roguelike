#define _CRT_SECURE_NO_WARNINGS 1
#include "core/loot/loot_filter.h"
#include "core/loot/loot_instances.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ROGUE_LOOT_FILTER_MAX_RULES
#define ROGUE_LOOT_FILTER_MAX_RULES 64
#endif

typedef enum
{
    LF_RARITY_MIN,
    LF_RARITY_MAX,
    LF_CATEGORY_EQ,
    LF_NAME_SUBSTR,
    LF_DEF_ID
} LootFilterRuleType;

typedef struct LootFilterRule
{
    LootFilterRuleType type;
    int ival;      /* rarity threshold, category number */
    char sval[32]; /* substring or id */
} LootFilterRule;

static LootFilterRule g_rules[ROGUE_LOOT_FILTER_MAX_RULES];
static int g_rule_count = 0;
static int g_mode_any = 0; /* 0 = ALL (AND), 1 = ANY (OR) */

static int str_ieq(const char* a, const char* b)
{
    while (*a && *b)
    {
        if (tolower((unsigned char) *a) != tolower((unsigned char) *b))
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}
static int contains_ic(const char* hay, const char* needle)
{
    if (!hay || !needle || !*needle)
        return 0;
    size_t nlen = strlen(needle);
    for (const char* p = hay; *p; p++)
    {
        if (tolower((unsigned char) *p) == tolower((unsigned char) needle[0]))
        {
            size_t i = 1;
            while (i < nlen && p[i] &&
                   tolower((unsigned char) p[i]) == tolower((unsigned char) needle[i]))
                i++;
            if (i == nlen)
                return 1;
        }
    }
    return 0;
}

int rogue_loot_filter_reset(void)
{
    g_rule_count = 0;
    g_mode_any = 0;
    return 0;
}
int rogue_loot_filter_rule_count(void) { return g_rule_count; }

static int parse_category(const char* v)
{
    if (str_ieq(v, "weapon"))
        return ROGUE_ITEM_WEAPON;
    if (str_ieq(v, "armor"))
        return ROGUE_ITEM_ARMOR;
    if (str_ieq(v, "material"))
        return ROGUE_ITEM_MATERIAL;
    if (str_ieq(v, "consumable"))
        return ROGUE_ITEM_CONSUMABLE;
    return atoi(v); /* fallback numeric */
}

int rogue_loot_filter_load(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        fprintf(stderr, "loot_filter: cannot open %s\n", path);
        return -1;
    }
    char line[256];
    int added = 0;
    while (fgets(line, sizeof line, f))
    {
        char* p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0')
            continue; /* skip */
        size_t len = strlen(p);
        while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r'))
            p[--len] = '\0';
        if (str_ieq(p, "MODE=ANY"))
        {
            g_mode_any = 1;
            continue;
        }
        if (str_ieq(p, "MODE=ALL"))
        {
            g_mode_any = 0;
            continue;
        }
        if (g_rule_count >= ROGUE_LOOT_FILTER_MAX_RULES)
        {
            fprintf(stderr, "loot_filter: capacity reached\n");
            break;
        }
        LootFilterRule r;
        memset(&r, 0, sizeof r);
        if (strncmp(p, "rarity>=", 8) == 0)
        {
            r.type = LF_RARITY_MIN;
            r.ival = atoi(p + 8);
        }
        else if (strncmp(p, "rarity<=", 8) == 0)
        {
            r.type = LF_RARITY_MAX;
            r.ival = atoi(p + 8);
        }
        else if (strncmp(p, "category=", 9) == 0)
        {
            r.type = LF_CATEGORY_EQ;
            r.ival = parse_category(p + 9);
        }
        else if (strncmp(p, "name~", 5) == 0)
        {
            r.type = LF_NAME_SUBSTR;
            memset(r.sval, 0, sizeof r.sval);
            strncpy(r.sval, p + 5, sizeof(r.sval) - 1);
        }
        else if (strncmp(p, "def=", 4) == 0)
        {
            r.type = LF_DEF_ID;
            memset(r.sval, 0, sizeof r.sval);
            strncpy(r.sval, p + 4, sizeof(r.sval) - 1);
        }
        else
        {
            fprintf(stderr, "loot_filter: unknown rule %s\n", p);
            continue;
        }
        g_rules[g_rule_count++] = r;
        added++;
    }
    fclose(f);
    return added;
}

static int rule_match(const LootFilterRule* r, const RogueItemDef* d)
{
    if (!r || !d)
        return 0;
    switch (r->type)
    {
    case LF_RARITY_MIN:
        return d->rarity >= r->ival;
    case LF_RARITY_MAX:
        return d->rarity <= r->ival;
    case LF_CATEGORY_EQ:
        return d->category == r->ival;
    case LF_NAME_SUBSTR:
        return contains_ic(d->name, r->sval);
    case LF_DEF_ID:
        return str_ieq(d->id, r->sval);
    default:
        return 0;
    }
}

int rogue_loot_filter_match(const RogueItemDef* def)
{
    if (!def)
        return -1;
    if (g_rule_count == 0)
        return 1;
    for (int i = 0; i < g_rule_count; i++)
    {
        int m = rule_match(&g_rules[i], def);
        if (g_mode_any)
        {
            if (m)
                return 1;
        }
        else
        {
            if (!m)
                return 0;
        }
    }
    return g_mode_any ? 0 : 1;
}

void rogue_loot_filter_refresh_instances(void) { rogue_items_reapply_filter(); }
