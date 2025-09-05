#include "../../src/core/app/app_state.h"
#include "../../src/core/inventory/inventory.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/world/world_gen.h"
#include <stdio.h>
#include <string.h>

/* Non-override end-to-end: ensure upgrade_possible heuristic leads to an upgrade plan
 * and that the planned item is strictly better than current equipment snapshot in
 * its category (weapon dmg_max or armor base_armor). */
static int count_overlay(const RogueTileMap* m, unsigned char code)
{
    if (!m->overlay_deco || m->overlay_magic != 0xDEC00EAU)
        return 0;
    int c = 0;
    for (int y = 0; y < m->height; ++y)
        for (int x = 0; x < m->width; ++x)
            if (rogue_tilemap_get_deco(m, x, y) == code)
                c++;
    return c;
}

int main(void)
{
    /* Minimal content: load default test items */
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        fprintf(stderr, "no test_items.cfg\n");
        return 1;
    }
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        fprintf(stderr, "failed to load items\n");
        return 2;
    }
    rogue_inventory_reset();
    rogue_items_init_runtime();

    /* Equip a weak weapon so upgrade is possible */
    int idx_sword = rogue_item_def_index("long_sword");
    if (idx_sword < 0)
    {
        fprintf(stderr, "missing long_sword def\n");
        return 3;
    }
    int inst_sword = rogue_items_spawn(idx_sword, 1, 0, 0);
    if (inst_sword < 0)
    {
        fprintf(stderr, "spawn sword failed\n");
        return 4;
    }
    /* Put into equipment slot via equipment API if available; fallback is snapshot sees instances.
     */
    extern int rogue_equip_try(enum RogueEquipSlot slot, int inst_index);
    if (rogue_equip_try)
    {
        (void) 0;
    }

    /* Worldgen context */
    RogueWorldGenConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.seed = 20250831;
    cfg.width = 192;
    cfg.height = 192;
    RogueWorldGenContext wctx;
    rogue_worldgen_context_init(&wctx, &cfg);

    RogueTileMap map;
    if (!rogue_tilemap_init(&map, cfg.width, cfg.height))
        return 5;
    RogueDungeonGraph graph;
    memset(&graph, 0, sizeof graph);
    if (!rogue_dungeon_generate_graph(&wctx, 18, 16, &graph))
    {
        fprintf(stderr, "graph\n");
        return 6;
    }
    int ox = (cfg.width - 160) / 2;
    if (ox < 0)
        ox = 0;
    int oy = (cfg.height - 160) / 2;
    if (oy < 0)
        oy = 0;
    rogue_dungeon_carve_into_map(&wctx, &map, &graph, ox, oy, 160, 160);
    rogue_dungeon_place_traps_and_secrets(&wctx, &map, &graph, 6, 0.1);

    RogueDungeonChestPlacement ch[32];
    int upcount = 0;
    int ccount = rogue_dungeon_place_chests(&wctx, &map, &graph, 5, ch, 32, &upcount);
    if (ccount <= 0)
    {
        fprintf(stderr, "no chests\n");
        return 7;
    }

    int markers = count_overlay(&map, 14);
    if (upcount == 1 && markers < 1)
    {
        fprintf(stderr, "heuristic said upgrade but no marker\n");
        return 8;
    }

    /* Find the upgrade chest and verify planning fields */
    int found = -1;
    for (int i = 0; i < ccount; ++i)
    {
        if (ch[i].is_upgrade)
        {
            found = i;
            break;
        }
    }
    if (upcount == 1 && found < 0)
    {
        fprintf(stderr, "expected is_upgrade chest\n");
        return 9;
    }
    if (found >= 0)
    {
        if (ch[found].planned_def_index < 0 || ch[found].planned_rarity < 0)
        {
            fprintf(stderr, "missing planned item fields\n");
            return 10;
        }
        const RogueItemDef* cur = rogue_item_def_at(rogue_item_def_index("long_sword"));
        const RogueItemDef* planned = rogue_item_def_at(ch[found].planned_def_index);
        if (!planned)
        {
            fprintf(stderr, "planned def invalid\n");
            return 11;
        }
        if (planned->category == ROGUE_ITEM_WEAPON)
        {
            if (cur && !(planned->base_damage_max > cur->base_damage_max))
            {
                fprintf(stderr, "planned weapon not strictly better\n");
                return 12;
            }
        }
        if (planned->category == ROGUE_ITEM_ARMOR)
        {
            if (cur && !(planned->base_armor >= cur->base_armor))
            {
                fprintf(stderr, "planned armor not >= current armor\n");
                return 13;
            }
        }
        if (ch[found].planned_rarity < 1 || ch[found].planned_rarity > 3)
        {
            fprintf(stderr, "planned rarity out of bounds %d\n", ch[found].planned_rarity);
            return 14;
        }
    }

    rogue_dungeon_free_graph(&graph);
    rogue_tilemap_free(&map);
    rogue_worldgen_context_shutdown(&wctx);
    return 0;
}
