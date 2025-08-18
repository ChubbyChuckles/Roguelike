#include <assert.h>
#include <stdio.h>
#include "core/save_manager.h"
#include "core/inventory_entries.h"

/* Test Phase 1.6 persistence integration of inventory entries component. */
static void test_roundtrip(){
    rogue_inventory_entries_init();
    rogue_register_core_save_components();
    rogue_inventory_register_pickup(42, 17);
    rogue_inventory_entry_set_labels(42, ROGUE_INV_LABEL_MATERIAL);
    rogue_inventory_register_pickup(7, 3);
    assert(rogue_inventory_quantity(42)==17);
    assert(rogue_save_manager_save_slot(0)==0);
    /* Wipe and reload */
    rogue_inventory_entries_init();
    assert(rogue_inventory_quantity(42)==0);
    assert(rogue_save_manager_load_slot(0)==0);
    assert(rogue_inventory_quantity(42)==17);
    unsigned lbl = rogue_inventory_entry_labels(42);
    assert((lbl & ROGUE_INV_LABEL_MATERIAL)!=0);
    assert(rogue_inventory_quantity(7)==3);
}
int main(){ test_roundtrip(); printf("inventory_phase1_persistence: OK\n"); return 0; }
