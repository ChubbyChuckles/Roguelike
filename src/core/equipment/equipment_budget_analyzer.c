/* Phase16.5 Budget Analyzer implementation */
#include "equipment_budget_analyzer.h"
#include "../loot/loot_instances.h" /* for item instances & budget helpers */
#include <stdio.h>
#include <string.h>

static RogueBudgetReport g_last_report;
static int g_has_report = 0;

void rogue_budget_analyzer_reset(void)
{
    memset(&g_last_report, 0, sizeof(g_last_report));
    g_has_report = 0;
}

static int bucket_index(float ratio)
{
    if (ratio < 0.25f)
        return 0;
    if (ratio < 0.5f)
        return 1;
    if (ratio < 0.75f)
        return 2;
    if (ratio < 0.90f)
        return 3;
    if (ratio <= 1.0f)
        return 4;
    return 5;
}

void rogue_budget_analyzer_run(RogueBudgetReport* out)
{
    RogueBudgetReport r;
    memset(&r, 0, sizeof(r));
    int count_cap = ROGUE_ITEM_INSTANCE_CAP; /* iterate full pool; use active instances */
    for (int i = 0; i < count_cap; i++)
    {
        const RogueItemInstance* inst = rogue_item_instance_at(i);
        if (!inst)
            continue; /* skip inactive */
        float max_budget = (float) rogue_budget_max(inst->item_level, inst->rarity);
        if (max_budget <= 0.0f)
            continue; /* skip invalid config */
        int weight = rogue_item_instance_total_affix_weight(i);
        float ratio = max_budget > 0 ? (float) weight / max_budget : 0.f;
        if (ratio > r.max_utilization)
        {
            r.max_utilization = ratio;
            r.max_item_index = i;
        }
        r.avg_utilization += ratio;
        if (weight > (int) max_budget)
            r.over_budget_count++;
        int bi = bucket_index(ratio);
        if (bi >= 0 && bi < 6)
            r.bucket_counts[bi]++;
        r.item_count++;
    }
    if (r.item_count > 0)
        r.avg_utilization /= (float) r.item_count;
    else
        r.avg_utilization = 0.f;
    g_last_report = r;
    g_has_report = 1;
    if (out)
        *out = r;
}

const RogueBudgetReport* rogue_budget_analyzer_last(void)
{
    return g_has_report ? &g_last_report : 0;
}

int rogue_budget_analyzer_export_json(char* buf, int cap)
{
    if (!buf || cap <= 0)
        return -1;
    if (!g_has_report)
    {
        if (cap > 0)
            buf[0] = '\0';
        return 0;
    }
    const RogueBudgetReport* r = &g_last_report;
    /* deterministic key order */
    int n =
        snprintf(buf, (size_t) cap,
                 "{\n"
                 "  \"item_count\":%d,\n"
                 "  \"over_budget_count\":%d,\n"
                 "  \"avg_utilization\":%.4f,\n"
                 "  \"max_utilization\":%.4f,\n"
                 "  \"max_item_index\":%d,\n"
                 "  \"buckets\":{\n"
                 "    \"lt25\":%d,\n"
                 "    \"lt50\":%d,\n"
                 "    \"lt75\":%d,\n"
                 "    \"lt90\":%d,\n"
                 "    \"le100\":%d,\n"
                 "    \"gt100\":%d\n"
                 "  }\n"
                 "}\n",
                 r->item_count, r->over_budget_count, r->avg_utilization, r->max_utilization,
                 r->max_item_index, r->bucket_counts[0], r->bucket_counts[1], r->bucket_counts[2],
                 r->bucket_counts[3], r->bucket_counts[4], r->bucket_counts[5]);
    if (n >= cap)
    { /* truncated */
        if (cap > 0)
            buf[cap - 1] = '\0';
    }
    return n;
}
