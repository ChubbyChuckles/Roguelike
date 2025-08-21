/* Phase 12: Progression persistence & migration tests */
#include "core/progression/progression_attributes.h"
#include "core/progression/progression_persist.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern struct AppState
{
    int level;
    unsigned long long xp_total_accum;
} g_app;
extern RogueAttributeState g_attr_state;

static void init_state(void)
{
    g_app.level = 15;
    g_app.xp_total_accum = 123456ull;
    g_attr_state.strength = 5;
    g_attr_state.dexterity = 3;
    g_attr_state.vitality = 2;
    g_attr_state.intelligence = 1;
    g_attr_state.spent_points = 7;
    g_attr_state.respec_tokens = 2;
}

/* Local copy of legacy V1 header to craft a synthetic old save. */
typedef struct LegacyProgHeaderV1
{
    uint32_t version;
    uint32_t level;
    uint64_t xp_total;
    uint32_t attr_str, attr_dex, attr_vit, attr_int;
    uint32_t unspent_pts;
    uint32_t respec_tokens;
    uint64_t attr_journal_hash;
    uint64_t passive_journal_hash;
    uint32_t passive_entry_count;
} LegacyProgHeaderV1;

int main(void)
{
    rogue_progression_persist_reset_state_for_tests();
    rogue_progression_persist_register();
    init_state();
    FILE* f = NULL;
#if defined(_MSC_VER)
    tmpfile_s(&f);
#else
    f = tmpfile();
#endif
    assert(f);
    assert(rogue_progression_persist_write(f) == 0);
    unsigned long long chainA = rogue_progression_persist_chain_hash();
    fseek(f, 0, SEEK_SET);
    g_app.level = 1;
    g_app.xp_total_accum = 0;
    g_attr_state.strength = 0;
    g_attr_state.dexterity = 0;
    g_attr_state.vitality = 0;
    g_attr_state.intelligence = 0;
    g_attr_state.spent_points = 0;
    g_attr_state.respec_tokens = 0;
    assert(rogue_progression_persist_read(f) == 0);
    assert(g_app.level == 15 && g_attr_state.strength == 5);
    unsigned long long chainB = rogue_progression_persist_chain_hash();
    assert(chainA == chainB);
    /* Legacy v1 simulation */
    LegacyProgHeaderV1 legacy;
    memset(&legacy, 0, sizeof legacy);
    legacy.version = 1;
    legacy.level = 20;
    legacy.xp_total = 999;
    legacy.attr_str = 9;
    legacy.attr_dex = 8;
    legacy.attr_vit = 7;
    legacy.attr_int = 6;
    legacy.unspent_pts = 4;
    legacy.respec_tokens = 3;
    legacy.attr_journal_hash = 111;
    legacy.passive_journal_hash = 222;
    legacy.passive_entry_count = 0;
    FILE* f2 = NULL;
#if defined(_MSC_VER)
    tmpfile_s(&f2);
#else
    f2 = tmpfile();
#endif
    assert(f2);
    fwrite(&legacy, sizeof legacy, 1, f2);
    fseek(f2, 0, SEEK_SET);
    assert(rogue_progression_persist_read(f2) == 0);
    assert(g_app.level == 20 && g_attr_state.dexterity == 8);
    unsigned int mig = rogue_progression_persist_last_migration_flags();
    (void) mig;
    printf("progression_phase12_persistence: OK\n");
    return 0;
}
