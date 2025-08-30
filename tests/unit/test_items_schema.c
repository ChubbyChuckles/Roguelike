#include "../../src/content/schema_items.h"
#include "../../src/core/integration/json_schema.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

static void make_basic_weapon(RogueItemDef* d, const char* id, const char* name)
{
    memset(d, 0, sizeof(*d));
#if defined(_MSC_VER)
    strncpy_s(d->id, sizeof d->id, id, _TRUNCATE);
    strncpy_s(d->name, sizeof d->name, name, _TRUNCATE);
#else
    strncpy(d->id, id, sizeof d->id - 1);
    strncpy(d->name, name, sizeof d->name - 1);
#endif
    d->category = ROGUE_ITEM_WEAPON;
    d->level_req = 1;
    d->stack_max = 1;
    d->base_value = 10;
    d->base_damage_min = 2;
    d->base_damage_max = 4;
    d->sprite_tw = 32;
    d->sprite_th = 32;
}

int main(void)
{
    RogueItemDef defs[2];
    make_basic_weapon(&defs[0], "sword_basic", "Basic Sword");
    make_basic_weapon(&defs[1], "axe_basic", "Basic Axe");

    RogueSchemaValidationResult res = {0};
    if (!rogue_items_validate_defs(defs, 2, &res))
    {
        printf("FAIL: expected valid defs, error_count=%u\n", res.error_count);
        return 1;
    }

    /* Negative: invalid category out of range */
    RogueItemDef bad;
    make_basic_weapon(&bad, "bad_cat", "Bad Cat");
    bad.category = (RogueItemCategory) 999;
    RogueSchemaValidationResult res2 = {0};
    if (rogue_items_validate_defs(&bad, 1, &res2))
    {
        printf("FAIL: expected invalid bad.category to be rejected\n");
        return 1;
    }

    printf("OK\n");
    return 0;
}
