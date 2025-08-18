#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/inventory_entries.h"
#include "core/inventory_query.h"

static void seed(){ rogue_inventory_entries_init(); for(int i=0;i<10;i++){ /* simulate def ids present with qty */ rogue_inventory_register_pickup(i, (i+1)*2); }
}

int main(){ seed(); int buf[64]; int n1 = rogue_inventory_query_execute_cached("qty>=10", buf, 64); assert(n1>0); /* second call should hit cache and match results */
    int buf2[64]; int n2 = rogue_inventory_query_execute_cached("qty>=10", buf2, 64); assert(n1==n2); for(int i=0;i<n1;i++) assert(buf[i]==buf2[i]);
    /* mutate inventory to invalidate */
    rogue_inventory_register_pickup(1,5); int buf3[64]; int n3 = rogue_inventory_query_execute_cached("qty>=10", buf3, 64); assert(n3>=n2); /* at least same or more entries after mutation */
    printf("inventory_phase4_query_cache: OK\n"); return 0; }
