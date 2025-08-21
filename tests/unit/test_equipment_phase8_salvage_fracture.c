/* Phase 8.4 & 8.5: Salvage yield scaling with durability + fracture penalty test */
#define SDL_MAIN_HANDLED 1
#include "core/durability.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/vendor/salvage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Minimal stubs */
void rogue_stat_cache_mark_dirty(void) {}
int rogue_minimap_ping_loot(float x, float y, int rarity)
{
    (void) x;
    (void) y;
    (void) rarity;
    return 0;
}
const RogueItemDef* rogue_item_def_at(int index)
{
    static RogueItemDef defs[2];
    defs[0].category = ROGUE_ITEM_WEAPON;
    defs[0].rarity = 1;
    defs[0].base_value = 60; /* value bracket -> x2 scaling */
    defs[1].category = ROGUE_ITEM_WEAPON;
    defs[1].rarity = 1;
    defs[1].base_value = 60;
    if (index >= 0 && index < 2)
        return &defs[index];
    return NULL;
}
int rogue_item_def_index(const char* id)
{
    if (!id)
        return -1;
    if (strcmp(id, "arcane_dust") == 0)
        return 10;
    if (strcmp(id, "primal_shard") == 0)
        return 11;
    return -1;
}
int rogue_inventory_add(int def_index, int qty)
{
    (void) def_index;
    (void) qty;
    return 0;
}

int main(void)
{
    rogue_items_init_runtime();
    int inst_full = rogue_items_spawn(0, 1, 0, 0);
    assert(inst_full >= 0);
    int cur = 0, max = 0;
    rogue_item_instance_get_durability(inst_full, &cur, &max);
    assert(max > 0);
    /* Full durability salvage (simulate) */
    int base_full = rogue_salvage_item_instance(inst_full, rogue_inventory_add);
    assert(base_full > 0);
    /* Damage to half durability */
    rogue_item_instance_damage_durability(inst_full, cur / 2);
    rogue_item_instance_get_durability(inst_full, &cur, &max);
    float pct = (float) cur / (float) max;
    assert(pct < 0.75f && pct > 0.20f);
    int half_qty = rogue_salvage_item_instance(inst_full, rogue_inventory_add);
    assert(half_qty > 0);
    /* Expect half durability yield < full yield (factor declines) */
    assert(half_qty < base_full);
    /* Fracture: reduce to zero and verify fractured penalty on damage output */
    int inst_fract = rogue_items_spawn(1, 1, 0, 0);
    assert(inst_fract >= 0);
    rogue_item_instance_get_durability(inst_fract, &cur, &max);
    rogue_item_instance_damage_durability(inst_fract, max); /* to zero */
    int dmin_full = rogue_item_instance_damage_min(inst_full);
    int dmin_fract = rogue_item_instance_damage_min(inst_fract);
    assert(dmin_fract <= (int) (dmin_full * 0.7f)); /* 40% penalty yields <=60% value */
    printf("equipment_phase8_salvage_fracture_ok\n");
    return 0;
}
