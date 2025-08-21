#ifndef ROGUE_CRAFTING_JOURNAL_H
#define ROGUE_CRAFTING_JOURNAL_H
/* Phase 7: outcome hash & append-only log for crafting/enhancement */
typedef struct RogueCraftJournalEntry {
    unsigned int op_id; /* incrementing */
    unsigned int item_guid;
    unsigned int pre_budget;
    unsigned int post_budget;
    unsigned int rng_stream_id;
    unsigned int outcome_hash; /* 32-bit hash */
} RogueCraftJournalEntry;

int rogue_craft_journal_append(unsigned int item_guid, unsigned int pre_budget, unsigned int post_budget, unsigned int rng_stream_id, unsigned int outcome_hash);
int rogue_craft_journal_count(void);
const RogueCraftJournalEntry* rogue_craft_journal_entry(int index);
unsigned int rogue_craft_journal_accum_hash(void);
void rogue_craft_journal_reset(void);

#endif
