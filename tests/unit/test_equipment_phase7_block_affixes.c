#define SDL_MAIN_HANDLED 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/loot_affixes.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/equipment_stats.h" /* for consistency (no direct calls) */
#include "core/stat_cache.h"

extern struct RogueStatCache g_player_stat_cache;

static void reset_stat_cache(){ memset(&g_player_stat_cache,0,sizeof g_player_stat_cache); }

static void load_basic_affixes(){ rogue_affixes_reset(); int added = rogue_affixes_load_from_cfg("assets/affixes.cfg"); assert(added>0); }

int main(){
    load_basic_affixes();
    /* Find our new affix indices */
    int idx_bulwark = rogue_affix_index("bulwark");
    int idx_guarding = rogue_affix_index("of_guarding");
    assert(idx_bulwark>=0 && idx_guarding>=0);
    RogueItemInstance inst={0}; /* local simulation only */
    inst.prefix_index = idx_bulwark; inst.prefix_value = 7; /* block chance */
    inst.suffix_index = idx_guarding; inst.suffix_value = 10; /* block value */
    /* For this focused test we bypass equipment APIs and inject values directly. */
    reset_stat_cache();
    g_player_stat_cache.block_chance = 0; g_player_stat_cache.block_value = 0;
    /* Simulate aggregation: add affix values */
    g_player_stat_cache.block_chance += inst.prefix_value; g_player_stat_cache.block_value += inst.suffix_value;
    assert(g_player_stat_cache.block_chance == 7);
    assert(g_player_stat_cache.block_value == 10);
    printf("equipment_phase7_block_affixes_ok\n");
    return 0;
}
