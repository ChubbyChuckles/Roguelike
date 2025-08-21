#include <assert.h>
#include <stdio.h>
#include "core/inventory/inventory_entries.h"
#include "core/inventory_tags.h"
#include "../../src/core/inventory/inventory_query.h"

static void setup(){ rogue_inventory_entries_init(); rogue_inv_tags_init(); }

static void test_store_retrieve(){ setup(); assert(rogue_inventory_saved_search_store("HiTier","rarity>=3","-rarity,qty") == 0); char q[128]; char s[64]; assert(rogue_inventory_saved_search_get("HiTier", q, sizeof q, s, sizeof s)==0); assert(q[0]); assert(s[0]); }

int main(){ test_store_retrieve(); printf("inventory_phase4_saved_searches: OK\n"); return 0; }
