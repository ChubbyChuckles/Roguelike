#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../../src/core/inventory/inventory_entries.h"

static void test_basic_addition(){
    rogue_inventory_entries_init();
    assert(rogue_inventory_unique_count()==0);
    assert(rogue_inventory_register_pickup(10,5)==0);
    assert(rogue_inventory_quantity(10)==5);
    assert(rogue_inventory_unique_count()==1);
    assert(rogue_inventory_register_pickup(10,7)==0);
    assert(rogue_inventory_quantity(10)==12);
    assert(rogue_inventory_unique_count()==1);
}

static void test_unique_cap(){
    rogue_inventory_entries_init();
    rogue_inventory_set_unique_cap(3);
    assert(rogue_inventory_register_pickup(1,1)==0);
    assert(rogue_inventory_register_pickup(2,1)==0);
    assert(rogue_inventory_register_pickup(3,1)==0);
    int rc = rogue_inventory_register_pickup(4,1);
    assert(rc==ROGUE_INV_ERR_UNIQUE_CAP);
    assert(rogue_inventory_unique_count()==3);
}

static void test_pressure(){
    rogue_inventory_entries_init();
    rogue_inventory_set_unique_cap(4);
    assert(rogue_inventory_entry_pressure()==0.0);
    rogue_inventory_register_pickup(1,1);
    double p = rogue_inventory_entry_pressure();
    assert(p > 0.0 && p < 0.4);
    rogue_inventory_register_pickup(2,1);
    rogue_inventory_register_pickup(3,1);
    p = rogue_inventory_entry_pressure();
    assert(p > 0.70 && p < 0.80); /* 3/4 == 0.75 */
}

static void test_overflow_guard(){
    rogue_inventory_entries_init();
    rogue_inventory_set_unique_cap(10);
    assert(rogue_inventory_register_pickup(5, UINT64_C(10))==0);
    int rc = rogue_inventory_register_pickup(5, UINT64_MAX);
    assert(rc == ROGUE_INV_ERR_OVERFLOW);
}

int main(){
    test_basic_addition();
    test_unique_cap();
    test_pressure();
    test_overflow_guard();
    printf("inventory_phase1_unique_entries: OK\n");
    return 0;
}
