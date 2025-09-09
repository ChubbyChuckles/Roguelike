/* Phase 10.1-10.3: Upgrade stones, affix transfer orb, fusion tests */
#define SDL_MAIN_HANDLED 1
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>

typedef struct RogueItemInstance RogueItemInstance; /* forward for local stub casts */

int rogue_minimap_ping_loot(float x, float y, int r)
{
    (void) x;
    (void) y;
    (void) r;
    return 0;
}
void rogue_stat_cache_mark_dirty(void) {}
/* Removed deterministic affix stubs: use real affix tables + gating. */

/* Minimal item def table: weapon (index 0), armor (1), blank orb (2) generic container */
static RogueItemDef defs[3];
const RogueItemDef* rogue_item_def_at(int idx)
{
    if (idx >= 0 && idx < 3)
        return &defs[idx];
    return NULL;
}
int rogue_item_def_index(const char* id)
{
    (void) id;
    return -1;
}

static unsigned int rng_state = 12345u;

static int spawn_with_affixes(int def_index, int rarity)
{
    int inst = rogue_items_spawn(def_index, 1, 0, 0);
    assert(inst >= 0);
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
    it->rarity = rarity;
    rogue_item_instance_generate_affixes(inst, &rng_state, rarity);
    return inst;
}

int main(void)
{
    defs[0].category = ROGUE_ITEM_WEAPON;
    defs[0].base_damage_min = 4;
    defs[0].base_damage_max = 8;
    defs[0].rarity = 2;
    defs[0].stack_max = 1;
    defs[1].category = ROGUE_ITEM_ARMOR;
    defs[1].rarity = 1;
    defs[1].stack_max = 1; /* armor baseline */
    defs[2].category = ROGUE_ITEM_MISC;
    defs[2].stack_max = 10; /* orb container */
    rogue_items_init_runtime();
    /* Load real affix definitions so generation produces prefixes/suffixes. */
    rogue_affixes_reset();
    int loaded = rogue_affixes_load_from_cfg("assets/affixes.cfg");
    assert(loaded > 0);

    /* 10.1 Upgrade stones: ensure level increases and budget elevation allows affix growth */
    int w = spawn_with_affixes(0, 3);
    RogueItemInstance* w_it = (RogueItemInstance*) rogue_item_instance_at(w);
    int before_level = w_it->item_level;
    int before_total = rogue_item_instance_total_affix_weight(w);
    int upgrade_rc = rogue_item_instance_apply_upgrade_stone(w, 3, &rng_state);
    assert(upgrade_rc == 0);
    assert(w_it->item_level == before_level + 3);
    int after_total = rogue_item_instance_total_affix_weight(w);
    assert(after_total >= before_total); /* may increase */

    /* 10.2 Affix extraction to orb */
    int orb = rogue_items_spawn(2, 1, 0, 0);
    assert(orb >= 0);
    int had_prefix = (w_it->prefix_index >= 0);
    int extract_rc = rogue_item_instance_affix_extract(w, had_prefix ? 1 : 0, orb);
    assert(extract_rc == 0);
    if (had_prefix)
        assert(w_it->prefix_index < 0);
    else
        assert(w_it->suffix_index < 0);
    RogueItemInstance* orb_it = (RogueItemInstance*) rogue_item_instance_at(orb);
    assert(orb_it->stored_affix_index >= 0);

    /* 10.2 apply orb to fresh target item */
    int armor = spawn_with_affixes(1, 2);
    RogueItemInstance* a_it =
        (RogueItemInstance*) rogue_item_instance_at(armor); /* clear one slot to simulate empty */
    a_it->prefix_index = -1;
    a_it->prefix_value = 0;
    int tgt_before = rogue_item_instance_total_affix_weight(armor);
    int apply_rc = rogue_item_instance_affix_orb_apply(orb, armor);
    assert(apply_rc == 0);
    assert(orb_it->stored_affix_used == 1);
    int tgt_after = rogue_item_instance_total_affix_weight(armor);
    assert(tgt_after > tgt_before);

    /* 10.3 Fusion: sacrifice another weapon into armor, transferring its highest affix if slot
     * vacant */
    int donor = spawn_with_affixes(0, 3);
    RogueItemInstance* d_it = (RogueItemInstance*) rogue_item_instance_at(
        donor); /* ensure armor suffix slot empty to accept possible suffix */
    a_it->suffix_index = -1;
    a_it->suffix_value = 0;
    int armor_before = rogue_item_instance_total_affix_weight(armor);
    int fuse_rc = rogue_item_instance_fusion(armor, donor);
    assert(fuse_rc == 0);
    assert(!d_it->active);
    int armor_after = rogue_item_instance_total_affix_weight(armor);
    assert(armor_after > armor_before);

    printf("equipment_phase10_crafting_ok\n");
    return 0;
}
