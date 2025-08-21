#include "core/crafting/crafting_journal.h"
#include <string.h>

#ifndef ROGUE_CRAFT_JOURNAL_CAP
#define ROGUE_CRAFT_JOURNAL_CAP 4096
#endif

static RogueCraftJournalEntry g_entries[ROGUE_CRAFT_JOURNAL_CAP];
static int g_entry_count = 0;
static unsigned int g_accum_hash = 0x811C9DC5u; /* FNV offset */

static unsigned int fnv1a_step(unsigned int h, unsigned int v)
{
    h ^= v;
    h *= 0x01000193u;
    return h;
}

void rogue_craft_journal_reset(void)
{
    g_entry_count = 0;
    g_accum_hash = 0x811C9DC5u;
}

int rogue_craft_journal_append(unsigned int item_guid, unsigned int pre_budget,
                               unsigned int post_budget, unsigned int rng_stream_id,
                               unsigned int outcome_hash)
{
    if (g_entry_count >= ROGUE_CRAFT_JOURNAL_CAP)
        return -1;
    RogueCraftJournalEntry e;
    e.op_id = (unsigned int) g_entry_count;
    e.item_guid = item_guid;
    e.pre_budget = pre_budget;
    e.post_budget = post_budget;
    e.rng_stream_id = rng_stream_id;
    e.outcome_hash = outcome_hash;
    g_entries[g_entry_count++] = e;
    /* hash chain incorporate fields */
    unsigned int h = g_accum_hash;
    h = fnv1a_step(h, e.op_id);
    h = fnv1a_step(h, item_guid);
    h = fnv1a_step(h, pre_budget);
    h = fnv1a_step(h, post_budget);
    h = fnv1a_step(h, rng_stream_id);
    h = fnv1a_step(h, outcome_hash);
    g_accum_hash = h;
    return (int) e.op_id;
}

int rogue_craft_journal_count(void) { return g_entry_count; }
const RogueCraftJournalEntry* rogue_craft_journal_entry(int index)
{
    if (index < 0 || index >= g_entry_count)
        return NULL;
    return &g_entries[index];
}
unsigned int rogue_craft_journal_accum_hash(void) { return g_accum_hash; }
