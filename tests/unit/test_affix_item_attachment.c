/* Test 7.5 item instantiation + affix attachment & 7.6 derived damage */
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_affixes.h"
#include "core/path_utils.h"
#include "core/app_state.h"
#include "entities/player.h"
#include <stdio.h>
#include <assert.h>

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

static int fail(const char* m){ fprintf(stderr,"FAIL:%s\n", m); return 1; }

int main(void){
    char apath[256]; if(!rogue_find_asset_path("affixes.cfg", apath, sizeof apath)) return fail("affix_path");
    rogue_affixes_reset(); if(rogue_affixes_load_from_cfg(apath) < 4) return fail("affix_load");
    rogue_item_defs_reset(); if(rogue_item_defs_load_from_cfg("../../assets/test_items.cfg") < 3) return fail("item_defs");
    rogue_items_init_runtime();
    int sword = rogue_item_def_index("long_sword"); if(sword<0) return fail("sword_idx");
    int inst = rogue_items_spawn(sword,1, 0.0f,0.0f); if(inst<0) return fail("spawn");
    unsigned int seed = 777u;
    /* Force rarity 3 (epic) to request both prefix+suffix */
    if(rogue_item_instance_generate_affixes(inst,&seed,3)!=0) return fail("gen");
    const RogueItemInstance* it = rogue_item_instance_at(inst); if(!it) return fail("inst_ptr");
    int dmin = rogue_item_instance_damage_min(inst); int dmax = rogue_item_instance_damage_max(inst);
    const RogueItemDef* def = rogue_item_def_at(it->def_index);
    if(!(dmin >= def->base_damage_min && dmax >= def->base_damage_max)) return fail("damage_bounds");
    /* Determinism check: recreate with same seed and expect same affix indices/values */
    rogue_items_shutdown_runtime(); rogue_items_init_runtime();
    int inst2 = rogue_items_spawn(sword,1,0,0); if(inst2<0) return fail("spawn2");
    unsigned int seed2 = 777u; if(rogue_item_instance_generate_affixes(inst2,&seed2,3)!=0) return fail("gen2");
    const RogueItemInstance* it2 = rogue_item_instance_at(inst2); if(!it2) return fail("inst2_ptr");
    if(it->prefix_index != it2->prefix_index || it->prefix_value != it2->prefix_value) return fail("det_prefix");
    if(it->suffix_index != it2->suffix_index || it->suffix_value != it2->suffix_value) return fail("det_suffix");
    printf("AFFIX_ITEM_ATTACHMENT_OK prefix=%d pv=%d suffix=%d sv=%d dmin=%d dmax=%d\n", it->prefix_index, it->prefix_value, it->suffix_index, it->suffix_value, dmin, dmax);
    return 0;
}
