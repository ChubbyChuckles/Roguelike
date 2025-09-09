/* Phase 5.2 Integration: inventory operations with JSON-loaded items
   - Write temporary items JSON (stackable consumable)
   - Add/remove/stack, serialize to kv file, reload and verify */
#include "../../src/core/inventory/inventory.h"
#include "../../src/core/loot/loot_item_defs.h"
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
    const char* items_path = "tmp_inv_items.json";
    const char* kv_path = "tmp_inv_kv.txt";
    const char* items_json =
        "[\n"
        " {\"id\":\"ijson_potion\",\"name\":\"JSON Potion\",\"category\":1,"
        "\"level_req\":1,\"stack_max\":10,\"base_value\":5,\"base_damage_min\":0,\"base_"
        "damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":0,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0}\n"
        "]\n";

    if (!write_file(items_path, items_json))
    {
        fprintf(stderr, "INV_JSON_FAIL write items\n");
        return 1;
    }

    rogue_item_defs_reset();
    int add = rogue_item_defs_load_from_json(items_path);
    if (add < 1)
    {
        fprintf(stderr, "INV_JSON_FAIL items=%d\n", add);
        remove(items_path);
        return 2;
    }

    int potion = rogue_item_def_index("ijson_potion");
    if (potion < 0)
    {
        fprintf(stderr, "INV_JSON_FAIL def index\n");
        remove(items_path);
        return 3;
    }

    rogue_inventory_reset();
    int a1 = rogue_inventory_add(potion, 7);
    int have = rogue_inventory_get_count(potion);
    if (a1 < 7 || have != 7)
    {
        fprintf(stderr, "INV_JSON_FAIL add7 a1=%d have=%d\n", a1, have);
        remove(items_path);
        return 4;
    }
    int c3 = rogue_inventory_consume(potion, 3);
    have = rogue_inventory_get_count(potion);
    if (c3 != 3 || have != 4)
    {
        fprintf(stderr, "INV_JSON_FAIL consume3 c3=%d have=%d\n", c3, have);
        remove(items_path);
        return 5;
    }
    int a2 = rogue_inventory_add(potion, 8);
    have = rogue_inventory_get_count(potion);
    if (a2 < 8 || have != 12)
    {
        fprintf(stderr, "INV_JSON_FAIL add8 a2=%d have=%d\n", a2, have);
        remove(items_path);
        return 6;
    }

    /* Serialize to kv file */
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, kv_path, "wb") != 0)
        f = NULL;
#else
    f = fopen(kv_path, "wb");
#endif
    if (!f)
    {
        fprintf(stderr, "INV_JSON_FAIL open kv for write\n");
        remove(items_path);
        return 7;
    }
    rogue_inventory_serialize(f);
    fclose(f);

    int expect = have;
    /* Reset and reload from kv */
    rogue_inventory_reset();
#if defined(_MSC_VER)
    if (fopen_s(&f, kv_path, "rb") != 0)
        f = NULL;
#else
    f = fopen(kv_path, "rb");
#endif
    if (!f)
    {
        fprintf(stderr, "INV_JSON_FAIL open kv for read\n");
        remove(items_path);
        remove(kv_path);
        return 8;
    }
    char line[256];
    while (fgets(line, sizeof line, f))
    {
        char* nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';
        char* eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;
        (void) rogue_inventory_try_parse_kv(key, val);
    }
    fclose(f);

    have = rogue_inventory_get_count(potion);
    if (have != expect)
    {
        fprintf(stderr, "INV_JSON_FAIL reload have=%d expect=%d\n", have, expect);
        remove(items_path);
        remove(kv_path);
        return 9;
    }

    remove(items_path);
    remove(kv_path);
    printf("INV_JSON_OK qty=%d\n", have);
    return 0;
}
