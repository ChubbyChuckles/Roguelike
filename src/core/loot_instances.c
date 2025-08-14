#include "core/loot_instances.h"
#include "core/app_state.h"
#include <string.h>

static RogueItemInstance g_instances[ROGUE_ITEM_INSTANCE_CAP];

void rogue_items_init_runtime(void){ memset(g_instances,0,sizeof g_instances); g_app.item_instances = g_instances; g_app.item_instance_cap = ROGUE_ITEM_INSTANCE_CAP; g_app.item_instance_count = 0; }
void rogue_items_shutdown_runtime(void){ g_app.item_instances=NULL; g_app.item_instance_cap=0; g_app.item_instance_count=0; }

int rogue_items_spawn(int def_index, int quantity, float x, float y){
    if(def_index<0 || quantity<=0) return -1;
    for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++) if(!g_instances[i].active){
        g_instances[i].def_index = def_index; g_instances[i].quantity = quantity; g_instances[i].x = x; g_instances[i].y = y; g_instances[i].life_ms=0; g_instances[i].active=1;
        if(i >= g_app.item_instance_count) g_app.item_instance_count = i+1; return i; }
    return -1;
}

int rogue_items_active_count(void){ int c=0; for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++){ if(g_instances[i].active) c++; } return c; }

void rogue_items_update(float dt_ms){ (void)dt_ms; /* future: despawn logic */ }
