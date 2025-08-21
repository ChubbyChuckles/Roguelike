/* Minimal standalone test (no shared harness) mirroring gem test style */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/equipment/equipment_enchant.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_affixes.h"
#include "core/vendor/economy.h"
#include "core\inventory\inventory.h"
#include <string.h>

static void ensure_materials(){
    int orb = rogue_item_def_index("enchant_orb");
    int hammer = rogue_item_def_index("reforge_hammer");
    assert(orb>=0 && hammer>=0);
    rogue_inventory_add(orb, 10);
    rogue_inventory_add(hammer, 10);
    rogue_econ_add_gold(100000); /* plenty */
}

static int make_test_item(){
    int def = rogue_item_def_index("long_sword");
    assert(def>=0);
    unsigned int seed=12345u;
    int inst = rogue_items_spawn(def, 1, 0.f, 0.f);
    assert(inst>=0);
    return inst;
}
int main(void){
    /* Reset subsystems similar to gem test approach */
    rogue_item_defs_reset();
    int base_added = rogue_item_defs_load_from_cfg("assets/test_items.cfg");
    if(base_added<=0) base_added = rogue_item_defs_load_from_cfg("../assets/test_items.cfg");
    assert(base_added>0);
    /* Add synthetic material defs if missing by loading supplemental cfg if exists */
    int orb = rogue_item_def_index("enchant_orb");
    int hammer = rogue_item_def_index("reforge_hammer");
    if(orb<0 || hammer<0){
        int extra = rogue_item_defs_load_from_cfg("assets/test_materials.cfg");
        if(extra<=0) extra = rogue_item_defs_load_from_cfg("../assets/test_materials.cfg");
        /* materials optional file */
    }
    /* If still missing, we cannot proceed; skip gracefully */
    orb = rogue_item_def_index("enchant_orb"); hammer = rogue_item_def_index("reforge_hammer");
    if(orb<0){ printf("skipped_enchant_reforge_missing_materials\n"); return 0; }
    ensure_materials();
    int inst = make_test_item();
    RogueItemInstance* it=(RogueItemInstance*)rogue_item_instance_at(inst);
    it->rarity = 3; unsigned int rng=777; it->prefix_index=rogue_affix_roll(ROGUE_AFFIX_PREFIX,it->rarity,&rng); it->prefix_value=rogue_affix_roll_value(it->prefix_index,&rng); it->suffix_index=rogue_affix_roll(ROGUE_AFFIX_SUFFIX,it->rarity,&rng); it->suffix_value=rogue_affix_roll_value(it->suffix_index,&rng);
    int old_prefix = it->prefix_index; int old_suffix=it->suffix_index; int cost=-1; int rc=0;
    rc = rogue_item_instance_enchant(inst,1,0,&cost); assert(rc==0 && cost>0 && it->prefix_index!=old_prefix && it->suffix_index==old_suffix);
    old_prefix = it->prefix_index; old_suffix=it->suffix_index; cost=-1;
    rc = rogue_item_instance_enchant(inst,1,1,&cost); assert(rc==0 && it->prefix_index!=old_prefix && it->suffix_index!=old_suffix);
    int level_before=it->item_level; int sockets_before=it->socket_count; int rarity_before=it->rarity;
    cost=-1; rc = rogue_item_instance_reforge(inst,&cost); assert(rc==0 && it->item_level==level_before && it->socket_count==sockets_before && it->rarity==rarity_before);
    printf("equipment_phase5_enchant_reforge_ok\n");
    return 0;
}
