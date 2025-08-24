#define SDL_MAIN_HANDLED 1
#include "../../src/core/app/app_state.h"
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/game/durability.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Minimal stubs: Only override item defs; reuse real budget + affix functions from core. */
void rogue_stat_cache_mark_dirty(void) {}
const RogueItemDef* rogue_item_def_at(int index)
{
    static RogueItemDef defs[2];
    defs[0].category = ROGUE_ITEM_WEAPON;
    defs[0].rarity = 0;
    defs[0].base_value = 10;
    defs[1].category = ROGUE_ITEM_WEAPON;
    defs[1].rarity = 4;
    defs[1].base_value = 10;
    if (index >= 0 && index < 2)
        return &defs[index];
    return NULL;
}

/* Minimal global app state simulation */
/* Stub external dependency from loot_instances.c */
int rogue_minimap_ping_loot(float x, float y, int rarity)
{
    (void) x;
    (void) y;
    (void) rarity;
    return 0;
}

int main(void)
{
    printf("durability_model_test_begin\n");
    rogue_items_init_runtime();
    printf("runtime_init_ok\n");
    int common = rogue_items_spawn(0, 1, 0, 0);
    printf("after spawn common=%d\n", common);
    fflush(stdout);
    assert(common >= 0);
    int loss = rogue_item_instance_apply_durability_event(common, 25);
    printf("single loss=%d\n", loss);
    fflush(stdout);
    assert(loss >= 1);
    printf("equipment_phase8_durability_model_ok\n");
    fflush(stdout);
    return 0;
}
