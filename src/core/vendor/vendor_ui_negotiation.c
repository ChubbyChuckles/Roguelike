#include "vendor_ui_negotiation.h"
#include "vendor_registry.h"
#include <string.h>

static int cmp_ci(char a, char b)
{
    if (a >= 'A' && a <= 'Z')
        a = (char) (a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z')
        b = (char) (b - 'A' + 'a');
    return a == b;
}
static int stricmp_local(const char* a, const char* b)
{
    if (!a || !b)
        return (int) (a - b);
    while (*a && *b)
    {
        if (!cmp_ci(*a, *b))
            return (int) ((unsigned char) *a - (unsigned char) *b);
        a++;
        b++;
    }
    return (int) ((unsigned char) *a - (unsigned char) *b);
}
static int skill_tag_score(const char* tag, int str, int dex, int intl, int vit)
{
    if (!tag)
        return 0;
    if (stricmp_local(tag, "insight") == 0)
        return intl;
    if (stricmp_local(tag, "finesse") == 0)
        return dex;
    if (stricmp_local(tag, "strength") == 0)
        return str;
    if (stricmp_local(tag, "dexterity") == 0)
        return dex;
    if (stricmp_local(tag, "intelligence") == 0)
        return intl;
    if (stricmp_local(tag, "vitality") == 0)
        return vit;
    return 0;
}

int rogue_vendor_ui_negotiation_preview(const char* rule_id, int attr_str, int attr_dex,
                                        int attr_int, int attr_vit, int* out_min_pct,
                                        int* out_max_pct, float* out_success_prob)
{
    const RogueNegotiationRule* rule = NULL;
    if (rule_id && *rule_id)
        rule = rogue_negotiation_rule_find(rule_id);
    if (!rule)
    {
        if (rogue_negotiation_rule_count() <= 0)
            return 0;
        rule = rogue_negotiation_rule_at(0);
    }
    if (!rule)
        return 0;
    if (out_min_pct)
        *out_min_pct = rule->discount_min_pct;
    if (out_max_pct)
        *out_max_pct = rule->discount_max_pct;

    /* Parse tags and compute avg score */
    char buf[64];
    {
        const char* src = rule->skill_checks;
        size_t i = 0;
        for (; i < sizeof buf - 1 && src[i]; ++i)
            buf[i] = src[i];
        buf[i] = '\0';
    }
    int tag_ct = 0;
    int total_score = 0;
    char* p = buf;
    while (*p)
    {
        while (*p == ' ')
            p++;
        char* start = p;
        while (*p && *p != ' ')
            p++;
        if (*p)
        {
            *p = '\0';
            p++;
        }
        if (*start)
        {
            total_score += skill_tag_score(start, attr_str, attr_dex, attr_int, attr_vit);
            tag_ct++;
        }
    }
    int avg_score = (tag_ct > 0) ? (total_score / tag_ct) : 0;
    /* Probability that d20 + avg_score >= min_roll:
       Let need = min_roll - avg_score; success if d20 >= need. P = clamp((21 - need)/20). */
    int need = rule->min_roll - avg_score;
    float prob = 0.0f;
    if (need <= 1)
        prob = 1.0f;
    else if (need > 20)
        prob = 0.0f;
    else
        prob = (float) (21 - need) / 20.0f;
    if (prob < 0.0f)
        prob = 0.0f;
    if (prob > 1.0f)
        prob = 1.0f;
    if (out_success_prob)
        *out_success_prob = prob;
    return 1;
}
