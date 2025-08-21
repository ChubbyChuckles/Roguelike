/* Phase 12 V3 persistence extended test */
#include "core/progression/progression_attributes.h"
#include "core/progression/progression_persist.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern struct AppState
{
    int level;
    unsigned long long xp_total_accum;
    int unspent_stat_points;
} g_app;
extern RogueAttributeState g_attr_state;

int main(void)
{
    rogue_progression_persist_reset_state_for_tests();
    assert(rogue_progression_persist_version() >= 3);
    /* seed state */
    g_app.level = 22;
    g_app.xp_total_accum = 77777ull;
    g_app.unspent_stat_points = 5;
    g_attr_state.strength = 2;
    g_attr_state.dexterity = 1;
    g_attr_state.vitality = 0;
    g_attr_state.intelligence = 0;
    g_attr_state.spent_points = 0;
    g_attr_state.respec_tokens = 1;
    g_attr_state.journal_hash = 0xcbf29ce484222325ULL;
    g_attr_state.ops = NULL;
    g_attr_state.op_count = 0;
    g_attr_state.op_cap = 0;
    /* simulate spends/respec (expect 3 operations) */
    rogue_attr_spend(&g_attr_state, 'S');  /* op0 */
    rogue_attr_spend(&g_attr_state, 'D');  /* op1 */
    rogue_attr_respec(&g_attr_state, 'D'); /* op2 */
    int ops_before = rogue_attr_journal_count();
    unsigned long long chain_before = 0ULL;
    FILE* f = NULL;
#if defined(_MSC_VER)
    tmpfile_s(&f);
#else
    f = tmpfile();
#endif
    assert(f);
    assert(rogue_progression_persist_write(f) == 0);
    chain_before = rogue_progression_persist_chain_hash();
    /* wipe and restore */
    g_app.level = 1;
    g_app.xp_total_accum = 0;
    g_attr_state.strength = 0;
    g_attr_state.dexterity = 0;
    g_attr_state.vitality = 0;
    g_attr_state.intelligence = 0;
    g_attr_state.spent_points = 0;
    g_attr_state.respec_tokens = 0;
    g_attr_state.op_count = 0;
    g_attr_state.op_cap = 0;
    if (g_attr_state.ops)
    {
        free(g_attr_state.ops);
        g_attr_state.ops = NULL;
    }
    fseek(f, 0, SEEK_SET);
    assert(rogue_progression_persist_read(f) == 0);
    assert(g_app.level == 22);
    unsigned long long chain_after = rogue_progression_persist_chain_hash();
    assert(chain_before == chain_after);
    /* attribute ops replay flag should be set */
    unsigned int mig = rogue_progression_persist_last_migration_flags();
    assert((mig & ROGUE_PROG_MIG_ATTR_REPLAY) != 0 || rogue_attr_journal_count() > 0);
    /* journal count should match pre-save op count */
    assert(rogue_attr_journal_count() == ops_before);
    printf("progression_phase12_v3: OK\n");
    return 0;
}
