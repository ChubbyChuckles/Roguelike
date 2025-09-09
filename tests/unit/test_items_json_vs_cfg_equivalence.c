/* Validate that loading identical items from CFG and JSON yields equivalent RogueItemDef fields. */
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#endif

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

static void assert_equal_int(const char* label, int a, int b, int* failures)
{
    if (a != b)
    {
        fprintf(stderr, "mismatch %s: %d vs %d\n", label, a, b);
        (*failures)++;
    }
}

static void assert_equal_str(const char* label, const char* a, const char* b, int* failures)
{
    if ((a == NULL && b != NULL) || (a != NULL && b == NULL) || (a && b && strcmp(a, b) != 0))
    {
        fprintf(stderr, "mismatch %s: '%s' vs '%s'\n", label, a ? a : "(null)", b ? b : "(null)");
        (*failures)++;
    }
}

int main(void)
{
    const char* id = "equiv_sword";

    /* Build a minimal equivalent item in both formats */
    const char* cfg = "# "
                      "id,name,category,level_req,stack_max,base_value,dmg_min,dmg_max,armor,sheet,"
                      "tx,ty,tw,th,rarity\n"
                      "equiv_sword,Equivalent Sword,2,1,1,25,3,7,0,sheet.png,0,0,1,1,1\n";

    const char* json = "[\n"
                       " {\"id\":\"equiv_sword\",\"name\":\"Equivalent Sword\",\"category\":2,"
                       "\"level_req\":1,\"stack_max\":1,\"base_value\":25,\"base_damage_min\":3,"
                       "\"base_damage_max\":7,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\","
                       "\"sprite_tx\":0,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":"
                       "1,\"flags\":0}\n"
                       "]";

    const char* cfg_path = "tmp_equiv_items.cfg";
    const char* json_path = "tmp_equiv_items.json";
    if (!write_file(cfg_path, cfg) || !write_file(json_path, json))
    {
        fprintf(stderr, "failed to write temp files\n");
        return 1;
    }

    int failures = 0;

    /* Load CFG variant */
    rogue_item_defs_reset();
    int added_cfg = rogue_item_defs_load_from_cfg(cfg_path);
    if (added_cfg <= 0)
    {
        fprintf(stderr, "failed to load cfg (%d)\n", added_cfg);
        remove(cfg_path);
        remove(json_path);
        return 2;
    }
    const RogueItemDef* d_cfg = rogue_item_def_by_id(id);
    if (!d_cfg)
    {
        fprintf(stderr, "missing id after cfg load\n");
        remove(cfg_path);
        remove(json_path);
        return 3;
    }

    /* Load JSON variant */
    rogue_item_defs_reset();
    int added_json = rogue_item_defs_load_from_json(json_path);
    if (added_json <= 0)
    {
        fprintf(stderr, "failed to load json (%d)\n", added_json);
        remove(cfg_path);
        remove(json_path);
        return 4;
    }
    const RogueItemDef* d_json = rogue_item_def_by_id(id);
    if (!d_json)
    {
        fprintf(stderr, "missing id after json load\n");
        remove(cfg_path);
        remove(json_path);
        return 5;
    }

    /* Field-by-field comparison */
    assert_equal_str("id", d_cfg->id, d_json->id, &failures);
    assert_equal_str("name", d_cfg->name, d_json->name, &failures);
    assert_equal_int("category", (int) d_cfg->category, (int) d_json->category, &failures);
    assert_equal_int("level_req", d_cfg->level_req, d_json->level_req, &failures);
    assert_equal_int("stack_max", d_cfg->stack_max, d_json->stack_max, &failures);
    assert_equal_int("base_value", d_cfg->base_value, d_json->base_value, &failures);
    assert_equal_int("base_damage_min", d_cfg->base_damage_min, d_json->base_damage_min, &failures);
    assert_equal_int("base_damage_max", d_cfg->base_damage_max, d_json->base_damage_max, &failures);
    assert_equal_int("base_armor", d_cfg->base_armor, d_json->base_armor, &failures);
    assert_equal_str("sprite_sheet", d_cfg->sprite_sheet, d_json->sprite_sheet, &failures);
    assert_equal_int("sprite_tx", d_cfg->sprite_tx, d_json->sprite_tx, &failures);
    assert_equal_int("sprite_ty", d_cfg->sprite_ty, d_json->sprite_ty, &failures);
    assert_equal_int("sprite_tw", d_cfg->sprite_tw, d_json->sprite_tw, &failures);
    assert_equal_int("sprite_th", d_cfg->sprite_th, d_json->sprite_th, &failures);
    assert_equal_int("rarity", d_cfg->rarity, d_json->rarity, &failures);
    assert_equal_int("flags", d_cfg->flags, d_json->flags, &failures);
    /* Implicits/sockets default to 0 in both paths */
    assert_equal_int("implicit_strength", d_cfg->implicit_strength, d_json->implicit_strength,
                     &failures);
    assert_equal_int("implicit_dexterity", d_cfg->implicit_dexterity, d_json->implicit_dexterity,
                     &failures);
    assert_equal_int("implicit_vitality", d_cfg->implicit_vitality, d_json->implicit_vitality,
                     &failures);
    assert_equal_int("implicit_intelligence", d_cfg->implicit_intelligence,
                     d_json->implicit_intelligence, &failures);
    assert_equal_int("socket_min", d_cfg->socket_min, d_json->socket_min, &failures);
    assert_equal_int("socket_max", d_cfg->socket_max, d_json->socket_max, &failures);

    /* Also verify fast index presence for the id */
    if (rogue_item_def_index_fast(id) < 0)
    {
        fprintf(stderr, "fast index missing for %s in JSON load\n", id);
        failures++;
    }

    /* Cleanup */
    remove(cfg_path);
    remove(json_path);

    if (failures)
    {
        fprintf(stderr, "equivalence test failed with %d mismatches\n", failures);
        return 10;
    }
    printf("items JSON vs CFG equivalence OK\n");
    return 0;
}
