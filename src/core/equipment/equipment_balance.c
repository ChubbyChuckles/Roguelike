/* Equipment Phase 11.5-11.6 Implementation */
#include "equipment_balance.h"
#include <stdio.h>
#include <string.h>

/* Proc / DR analytics state */
static int g_proc_triggers_window = 0; /* triggers counted since last analyze */
static int g_proc_oversat_flag = 0;
static float g_dr_sources[16];
static int g_dr_source_count = 0;
static int g_dr_chain_flag = 0;

/* Balance variants */
static RogueBalanceParams g_variants[ROGUE_BALANCE_VARIANT_CAP];
static int g_variant_count = 0;
static const RogueBalanceParams* g_current = NULL;

static int hash_str(const char* s)
{
    unsigned int h = 2166136261u;
    while (*s)
    {
        h ^= (unsigned char) *s++;
        h *= 16777619u;
    }
    return (int) h;
}

void rogue_equipment_analytics_reset(void)
{
    g_proc_triggers_window = 0;
    g_proc_oversat_flag = 0;
    g_dr_source_count = 0;
    g_dr_chain_flag = 0;
}
void rogue_equipment_analytics_record_proc_trigger(int magnitude)
{
    (void) magnitude;
    g_proc_triggers_window++;
}
void rogue_equipment_analytics_record_dr_source(float reduction_pct)
{
    if (g_dr_source_count < (int) (sizeof g_dr_sources / sizeof g_dr_sources[0]))
        g_dr_sources[g_dr_source_count++] = reduction_pct;
}

void rogue_balance_ensure_default(void)
{
    if (g_variant_count == 0)
    {
        RogueBalanceParams def = {0};
        snprintf(def.id, sizeof def.id, "default");
        def.id_hash = hash_str(def.id);
        def.outlier_mad_mult = 5;
        def.proc_oversat_threshold = 20;
        def.dr_chain_floor = 0.2f;
        g_variants[g_variant_count++] = def;
        g_current = &g_variants[0];
    }
    if (!g_current && g_variant_count > 0)
        g_current = &g_variants[0];
}

int rogue_balance_register(const RogueBalanceParams* p)
{
    if (!p || !p->id[0])
        return -1;
    if (g_variant_count >= ROGUE_BALANCE_VARIANT_CAP)
        return -2; /* prevent duplicate id */
    for (int i = 0; i < g_variant_count; i++)
    {
        if (strcmp(g_variants[i].id, p->id) == 0)
            return -3;
    }
    g_variants[g_variant_count] = *p;
    g_variants[g_variant_count].id_hash = hash_str(p->id);
    if (!g_current)
        g_current = &g_variants[g_variant_count];
    return g_variant_count++;
}
int rogue_balance_variant_count(void) { return g_variant_count; }
int rogue_balance_select_deterministic(unsigned int seed)
{
    if (g_variant_count == 0)
    {
        rogue_balance_ensure_default();
    }
    if (g_variant_count == 0)
        return -1;
    unsigned int h = seed;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    int idx = (int) (h % (unsigned int) g_variant_count);
    g_current = &g_variants[idx];
    return idx;
}
const RogueBalanceParams* rogue_balance_current(void)
{
    rogue_balance_ensure_default();
    return g_current;
}

void rogue_equipment_analytics_analyze(void)
{
    rogue_balance_ensure_default();
    const RogueBalanceParams* cfg = g_current;
    if (!cfg)
        return; /* Proc oversaturation: simple threshold */
    g_proc_oversat_flag =
        (g_proc_triggers_window > cfg->proc_oversat_threshold)
            ? 1
            : 0; /* DR chain: compute cumulative remaining damage fraction after applying sources
                    sequentially as (1 - r/100). If result < floor => flag */
    float remain = 1.0f;
    for (int i = 0; i < g_dr_source_count; i++)
    {
        float r = g_dr_sources[i];
        if (r < 0)
            r = 0;
        if (r > 95.f)
            r = 95.f;
        remain *= (1.f - r / 100.f);
    }
    g_dr_chain_flag = (remain < cfg->dr_chain_floor) ? 1 : 0; /* reset rolling counters (window) */
    g_proc_triggers_window = 0;
    g_dr_source_count = 0;
}

int rogue_equipment_analytics_flag_proc_oversat(void) { return g_proc_oversat_flag; }
int rogue_equipment_analytics_flag_dr_chain(void) { return g_dr_chain_flag; }

int rogue_equipment_analytics_export_json(char* buf, int cap)
{
    if (!buf || cap < 8)
        return -1;
    int n = snprintf(buf, (size_t) cap, "{\"proc_oversaturation\":%d,\"dr_chain\":%d}",
                     g_proc_oversat_flag, g_dr_chain_flag);
    if (n <= 0 || n >= cap)
        return -1;
    return n;
}
