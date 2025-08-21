/* Phase 15 Integrity helpers implementation */
#include "core/equipment/equipment_integrity.h"
#include "core/equipment/equipment.h"
#include "core/equipment/equipment_procs.h"
#include "core/loot/loot_affixes.h"
#include "core/loot/loot_instances.h"

#include <string.h>

/* ---------------- Proc Rate Auditor (15.4) ---------------- */
int rogue_integrity_scan_proc_anomalies(RogueProcAnomaly* out, int max_out, float max_tpm)
{
    if (max_out < 0)
        return 0;
    int written = 0;
    int count = rogue_proc_count();
    for (int i = 0; i < count; i++)
    {
        float tpm = rogue_proc_triggers_per_min(i);
        if (tpm > max_tpm)
        {
            if (out && written < max_out)
            {
                out[written].proc_id = i;
                out[written].triggers_per_min = tpm;
            }
            written++;
        }
    }
    return written;
}

/* ---------------- Banned Affix Pairs (15.5) ---------------- */
#ifndef ROGUE_INTEGRITY_BANNED_PAIR_CAP
#define ROGUE_INTEGRITY_BANNED_PAIR_CAP 64
#endif
typedef struct BannedPair
{
    int a, b;
} BannedPair;
static BannedPair g_banned_pairs[ROGUE_INTEGRITY_BANNED_PAIR_CAP];
static int g_banned_pair_count = 0;

void rogue_integrity_clear_banned_affix_pairs(void) { g_banned_pair_count = 0; }
int rogue_integrity_add_banned_affix_pair(int affix_a, int affix_b)
{
    if (affix_a < 0 || affix_b < 0 || affix_a == affix_b)
        return -1;
    if (g_banned_pair_count >= ROGUE_INTEGRITY_BANNED_PAIR_CAP)
        return -2;
    for (int i = 0; i < g_banned_pair_count; i++)
    {
        if ((g_banned_pairs[i].a == affix_a && g_banned_pairs[i].b == affix_b) ||
            (g_banned_pairs[i].a == affix_b && g_banned_pairs[i].b == affix_a))
            return 1;
    }
    g_banned_pairs[g_banned_pair_count].a = affix_a;
    g_banned_pairs[g_banned_pair_count].b = affix_b;
    g_banned_pair_count++;
    return 0;
}
int rogue_integrity_is_item_banned(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    int a = it->prefix_index;
    int b = it->suffix_index;
    if (a < 0 || b < 0)
        return 0;
    for (int i = 0; i < g_banned_pair_count; i++)
    {
        int pa = g_banned_pairs[i].a, pb = g_banned_pairs[i].b;
        if ((a == pa && b == pb) || (a == pb && b == pa))
            return 1;
    }
    return 0;
}

/* ---------------- Equip Chain & GUID Audits (15.6) ---------------- */
static unsigned long long mix64(unsigned long long h, unsigned long long v)
{
    h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    return h;
}
unsigned long long rogue_integrity_expected_item_equip_hash(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return 0ULL;
    unsigned long long h = 0ULL;
    for (int s = 0; s < ROGUE_EQUIP__COUNT; s++)
    {
        int cur = rogue_equip_get((enum RogueEquipSlot) s);
        if (cur == inst_index)
        {
            h = mix64(h, ((unsigned long long) s << 56) ^ it->guid ^ 0xE11AFBULL);
        }
    }
    return h;
}
int rogue_integrity_scan_equip_chain_mismatches(RogueItemChainMismatch* out, int max_out)
{
    int written = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (!it)
            continue;
        unsigned long long expected = rogue_integrity_expected_item_equip_hash(i);
        if (expected != it->equip_hash_chain)
        {
            if (out && written < max_out)
            {
                out[written].inst_index = i;
                out[written].stored_chain = it->equip_hash_chain;
                out[written].expected_chain = expected;
            }
            written++;
        }
    }
    return written;
}
int rogue_integrity_scan_duplicate_guids(int* out, int max_out)
{
    unsigned long long seen[ROGUE_ITEM_INSTANCE_CAP];
    int seen_count = 0;
    int dup_written = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (!it)
            continue;
        int duplicate = 0;
        for (int j = 0; j < seen_count; j++)
        {
            if (seen[j] == it->guid)
            {
                duplicate = 1;
                break;
            }
        }
        if (duplicate)
        {
            if (out && dup_written < max_out)
                out[dup_written] = i;
            dup_written++;
        }
        else
        {
            seen[seen_count++] = it->guid;
        }
    }
    return dup_written;
}
