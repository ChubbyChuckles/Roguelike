#include <assert.h>
#include <stdio.h>
#include "core/inventory/inventory_entries.h"
#include "core/inventory_tags.h"
#include "../../src/core/inventory/inventory_query.h"

static void setup(){ rogue_inventory_entries_init(); rogue_inv_tags_init(); }

static void test_quick_actions_basic(){ setup();
    /* seed a couple of saved searches */
    assert(rogue_inventory_saved_search_store("HiTier","rarity>=2","-rarity,qty") == 0);
    assert(rogue_inventory_saved_search_store("All","qty>=0",NULL) == 0);
    int count = rogue_inventory_quick_actions_count();
    assert(count>=2);
    char name[32]; int found=0; for(int i=0;i<count;i++){ if(rogue_inventory_quick_action_name(i,name,sizeof name)==0){ if(strcmp(name,"HiTier")==0) found=1; }}
    assert(found);
    int ids[32]; int n = 0;
    /* find index of HiTier and apply */
    for(int i=0;i<count;i++){ if(rogue_inventory_quick_action_name(i,name,sizeof name)==0 && strcmp(name,"HiTier")==0){ n = rogue_inventory_quick_action_apply(i,ids,32); break; }}
    assert(n>=0); /* may be zero if rarity filter excludes all current defs */
}

int main(){ test_quick_actions_basic(); printf("inventory_phase4_quick_actions: OK\n"); return 0; }
