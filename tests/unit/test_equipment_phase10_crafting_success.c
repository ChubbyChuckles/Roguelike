/* Phase 10.5: Crafting success chance scaling test */
#define SDL_MAIN_HANDLED 1
#include "../../src/core/crafting/crafting.h"
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

static RogueItemDef defs[1];
const RogueItemDef* rogue_item_def_at(int idx)
{
    if (idx == 0)
        return &defs[0];
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
    defs[0].base_damage_min = 2;
    defs[0].base_damage_max = 4;
    defs[0].rarity = 2;
    defs[0].stack_max = 1;
    rogue_items_init_runtime();
    int inst = rogue_items_spawn(0, 1, 0, 0);
    assert(inst >= 0);
    unsigned int rng = 123u;
    /* Low skill should often fail */
    rogue_craft_set_skill(0);
    int fail_count = 0, attempts = 50;
    for (int i = 0; i < attempts; i++)
    {
        if (rogue_craft_success_attempt(2, 3, &rng) == 0)
            fail_count++;
    }
    assert(fail_count > 10); /* expect some failures */
    /* High skill should mostly succeed */
    rogue_craft_set_skill(10);
    int success = 0;
    for (int i = 0; i < attempts; i++)
    {
        if (rogue_craft_success_attempt(2, 3, &rng) == 1)
            success++;
    }
    assert(success > fail_count); /* improvement */
    /* Direct gated upgrade attempt: force many attempts until success observed */
    rogue_craft_set_skill(0);
    int rc;
    int observed_success = 0;
    for (int i = 0; i < 100; i++)
    {
        unsigned int lrng = (unsigned int) i + 77;
        rc = rogue_craft_attempt_upgrade(inst, 1, 5, &lrng);
        if (rc == 0)
        {
            observed_success = 1;
            break;
        }
    }
    assert(observed_success == 1);
    printf("equipment_phase10_crafting_success_ok\n");
    return 0;
}
