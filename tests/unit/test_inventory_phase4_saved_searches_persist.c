/* Phase 4.4 persistence test: ensure saved searches survive save/load component cycle.
 * We simulate by writing component payload to a temp file then reading back.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/inventory/inventory_entries.h"
#include "core/inventory_tags.h"
#include "src/core/inventory/inventory_query.h"

static void setup(){ rogue_inventory_entries_init(); rogue_inv_tags_init(); }

int main(){
    setup();
    assert(rogue_inventory_saved_search_store("HiTier","rarity>=3","-rarity,qty") == 0);
    FILE* f = fopen("saved_searches_test.bin","wb");
    assert(f);
    assert(rogue_inventory_saved_searches_write(f)==0);
    fclose(f);
    /* simulate fresh run by reading back into current structures (reader overwrites) */
    FILE* f2=fopen("saved_searches_test.bin","rb");
    assert(f2);
    assert(rogue_inventory_saved_searches_read(f2, 1024)==0);
    fclose(f2);
    char q[64]; char s[32];
    assert(rogue_inventory_saved_search_get("HiTier",q,sizeof q,s,sizeof s)==0);
    assert(strcmp(q,"rarity>=3")==0);
    remove("saved_searches_test.bin");
    printf("inventory_phase4_saved_searches_persist: OK\n");
    return 0; }
