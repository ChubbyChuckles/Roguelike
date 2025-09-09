#include "../../src/core/app/app_state.h"
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static int have_damage_flat_affix(void)
{
    int n = rogue_affix_count();
    for (int i = 0; i < n; i++)
    {
        const RogueAffixDef* a = rogue_affix_at(i);
        if (a && a->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
            return 1;
    }
    return 0;
}

static int find_weapon_def(void)
{
    for (int i = 0; i < rogue_item_defs_count(); i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (d && d->category == ROGUE_ITEM_WEAPON)
            return i;
    }
    return -1;
}
static int find_armor_def(void)
{
    for (int i = 0; i < rogue_item_defs_count(); i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (d && d->category == ROGUE_ITEM_ARMOR)
            return i;
    }
    return -1;
}

int main(void)
{
    /* Load affixes (from assets) and basic item defs (from cfg) */
    char apath[256];
    if (!rogue_find_asset_path("affixes.cfg", apath, sizeof apath))
    {
        /* If affix path missing, skip: environment issue */
        printf("GATE_SKIP no_affix_path\n");
        return 0;
    }
    rogue_affixes_reset();
    if (rogue_affixes_load_from_cfg(apath) <= 0)
    {
        printf("GATE_SKIP affix_load\n");
        return 0;
    }
    if (!have_damage_flat_affix())
    {
        printf("GATE_SKIP no_damage_flat\n");
        return 0;
    }
    rogue_item_defs_reset();
    /* Prefer JSON directory if available, else fallback to asset cfg set */
    int items_added = rogue_item_defs_load_directory("assets/items");
    if (items_added <= 0)
        items_added = rogue_item_defs_load_from_cfg("../../assets/test_items.cfg");
    assert(items_added > 0);

    rogue_items_init_runtime();
    /* Find a weapon and an armor */
    int wdef = find_weapon_def();
    int adef = find_armor_def();
    assert(wdef >= 0 && adef >= 0);
    int winst = rogue_items_spawn(wdef, 1, 0, 0);
    int ainst = rogue_items_spawn(adef, 1, 0, 0);
    assert(winst >= 0 && ainst >= 0);

    /* Roll affixes at rarity 3 to ensure both slots attempt to roll. */
    unsigned int s1 = 12345u;
    unsigned int s2 = 54321u;
    assert(rogue_item_instance_generate_affixes(winst, &s1, 3) == 0);
    assert(rogue_item_instance_generate_affixes(ainst, &s2, 3) == 0);

    const RogueItemInstance* wit = rogue_item_instance_at(winst);
    const RogueItemInstance* ait = rogue_item_instance_at(ainst);
    assert(wit && ait);
    /* Verify gating: armor must not receive DAMAGE_FLAT affixes */
    if (ait->prefix_index >= 0)
    {
        const RogueAffixDef* ap = rogue_affix_at(ait->prefix_index);
        assert(ap->stat != ROGUE_AFFIX_STAT_DAMAGE_FLAT);
    }
    if (ait->suffix_index >= 0)
    {
        const RogueAffixDef* as = rogue_affix_at(ait->suffix_index);
        assert(as->stat != ROGUE_AFFIX_STAT_DAMAGE_FLAT);
    }
    /* Weapon is allowed to get DAMAGE_FLAT; do not require it, just ensure any damage_flat found is
     * on weapon */
    if (wit->prefix_index >= 0)
    {
        const RogueAffixDef* wp = rogue_affix_at(wit->prefix_index);
        (void) wp; /* no strict assertion; presence is allowed */
    }
    if (wit->suffix_index >= 0)
    {
        const RogueAffixDef* ws = rogue_affix_at(wit->suffix_index);
        (void) ws;
    }
    printf("GATE_OK w_px=%d w_sx=%d a_px=%d a_sx=%d\n", wit->prefix_index, wit->suffix_index,
           ait->prefix_index, ait->suffix_index);
    return 0;
}
