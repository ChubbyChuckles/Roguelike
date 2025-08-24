#include "loot_pickup.h"
#include "../../util/log.h"
#include "../app/app_state.h"
#include "../inventory/inventory.h"
#include "../metrics.h"
#include "loot_adaptive.h"
#include "loot_instances.h"
#include "loot_multiplayer.h"
#include "loot_rarity_adv.h"
#include <math.h>

void rogue_loot_pickup_update(float radius)
{
    if (!g_app.item_instances)
        return;
    float r2 = radius * radius;
    for (int i = 0; i < g_app.item_instance_cap; i++)
        if (g_app.item_instances[i].active)
        {
            RogueItemInstance* it = &g_app.item_instances[i];
            /* Phase 16.1: if item has specific owner and (future) local player id mismatches, skip.
             * Single player local id assumed 0. */
            if (it->owner_player_id >= 0 && it->owner_player_id != 0)
                continue; /* personal ownership gating */
            if (rogue_loot_instance_locked(i))
                continue; /* locked by unresolved need/greed session */
            float dx = it->x - g_app.player.base.pos.x;
            float dy = it->y - g_app.player.base.pos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 <= r2)
            {
                int added = rogue_inventory_add(it->def_index, it->quantity);
                if (added > 0)
                {
                    it->active = 0;
                    /* Record pickup for preference learning + session metrics */
                    rogue_adaptive_record_pickup(it->def_index);
                    rogue_metrics_record_pickup(it->rarity);
                    const char* sfx = rogue_rarity_get_pickup_sound(it->rarity);
                    if (sfx)
                    {
                        ROGUE_LOG_INFO("Pickup def=%d qty=%d sfx=%s", it->def_index, it->quantity,
                                       sfx);
                    }
                    else
                    {
                        ROGUE_LOG_INFO("Pickup def=%d qty=%d", it->def_index, it->quantity);
                    }
                }
            }
        }
}
