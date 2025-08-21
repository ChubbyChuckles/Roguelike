#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/inventory/inventory_entries.h"
#include "../../src/core/inventory/inventory_query.h"

static void seed(){ rogue_inventory_entries_init(); /* use def indices known to exist in other tests */
    rogue_inventory_register_pickup(2,12); /* >=4 */
    rogue_inventory_register_pickup(5,8);  /* >=4 */
    rogue_inventory_register_pickup(8,2);  /* below threshold */
}

int main(){ seed(); unsigned hits0=0,miss0=0; rogue_inventory_query_cache_stats(&hits0,&miss0);
    int buf[64]; int n1 = rogue_inventory_query_execute_cached("qty>=4", buf, 64); assert(n1==2); /* defs 2 & 5 */
    unsigned hits1=0,miss1=0; rogue_inventory_query_cache_stats(&hits1,&miss1); assert(miss1==miss0+1); /* first call miss */
    int buf2[64]; int n2 = rogue_inventory_query_execute_cached("qty>=4", buf2, 64); assert(n1==n2); for(int i=0;i<n1;i++) assert(buf[i]==buf2[i]);
    unsigned hits2=0,miss2=0; rogue_inventory_query_cache_stats(&hits2,&miss2); assert(hits2==hits1+1); /* second call hit */
    /* mutate inventory to invalidate (increase def 1 across threshold maybe) */
    rogue_inventory_register_pickup(8,5); int buf3[64]; int n3 = rogue_inventory_query_execute_cached("qty>=4", buf3, 64); assert(n3==3); /* now defs 2,5,8 */
    printf("inventory_phase4_query_cache: OK\n"); return 0; }
