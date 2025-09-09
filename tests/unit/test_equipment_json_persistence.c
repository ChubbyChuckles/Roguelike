/* Phase 5.2 Integration: equipment persistence with JSON-loaded items
   - Write temporary items JSON (weapon)
   - Spawn instance, equip, save slot, reset, load slot, verify equipped instance restored */
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/persistence/save_internal.h"
#include "../../src/core/persistence/save_manager.h"
#include <stdio.h>
#include <string.h>

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
    const char* items_path = "tmp_equip_items.json";
    const char* items_json =
        "[\n"
        " {\"id\":\"ejson_sword\",\"name\":\"Equip JSON Sword\",\"category\":2,"
        "\"level_req\":1,\"stack_max\":1,\"base_value\":25,\"base_damage_min\":2,\"base_"
        "damage_max\":5,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":0,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":1,\"flags\":0}\n"
        "]\n";
    if (!write_file(items_path, items_json))
    {
        fprintf(stderr, "EQUIP_JSON_FAIL write items\n");
        return 1;
    }

    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_json(items_path);
    if (added < 1)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL items=%d\n", added);
        remove(items_path);
        return 2;
    }

    /* Prepare runtime */
    rogue_items_init_runtime();
    rogue_equip_reset();

    int def = rogue_item_def_index("ejson_sword");
    if (def < 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL def index\n");
        remove(items_path);
        return 3;
    }
    int inst = rogue_items_spawn(def, 1, 0.0f, 0.0f);
    if (inst < 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL spawn\n");
        remove(items_path);
        return 4;
    }
    if (rogue_equip_try(ROGUE_EQUIP_WEAPON, inst) != 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL equip\n");
        remove(items_path);
        return 5;
    }

    /* Configure save paths to test sandbox and register core components */
    rogue_save_manager_init();
    rogue_register_core_save_components();
    rogue_save_paths_set_prefix_tests();

    if (rogue_save_manager_save_slot(0) != 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL save\n");
        remove(items_path);
        return 6;
    }

    /* Reset runtime state */
    rogue_equip_reset();
    rogue_items_shutdown_runtime();

    if (rogue_save_manager_load_slot(0) != 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL load\n");
        remove(items_path);
        return 7;
    }

    int got = rogue_equip_get(ROGUE_EQUIP_WEAPON);
    if (got < 0)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL no weapon equipped after load\n");
        remove(items_path);
        return 8;
    }
    const RogueItemInstance* it = rogue_item_instance_at(got);
    if (!it || it->def_index != def)
    {
        fprintf(stderr, "EQUIP_JSON_FAIL equip mismatch def=%d got_def=%d\n", def,
                it ? it->def_index : -1);
        remove(items_path);
        return 9;
    }

    /* Cleanup */
    (void) rogue_save_manager_delete_slot(0);
    remove(items_path);
    printf("EQUIP_JSON_OK inst=%d def=%d\n", got, def);
    return 0;
}
