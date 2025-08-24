/* Phase 16.1: External equipment definition editor JSON roundtrip test */
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_temp_json(const char* path)
{
    const char* json =
        "[\n"
        " {\"id\":\"json_sword\",\"name\":\"JSON "
        "Sword\",\"category\":2,\"level_req\":5,\"stack_max\":1,\"base_value\":50,\"base_damage_"
        "min\":4,\"base_damage_max\":9,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_"
        "tx\":0,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":2,\"flags\":0,"
        "\"implicit_strength\":1,\"implicit_dexterity\":0,\"implicit_vitality\":0,\"implicit_"
        "intelligence\":0,\"implicit_armor_flat\":0,\"implicit_resist_physical\":0,\"implicit_"
        "resist_fire\":0,\"implicit_resist_cold\":0,\"implicit_resist_lightning\":0,\"implicit_"
        "resist_poison\":0,\"implicit_resist_status\":0,\"set_id\":0,\"socket_min\":0,\"socket_"
        "max\":0},\n"
        " {\"id\":\"json_helm\",\"name\":\"JSON "
        "Helm\",\"category\":3,\"level_req\":3,\"stack_max\":1,\"base_value\":30,\"base_damage_"
        "min\":0,\"base_damage_max\":0,\"base_armor\":5,\"sprite_sheet\":\"sheet.png\",\"sprite_"
        "tx\":1,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":1,\"flags\":0,"
        "\"implicit_strength\":0,\"implicit_dexterity\":0,\"implicit_vitality\":1,\"implicit_"
        "intelligence\":0,\"implicit_armor_flat\":2,\"implicit_resist_physical\":0,\"implicit_"
        "resist_fire\":1,\"implicit_resist_cold\":0,\"implicit_resist_lightning\":0,\"implicit_"
        "resist_poison\":0,\"implicit_resist_status\":0,\"set_id\":0,\"socket_min\":0,\"socket_"
        "max\":1}\n"
        "]";
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0)
        f = NULL;
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return 0;
    fwrite(json, 1, strlen(json), f);
    fclose(f);
    return 1;
}

int main(void)
{
    rogue_item_defs_reset();
    const char* path = "temp_itemdefs_phase16.json";
    if (!write_temp_json(path))
    {
        fprintf(stderr, "failed to write temp json\n");
        return 1;
    }
    int loaded = rogue_item_defs_load_from_json(path);
    if (loaded != 2)
    {
        fprintf(stderr, "expected 2 loaded got %d\n", loaded);
        return 2;
    }
    if (rogue_item_def_index_fast("json_sword") < 0 || rogue_item_def_index_fast("json_helm") < 0)
    {
        fprintf(stderr, "fast index lookup failed\n");
        return 3;
    }
    char buf[2048];
    int n = rogue_item_defs_export_json(buf, (int) sizeof buf);
    if (n < 0)
    {
        fprintf(stderr, "export failed\n");
        return 4;
    }
    if (strstr(buf, "json_sword") == NULL || strstr(buf, "json_helm") == NULL)
    {
        fprintf(stderr, "export missing ids\n");
        return 5;
    }
    if (strstr(buf, "\"level_req\":5") == NULL)
    {
        fprintf(stderr, "missing level_req 5\n");
        return 6;
    }
    if (strstr(buf, "\"base_armor\":5") == NULL)
    {
        fprintf(stderr, "missing base_armor 5\n");
        return 7;
    }
    remove(path);
    printf("Phase16.1 itemdefs JSON roundtrip OK (%d chars)\n", n);
    return 0;
}
