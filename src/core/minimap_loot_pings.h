/* Mini-map loot pings (Itemsystem 12.4) */
#ifndef ROGUE_MINIMAP_LOOT_PINGS_H
#define ROGUE_MINIMAP_LOOT_PINGS_H

/* Register a loot ping for an item spawn at world tile coords (x,y) with rarity (0..4). */
int rogue_minimap_ping_loot(float x, float y, int rarity);
/* Update lifetimes & cull expired pings (dt in ms). */
void rogue_minimap_pings_update(float dt_ms);
/* Number of active pings. */
int rogue_minimap_pings_active_count(void);
/* Clear all pings (for tests). */
void rogue_minimap_pings_reset(void);

#endif
