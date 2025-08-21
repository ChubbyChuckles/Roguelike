/* Equipment Phase 3.1/3.2/3.4 item_level + budget governance tests */
#include <assert.h>
#include <stdio.h>
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_affixes.h"
#include "core/app_state.h"

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){(void)p;}

static void seed_affixes(void){
    rogue_affixes_reset();
    FILE* f=fopen("affix_budget_tmp.cfg","wb");
    /* Two flat damage affixes: small and large allowing weight sum comparisons */
    fprintf(f,"PREFIX,flat_small,damage_flat,3,3,10,10,10,10,10\n");
    fprintf(f,"SUFFIX,flat_large,damage_flat,15,15,10,10,10,10,10\n");
    fclose(f);
    rogue_affixes_load_from_cfg("affix_budget_tmp.cfg");
}

static void seed_items(void){
    rogue_item_defs_reset();
    FILE* f=fopen("item_budget_tmp.cfg","wb");
    fprintf(f,"budget_sword,BudgetSword,2,1,1,10,5,7,0,sheet.png,0,0,1,1,1\n");
    fclose(f);
    rogue_item_defs_load_from_cfg("item_budget_tmp.cfg");
}

static int spawn_with_affixes(const char* id,const char* pre,const char* suf){
    int di=rogue_item_def_index(id); assert(di>=0);
    int inst=rogue_items_spawn(di,1,0,0); assert(inst>=0);
    RogueItemInstance* it=(RogueItemInstance*)rogue_item_instance_at(inst);
    if(pre){ int ai=rogue_affix_index(pre); assert(ai>=0); it->prefix_index=ai; it->prefix_value=rogue_affix_at(ai)->min_value; }
    if(suf){ int ai=rogue_affix_index(suf); assert(ai>=0); it->suffix_index=ai; it->suffix_value=rogue_affix_at(ai)->min_value; }
    return inst;
}

static void test_budget_validation(void){
    int inst = spawn_with_affixes("budget_sword","flat_small","flat_large");
    RogueItemInstance* it=(RogueItemInstance*)rogue_item_instance_at(inst);
    it->item_level=1; it->rarity=1; /* cap = 20 + 1*5 + 1^2*10 = 35; total weight = 3+15=18 -> valid */
    assert(rogue_item_instance_validate_budget(inst)==0);
    /* Force over budget by manual inflation */
    it->prefix_value = 30; it->suffix_value = 30; /* total 60 > 35 */
    assert(rogue_item_instance_validate_budget(inst)<0);
}

static void test_upgrade_within_budget(void){
    unsigned int rng=1234u;
    int inst = spawn_with_affixes("budget_sword","flat_small",NULL);
    RogueItemInstance* it=(RogueItemInstance*)rogue_item_instance_at(inst);
    it->item_level=1; it->rarity=1; /* cap 35 */
    int start_val = it->prefix_value; assert(start_val==3);
    rogue_item_instance_upgrade_level(inst,4,&rng); /* item_level=5 -> cap = 20 + 5*5 + 1*10 = 55 */
    assert(it->item_level==5);
    assert(it->prefix_value > start_val); /* elevated toward new cap */
    assert(rogue_item_instance_validate_budget(inst)==0);
}

int main(void){
    seed_affixes();
    seed_items();
    test_budget_validation();
    test_upgrade_within_budget();
    /* Basic runtime sanity (defensive if NDEBUG stripped asserts) */
    int di=rogue_item_def_index("budget_sword"); if(di<0){ fprintf(stderr,"missing_def\n"); return 1; }
    printf("phase3_budget_ok\n");
    return 0;
}
