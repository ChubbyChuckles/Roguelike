/* Phase 10.4: Quality metric scaling test */
#define SDL_MAIN_HANDLED 1
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>

int rogue_minimap_ping_loot(float x, float y, int r)
{
    (void) x;
    (void) y;
    (void) r;
    return 0;
}
void rogue_stat_cache_mark_dirty(void) {}

/* Minimal item def table */
static RogueItemDef defs[2];
const RogueItemDef* rogue_item_def_at(int idx)
{
    if (idx >= 0 && idx < 2)
        return &defs[idx];
    return NULL;
}
int rogue_item_def_index(const char* id)
{
    (void) id;
    return -1;
}

int main(void)
{
    defs[0].category = ROGUE_ITEM_WEAPON;
    defs[0].base_damage_min = 10;
    defs[0].base_damage_max = 20;
    defs[0].rarity = 1;
    defs[0].stack_max = 1;
    rogue_items_init_runtime();
    int inst = rogue_items_spawn(0, 1, 0, 0);
    assert(inst >= 0);
    int dmin0 = rogue_item_instance_damage_min(inst);
    int dmax0 = rogue_item_instance_damage_max(inst);
    /* Improve quality */
    assert(rogue_item_instance_set_quality(inst, 10) >= 0); /* +5% expected */
    int dmin5 = rogue_item_instance_damage_min(inst);
    int dmax5 = rogue_item_instance_damage_max(inst);
    assert(dmin5 > dmin0 && dmax5 > dmax0);
    assert(rogue_item_instance_improve_quality(inst, 15) <= 20); /* clamp to 20 (10% max) */
    int dmin10 = rogue_item_instance_damage_min(inst);
    int dmax10 = rogue_item_instance_damage_max(inst);
    assert(dmin10 > dmin5 && dmax10 > dmax5);
    printf("equipment_phase10_quality_ok\n");
    return 0;
}
