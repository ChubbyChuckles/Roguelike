/* Tests for advanced generation API (8.1, partial 8.3, 8.5) */
#include "../../src/core/app_state.h"
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_generation.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_rarity_adv.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/core/path_utils.h"
#include "../../src/entities/player.h"
#include <assert.h>
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static int fail(const char* m)
{
    printf("FAIL:%s\n", m);
    return 1;
}

int main(void)
{
    rogue_drop_rates_reset();
    rogue_affixes_reset();
    char apath[256];
    if (!rogue_find_asset_path("affixes.cfg", apath, sizeof apath))
        return fail("affix_path");
    if (rogue_affixes_load_from_cfg(apath) <= 0)
        return fail("affix_load");
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
        return fail("items_path");
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
        return fail("item_defs");
    rogue_loot_tables_reset();
    char ptables[256];
    if (!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables))
        return fail("tables_path");
    if (rogue_loot_tables_load_from_cfg(ptables) <= 0)
        return fail("tables");
    rogue_items_init_runtime();
    RogueGenerationContext ctx;
    ctx.enemy_level = 25;
    ctx.biome_id = 1;
    ctx.enemy_archetype = 2;
    ctx.player_luck = 5;
    unsigned int seed = 1234u;
    RogueGeneratedItem gi;
    if (rogue_generate_item(0, &ctx, &seed, &gi) != 0)
        return fail("gen");
    if (gi.def_index < 0 || gi.rarity < 0 || gi.inst_index < 0)
        return fail("gen_fields");
    /* Expect floor raised to at least 2 for level 25 context */
    if (gi.rarity < 2)
        return fail("rarity_floor_ctx");
    /* Determinism: same context & initial master seed produce same result */
    rogue_items_shutdown_runtime();
    rogue_items_init_runtime();
    unsigned int seed2 = 1234u;
    RogueGeneratedItem gi2;
    if (rogue_generate_item(0, &ctx, &seed2, &gi2) != 0)
        return fail("gen2");
    if (gi.def_index != gi2.def_index || gi.rarity != gi2.rarity)
        return fail("determinism_core");
    printf("GENERATION_BASIC_OK def=%d rarity=%d inst=%d\n", gi.def_index, gi.rarity,
           gi.inst_index);
    return 0;
}
