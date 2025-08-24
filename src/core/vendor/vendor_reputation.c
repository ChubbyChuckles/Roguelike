#include "vendor_reputation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef ROGUE_VENDOR_REP_MAX
#define ROGUE_VENDOR_REP_MAX 100000
#endif

static RogueVendorRepState g_rep_states[ROGUE_MAX_VENDOR_DEFS];
static int g_rep_state_count = 0;

static RogueVendorRepState* ensure_state(int vendor_def_index)
{
    if (vendor_def_index < 0 || vendor_def_index >= rogue_vendor_def_count())
        return NULL;
    for (int i = 0; i < g_rep_state_count; i++)
        if (g_rep_states[i].vendor_def_index == vendor_def_index)
            return &g_rep_states[i];
    if (g_rep_state_count >= ROGUE_MAX_VENDOR_DEFS)
        return NULL;
    RogueVendorRepState* st = &g_rep_states[g_rep_state_count++];
    memset(st, 0, sizeof *st);
    st->vendor_def_index = vendor_def_index;
    st->reputation = 0;
    st->nego_attempts = 0;
    st->lockout_expires_ms = 0;
    st->last_discount_pct = 0;
    return st;
}

void rogue_vendor_rep_system_reset(void)
{
    g_rep_state_count = 0;
    for (int i = 0; i < ROGUE_MAX_VENDOR_DEFS; i++)
    {
        g_rep_states[i].vendor_def_index = -1;
    }
}

static float logistic_scalar(float x)
{
    if (x < 0.0f)
        x = 0.0f;
    if (x > 1.0f)
        x = 1.0f;
    const float k = 6.0f;
    const float m = 0.5f;
    float val = 1.0f / (1.0f + expf(k * (x - m)));
    const float at0 = 1.0f / (1.0f + expf(k * (0.0f - m)));
    const float at1 = 1.0f / (1.0f + expf(k * (1.0f - m)));
    const float min_scale = 0.15f;
    float norm = (val - at1) / (at0 - at1);
    if (norm < 0.0f)
        norm = 0.0f;
    if (norm > 1.0f)
        norm = 1.0f;
    return min_scale + (1.0f - min_scale) * norm;
}

static int next_tier_threshold(int vendor_def_index, int current_rep)
{
    (void) vendor_def_index; /* vendor-specific thresholds not yet differentiated */
    int best = -1;
    for (int i = 0; i < rogue_rep_tier_count(); i++)
    {
        const RogueRepTier* rt = rogue_rep_tier_at(i);
        if (!rt)
            continue;
        if (rt->rep_min > current_rep)
        {
            if (best < 0 || rt->rep_min < best)
                best = rt->rep_min;
        }
    }
    return best;
}

int rogue_vendor_rep_gain(int vendor_def_index, int base_amount)
{
    if (base_amount <= 0)
        return 0;
    RogueVendorRepState* st = ensure_state(vendor_def_index);
    if (!st)
        return 0;
    if (st->reputation >= ROGUE_VENDOR_REP_MAX)
        return 0;
    int next_thr = next_tier_threshold(vendor_def_index, st->reputation);
    float scale = 1.0f;
    if (next_thr > 0)
    {
        float frac = (float) st->reputation / (float) next_thr;
        scale = logistic_scalar(frac);
    }
    int delta = (int) floorf((float) base_amount * scale + 0.5f);
    if (delta < 1)
        delta = 1;
    st->reputation += delta;
    if (st->reputation > ROGUE_VENDOR_REP_MAX)
        st->reputation = ROGUE_VENDOR_REP_MAX;
    return delta;
}

void rogue_vendor_rep_adjust_raw(int vendor_def_index, int delta)
{
    RogueVendorRepState* st = ensure_state(vendor_def_index);
    if (!st)
        return;
    st->reputation += delta;
    if (st->reputation < 0)
        st->reputation = 0;
    if (st->reputation > ROGUE_VENDOR_REP_MAX)
        st->reputation = ROGUE_VENDOR_REP_MAX;
}

int rogue_vendor_rep_current_tier(int vendor_def_index)
{
    RogueVendorRepState* st = ensure_state(vendor_def_index);
    if (!st)
        return -1;
    int tier = -1;
    for (int i = 0; i < rogue_rep_tier_count(); i++)
    {
        const RogueRepTier* rt = rogue_rep_tier_at(i);
        if (!rt)
            continue;
        if (st->reputation >= rt->rep_min)
        {
            if (tier < 0 || rt->rep_min > rogue_rep_tier_at(tier)->rep_min)
                tier = i;
        }
    }
    return tier;
}

float rogue_vendor_rep_progress(int vendor_def_index)
{
    RogueVendorRepState* st = ensure_state(vendor_def_index);
    if (!st)
        return 0.0f;
    int next_thr = next_tier_threshold(vendor_def_index, st->reputation);
    if (next_thr <= 0)
        return 0.0f;
    int cur_min = 0;
    int cur_idx = rogue_vendor_rep_current_tier(vendor_def_index);
    if (cur_idx >= 0)
    {
        const RogueRepTier* rt = rogue_rep_tier_at(cur_idx);
        if (rt)
            cur_min = rt->rep_min;
    }
    if (next_thr <= cur_min)
        return 0.0f;
    float frac = (float) (st->reputation - cur_min) / (float) (next_thr - cur_min);
    if (frac < 0.0f)
        frac = 0.0f;
    if (frac > 1.0f)
        frac = 1.0f;
    return frac;
}

static unsigned int deterministic_attempt_seed(unsigned int session_seed, int vendor_def_index,
                                               unsigned int attempt_index)
{
    unsigned int h = session_seed ^ (vendor_def_index * 0x9E3779B9u);
    h ^= (attempt_index + 1) * 0x85EBCA6Bu;
    h ^= (h >> 13);
    h *= 0xC2B2AE35u;
    h ^= (h >> 16);
    return h ? h : 0xA136AAADu;
}

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

int rogue_vendor_attempt_negotiation(int vendor_def_index, const char* negotiation_rule_id,
                                     int attr_str, int attr_dex, int attr_int, int attr_vit,
                                     unsigned int now_ms, unsigned int session_seed,
                                     int* out_success, int* out_locked)
{
    if (out_success)
        *out_success = 0;
    if (out_locked)
        *out_locked = 0;
    RogueVendorRepState* st = ensure_state(vendor_def_index);
    if (!st)
        return 0;
    const RogueNegotiationRule* rule = rogue_negotiation_rule_find(negotiation_rule_id);
    if (!rule)
        return 0;
    if (now_ms < st->lockout_expires_ms)
    {
        if (out_locked)
            *out_locked = 1;
        return 0;
    }
    unsigned int seed =
        deterministic_attempt_seed(session_seed, vendor_def_index, st->nego_attempts++);
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
    int roll = (int) (seed % 20u) + 1 + avg_score;
    int success = (roll >= rule->min_roll);
    int discount = 0;
    if (success)
    {
        unsigned int span =
            (rule->discount_max_pct >= rule->discount_min_pct)
                ? (unsigned int) (rule->discount_max_pct - rule->discount_min_pct + 1)
                : 1u;
        discount = rule->discount_min_pct + (int) ((seed >> 8) % span);
        st->last_discount_pct = discount;
        if (out_success)
            *out_success = 1;
        rogue_vendor_rep_gain(vendor_def_index, 2);
        st->lockout_expires_ms = now_ms + 5000u;
    }
    else
    {
        rogue_vendor_rep_adjust_raw(vendor_def_index, -1);
        st->lockout_expires_ms = now_ms + 10000u;
        st->last_discount_pct = 0;
    }
    return discount;
}

int rogue_vendor_rep_last_discount(int vendor_def_index)
{
    RogueVendorRepState* st = ensure_state(vendor_def_index);
    if (!st)
        return 0;
    return st->last_discount_pct;
}
int rogue_vendor_rep_state_count(void) { return g_rep_state_count; }
const RogueVendorRepState* rogue_vendor_rep_state_at(int idx)
{
    if (idx < 0 || idx >= g_rep_state_count)
        return NULL;
    return &g_rep_states[idx];
}
