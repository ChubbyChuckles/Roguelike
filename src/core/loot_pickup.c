#include "core/loot_pickup.h"
#include "core/app_state.h"
#include "core/loot_instances.h"
#include "core/inventory.h"
#include "util/log.h"
#include "core/loot_adaptive.h"
#include <math.h>

void rogue_loot_pickup_update(float radius){
    if(!g_app.item_instances) return;
    float r2 = radius * radius;
    for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active){
        RogueItemInstance* it = &g_app.item_instances[i];
        float dx = it->x - g_app.player.base.pos.x;
        float dy = it->y - g_app.player.base.pos.y;
        float d2 = dx*dx + dy*dy;
        if(d2 <= r2){
            int added = rogue_inventory_add(it->def_index, it->quantity);
            if(added>0){
                it->active = 0;
                rogue_adaptive_record_pickup(it->def_index);
                ROGUE_LOG_INFO("Pickup def=%d qty=%d", it->def_index, it->quantity);
            }
        }
    }
}
