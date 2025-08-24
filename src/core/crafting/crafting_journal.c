/**
 * @file src/core/crafting/crafting_journal.c
 * @brief Append-only crafting operation journal for deterministic replay and debugging.
 *
 * This module records compact entries describing crafting operations. Each entry
 * holds an operation id, item GUID, pre/post budgets, RNG stream id and an
 * outcome hash. The module maintains an accumulated 32-bit FNV-1a hash over
 * recorded fields that can be used for quick integrity checks or to verify
 * deterministic replays.
 */

#include "crafting_journal.h"
#include <string.h>

#ifndef ROGUE_CRAFT_JOURNAL_CAP
/**
 * @brief Maximum number of journal entries. Can be overridden at compile time.
 */
#define ROGUE_CRAFT_JOURNAL_CAP 4096
#endif

/** @brief Storage for journal entries (append-only buffer). */
static RogueCraftJournalEntry g_entries[ROGUE_CRAFT_JOURNAL_CAP];

/** @brief Number of entries currently stored in the journal. */
static int g_entry_count = 0;

/**
 * @brief Accumulated 32-bit FNV-1a hash over recorded fields.
 *
 * Initialized to the FNV offset basis. See fnv1a_step() for update logic.
 */
static unsigned int g_accum_hash = 0x811C9DC5u; /* FNV offset */

/**
 * @brief Perform one step of the 32-bit FNV-1a hash over a 32-bit value.
 * @param h Current hash value.
 * @param v 32-bit value to incorporate.
 * @return Updated hash value.
 */
static unsigned int fnv1a_step(unsigned int h, unsigned int v)
{
    h ^= v;
    h *= 0x01000193u;
    return h;
}

/**
 * @brief Reset the crafting journal to an empty state.
 *
 * Clears stored entries and resets the accumulated FNV-1a hash to the offset
 * basis. Useful when beginning a new deterministic session or when replaying.
 */
void rogue_craft_journal_reset(void)
{
    g_entry_count = 0;
    g_accum_hash = 0x811C9DC5u;
}

/**
 * @brief Append a crafting operation to the journal.
 * @param item_guid GUID of the item involved in the operation.
 * @param pre_budget Resource budget before the operation.
 * @param post_budget Resource budget after the operation.
 * @param rng_stream_id RNG stream identifier used for the operation.
 * @param outcome_hash Release-specific or operation-specific outcome hash.
 * @return On success returns the operation id (>=0). Returns -1 if the journal
 *         is full and the append cannot be performed.
 *
 * Records the provided fields into the append-only buffer and updates the
 * running FNV-1a hash so callers can later validate or fingerprint the
 * recorded sequence.
 */
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

/** @brief Get the number of recorded journal entries. */
int rogue_craft_journal_count(void) { return g_entry_count; }

/**
 * @brief Return a pointer to the journal entry at the given index.
 * @param index Zero-based entry index.
 * @return Pointer to the entry or NULL if index is out of range.
 */
const RogueCraftJournalEntry* rogue_craft_journal_entry(int index)
{
    if (index < 0 || index >= g_entry_count)
        return NULL;
    return &g_entries[index];
}

/** @brief Return the accumulated FNV-1a hash across all recorded fields. */
unsigned int rogue_craft_journal_accum_hash(void) { return g_accum_hash; }
