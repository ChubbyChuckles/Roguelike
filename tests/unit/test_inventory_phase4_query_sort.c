#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/core/inventory/inventory_entries.h"
#include "../../src/core/inventory/inventory_tags.h"
#include "../../src/core/inventory/inventory_tag_rules.h"
#include "../../src/core/inventory/inventory_query.h"
#include "core/loot/loot_item_defs.h"

/* Minimal harness: we assume some item definitions loaded by game bootstrap in broader test runs.
 * Here we simulate by manually crafting a few pickups with arbitrary def indices.
 */
static void populate(){ rogue_inventory_entries_init(); rogue_inv_tags_init(); rogue_inv_tag_rules_clear();
    /* Acquire some defs with varying counts */
    rogue_inventory_register_pickup(2,10); /* assume rarity maybe 0 */
    rogue_inventory_register_pickup(5,3);
    rogue_inventory_register_pickup(8,7);
}

static void test_basic_query_qty(){ populate(); int ids[16]; int n=rogue_inventory_query_execute("qty>=5", ids, 16); assert(n>0); for(int i=0;i<n;i++){ int q=(int)rogue_inventory_quantity(ids[i]); assert(q>=5); }
}

static void test_sort(){ populate(); int ids[16]; int n=rogue_inventory_query_execute("qty>=0", ids, 16); assert(n>=3); int r=rogue_inventory_query_sort(ids,n,"-qty"); assert(r==0); for(int i=1;i<n;i++){ assert(rogue_inventory_quantity(ids[i-1]) >= rogue_inventory_quantity(ids[i])); }
}

static void test_fuzzy(){ populate(); int ids[16]; int n=rogue_inventory_fuzzy_search("swo", ids, 16); /* may be zero if no matching names, ensure no crash */ assert(n>=0); }

int main(){ populate(); test_basic_query_qty(); test_sort(); test_fuzzy(); printf("inventory_phase4_query_sort: OK\n"); return 0; }
