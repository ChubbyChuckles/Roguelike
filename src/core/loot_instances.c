#include "core/loot_instances.h"
#include "core/app_state.h"
#include <string.h>
#include "core/loot_logging.h"

static RogueItemInstance g_instances[ROGUE_ITEM_INSTANCE_CAP];

void rogue_items_init_runtime(void){ memset(g_instances,0,sizeof g_instances); g_app.item_instances = g_instances; g_app.item_instance_cap = ROGUE_ITEM_INSTANCE_CAP; g_app.item_instance_count = 0; }
void rogue_items_shutdown_runtime(void){ g_app.item_instances=NULL; g_app.item_instance_cap=0; g_app.item_instance_count=0; }

int rogue_items_spawn(int def_index, int quantity, float x, float y){
    if(def_index<0 || quantity<=0){ ROGUE_LOOT_LOG_DEBUG("loot_spawn: rejected def=%d qty=%d", def_index, quantity); return -1; }
    for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++) if(!g_instances[i].active){
        const RogueItemDef* idef = rogue_item_def_at(def_index);
        int rarity = (idef? idef->rarity : 0);
        g_instances[i].def_index = def_index; g_instances[i].quantity = quantity; g_instances[i].x = x; g_instances[i].y = y; g_instances[i].life_ms=0; g_instances[i].active=1; g_instances[i].rarity=rarity;
        if(i >= g_app.item_instance_count) g_app.item_instance_count = i+1;
    ROGUE_LOOT_LOG_INFO("loot_spawn: def=%d qty=%d at(%.2f,%.2f) slot=%d active_total=%d", def_index, quantity, x, y, i, rogue_items_active_count()+1);
        return i; }
    ROGUE_LOG_WARN("loot_spawn: pool full (cap=%d) def=%d qty=%d", ROGUE_ITEM_INSTANCE_CAP, def_index, quantity);
    return -1;
}

int rogue_items_active_count(void){ int c=0; for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++){ if(g_instances[i].active) c++; } return c; }
void rogue_items_update(float dt_ms){
    /* Advance lifetime & mark for despawn */
    for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++) if(g_instances[i].active){
        g_instances[i].life_ms += dt_ms;
        if(g_instances[i].life_ms >= (float)ROGUE_ITEM_DESPAWN_MS){
            g_instances[i].active=0; continue; }
    }
    /* Stack merge pass (single sweep O(n^2) small cap) */
    for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++) if(g_instances[i].active){
        for(int j=i+1;j<ROGUE_ITEM_INSTANCE_CAP;j++) if(g_instances[j].active){
            if(g_instances[i].def_index == g_instances[j].def_index && g_instances[i].rarity == g_instances[j].rarity){
                float dx = g_instances[i].x - g_instances[j].x; float dy = g_instances[i].y - g_instances[j].y;
                if(dx*dx + dy*dy <= ROGUE_ITEM_STACK_MERGE_RADIUS * ROGUE_ITEM_STACK_MERGE_RADIUS){
                    /* Merge j into i respecting stack_max */
                    const RogueItemDef* d = rogue_item_def_at(g_instances[i].def_index);
                    int stack_max = d? d->stack_max : 999999;
                    int space = stack_max - g_instances[i].quantity;
                    if(space>0){
                        int move = g_instances[j].quantity < space ? g_instances[j].quantity : space;
                        g_instances[i].quantity += move;
                        g_instances[j].quantity -= move;
                        if(g_instances[j].quantity <= 0){ g_instances[j].active = 0; }
                    }
                }
            }
        }
    }
}
