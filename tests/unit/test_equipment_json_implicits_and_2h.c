#include "../../src/core/app/app_state.h"
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/equipment/equipment_stats.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/entities/player.h"
#include "../../src/game/stat_cache.h"
#include <stdio.h>
#include <string.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static int write_file(const char* path, const char* contents)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0)
        f = NULL;
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return 0;
    fwrite(contents, 1, strlen(contents), f);
    fclose(f);
    return 1;
}

int main(void)
{
    /* Prepare two JSON items: a 2H weapon and an armor with implicit stats */
    const char* items_path = "tmp_equipment_items.json";
    const char* items_json =
        "[\n"
        " {\"id\":\"json_2h_greatsword\",\"name\":\"JSON Greatsword\",\"category\":2,"
        "\"level_req\":1,\"stack_max\":1,\"base_value\":50,\"base_damage_min\":5,\"base_"
        "damage_max\":10,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":0,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":1,\"flags\":1},\n"
        " {\"id\":\"json_helm_implicit\",\"name\":\"JSON Implicit Helm\",\"category\":3,"
        "\"level_req\":1,\"stack_max\":1,\"base_value\":10,\"base_damage_min\":0,\"base_"
        "damage_max\":0,\"base_armor\":2,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":1,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":1,\"flags\":0,"
        "\"implicit_strength\":3,\"implicit_resist_fire\":5}\n"
        "]\n";
    if (!write_file(items_path, items_json))
    {
        fprintf(stderr, "EQUIP_JSON_FAIL write items\n");
        return 1;
    }

    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_json(items_path);
    if (added < 2)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL items=%d\n", added);
        remove(items_path);
        return 2;
    }
    rogue_items_init_runtime();
    rogue_equip_reset();

    int widx = rogue_item_def_index("json_2h_greatsword");
    int hidx = rogue_item_def_index("json_helm_implicit");
    if (widx < 0 || hidx < 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL indices\n");
        remove(items_path);
        return 3;
    }
    int winst = rogue_items_spawn(widx, 1, 0, 0);
    int hinst = rogue_items_spawn(hidx, 1, 0, 0);
    if (winst < 0 || hinst < 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL spawn\n");
        remove(items_path);
        return 4;
    }

    /* Equip helm and verify implicit aggregation */
    RoguePlayer p = {0};
    p.max_health = 100;
    if (rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, hinst) != 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL equip helm\n");
        remove(items_path);
        return 5;
    }
    rogue_equipment_apply_stat_bonuses(&p);
    rogue_stat_cache_force_update(&p);
    if (g_player_stat_cache.implicit_strength < 3 || g_player_stat_cache.resist_fire < 5)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL implicit agg\n");
        remove(items_path);
        return 6;
    }

    /* Equip 2H weapon and ensure OFFHAND equip is blocked */
    if (rogue_equip_try(ROGUE_EQUIP_WEAPON, winst) != 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL equip weapon\n");
        remove(items_path);
        return 7;
    }
    /* Create a dummy offhand item (reuse helm index to simulate offhand category permissively) */
    int ohinst = rogue_items_spawn(hidx, 1, 0, 0);
    if (ohinst < 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL spawn offhand\n");
        remove(items_path);
        return 8;
    }
    int r = rogue_equip_try(ROGUE_EQUIP_OFFHAND, ohinst);
    if (r == 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL offhand allowed with 2H\n");
        remove(items_path);
        return 9;
    }

    printf("EQUIP_JSON_OK implicit_str=%d resist_fire=%d offhand_blocked=%d\n",
           g_player_stat_cache.implicit_strength, g_player_stat_cache.resist_fire, r);
    remove(items_path);
    return 0;
}
