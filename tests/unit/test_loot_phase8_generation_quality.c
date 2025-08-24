/* Tests for generation quality scalar (8.4), gating (8.2) and duplicate avoidance (8.6) */
#include "../../src/core/app/app_state.h"
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

static int is_weapon(int def_index)
{
    const RogueItemDef* d = rogue_item_def_at(def_index);
    return d && d->category == ROGUE_ITEM_WEAPON;
}

int main(void)
{
    rogue_drop_rates_reset();
    rogue_affixes_reset();
    char apath[256];
    if (!rogue_find_asset_path("affixes.cfg", apath, sizeof apath))
        return fail("affix_path");
    if (rogue_affixes_load_from_cfg(apath) < 4)
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
    rogue_generation_set_quality_scalar(1.0f, 2.5f);
    RogueGenerationContext low_ctx;
    low_ctx.enemy_level = 5;
    low_ctx.biome_id = 0;
    low_ctx.enemy_archetype = 1;
    low_ctx.player_luck = 0;
    RogueGenerationContext high_ctx = low_ctx;
    high_ctx.player_luck = 50; /* very high luck for strong bias */
    unsigned int seedA = 777u;
    RogueGeneratedItem a;
    if (rogue_generate_item(0, &low_ctx, &seedA, &a) != 0)
        return fail("gen_low");
    unsigned int seedB = 777u;
    RogueGeneratedItem b;
    if (rogue_generate_item(0, &high_ctx, &seedB, &b) != 0)
        return fail("gen_high");
    if (a.inst_index < 0 || b.inst_index < 0)
        return fail("inst");
    const RogueItemInstance* ia = rogue_item_instance_at(a.inst_index);
    const RogueItemInstance* ib = rogue_item_instance_at(b.inst_index);
    if (!ia || !ib)
        return fail("inst_ptr");
    /* Gating: if base item not weapon, any damage_flat affix should be absent. We'll search for
     * damage stat presence only when weapon. */
    if (!is_weapon(a.def_index))
    {
        if (ia->prefix_index >= 0)
        {
            const RogueAffixDef* pa = rogue_affix_at(ia->prefix_index);
            if (pa && pa->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
                return fail("gating_damage_prefix");
        }
        if (ia->suffix_index >= 0)
        {
            const RogueAffixDef* sa = rogue_affix_at(ia->suffix_index);
            if (sa && sa->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
                return fail("gating_damage_suffix");
        }
    }
    /* Duplicate avoidance: prefix and suffix indices should not match */
    if (ib->prefix_index >= 0 && ib->suffix_index >= 0 && ib->prefix_index == ib->suffix_index)
        return fail("duplicate_affix");
    /* Quality scalar bias: with identical master seed, high luck should not produce strictly lower
     * affix values when both have that slot. We allow equality; looking for upgrades often. */
    if (ia->prefix_index >= 0 && ib->prefix_index == ia->prefix_index)
    {
        if (ib->prefix_value < ia->prefix_value)
            return fail("quality_bias_prefix");
    }
    if (ia->suffix_index >= 0 && ib->suffix_index == ia->suffix_index)
    {
        if (ib->suffix_value < ia->suffix_value)
            return fail("quality_bias_suffix");
    }
    printf("GENERATION_QUALITY_OK low_prefix=%d pv=%d high_prefix=%d pv=%d low_suffix=%d sv=%d "
           "high_suffix=%d sv=%d\n",
           ia->prefix_index, ia->prefix_value, ib->prefix_index, ib->prefix_value, ia->suffix_index,
           ia->suffix_value, ib->suffix_index, ib->suffix_value);
    return 0;
}
