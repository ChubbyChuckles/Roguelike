#include "core/inventory.h"
#include <string.h>

static int g_counts[ROGUE_ITEM_DEF_CAP];
static int g_distinct = 0;

void rogue_inventory_init(void){ memset(g_counts,0,sizeof g_counts); g_distinct = 0; }
void rogue_inventory_reset(void){ rogue_inventory_init(); }

int rogue_inventory_add(int def_index, int quantity){
    if(def_index < 0 || def_index >= ROGUE_ITEM_DEF_CAP || quantity <= 0) return 0;
    if(g_counts[def_index] == 0) g_distinct++;
    long long before = g_counts[def_index];
    long long after = before + quantity;
    if(after > 2147483647) after = 2147483647;
    g_counts[def_index] = (int)after;
    return (int)(after - before);
}

int rogue_inventory_get_count(int def_index){ if(def_index<0 || def_index>=ROGUE_ITEM_DEF_CAP) return 0; return g_counts[def_index]; }
int rogue_inventory_total_distinct(void){ return g_distinct; }
