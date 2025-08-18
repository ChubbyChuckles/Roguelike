#include <assert.h>
#include <stdio.h>
#include "core/inventory_tags.h"
#include "core/save_manager.h"
#include "core/inventory_entries.h"

static void test_basic_flags_tags(){
    rogue_inv_tags_init();
    assert(rogue_inv_tags_set_flags(5, ROGUE_INV_FLAG_FAVORITE|ROGUE_INV_FLAG_LOCKED)==0);
    unsigned f=rogue_inv_tags_get_flags(5); assert((f & ROGUE_INV_FLAG_FAVORITE)!=0 && (f & ROGUE_INV_FLAG_LOCKED)!=0);
    assert(rogue_inv_tags_add_tag(5, "mat") == 0);
    assert(rogue_inv_tags_add_tag(5, "rare") == 0);
    assert(rogue_inv_tags_has(5, "mat"));
    const char* list[4]; int cnt=rogue_inv_tags_list(5,list,4); assert(cnt==2);
    assert(!rogue_inv_tags_can_salvage(5));
}

static void test_persistence(){
    rogue_inv_tags_init(); rogue_inventory_entries_init(); rogue_register_core_save_components();
    rogue_inv_tags_set_flags(10, ROGUE_INV_FLAG_FAVORITE); rogue_inv_tags_add_tag(10, "fav");
    rogue_inventory_register_pickup(10,2);
    assert(rogue_save_manager_save_slot(0)==0);
    rogue_inv_tags_init(); rogue_inventory_entries_init();
    assert(rogue_save_manager_load_slot(0)==0);
    unsigned f=rogue_inv_tags_get_flags(10); assert(f & ROGUE_INV_FLAG_FAVORITE);
    assert(rogue_inv_tags_has(10,"fav"));
}

int main(){ test_basic_flags_tags(); test_persistence(); printf("inventory_phase3_tags: OK\n"); return 0; }
