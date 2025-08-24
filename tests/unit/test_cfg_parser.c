#include "../../src/util/cfg_parser.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test helper functions */
static void print_test_header(const char* test_name) { printf("Running test: %s...\n", test_name); }

static void print_test_result(const char* test_name, bool passed)
{
    printf("  %s\n", passed ? "PASS" : "FAIL");
    if (!passed)
    {
        exit(1); /* Fail fast for debugging */
    }
}

/* Create temporary test files for testing */
static void create_test_affix_file(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file)
        return;

    fprintf(file, "# type,id,stat,min,max,w_common,w_uncommon,w_rare,w_epic,w_legendary\n");
    fprintf(file, "PREFIX,sharp,damage_flat,1,3,50,30,15,4,1\n");
    fprintf(file, "SUFFIX,of_the_fox,agility_flat,1,2,40,35,15,7,3\n");
    fprintf(file, "PREFIX,heavy,damage_flat,2,5,25,25,20,10,4\n");
    fprintf(file, "SUFFIX,of_swiftness,agility_flat,2,4,10,15,20,15,6\n");

    fclose(file);
}

static void create_test_item_file(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file)
        return;

    fprintf(file, "# "
                  "id,name,category,level_req,stack_max,base_value,dmg_min,dmg_max,armor,sheet,tx,"
                  "ty,tw,th,rarity\n");
    fprintf(file, "# categories: 0=misc 1=consumable 2=weapon 3=armor 4=gem 5=material\n");
    fprintf(
        file,
        "gold_coin,Gold Coin,0,0,100,1,0,0,0,../assets/biomes/default/Props/Static/Resources.png, "
        "6, 1, 1, 1,0\n");
    fprintf(file, "bandage,Bandage,1,0,10,5,0,0,0,../assets/biomes/default/Props/Static/"
                  "Resources.png, 5, 1, 1, 1,0\n");
    fprintf(
        file,
        "long_sword,Long Sword,2,1,1,25,3,7,0,../assets/biomes/default/Props/Static/Resources.png, "
        "4, 1, 1, 1,1\n");

    fclose(file);
}

static void create_test_keyvalue_file(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file)
        return;

    fprintf(file, "# Configuration file\n");
    fprintf(file, "max_health=100\n");
    fprintf(file, "player_speed=5.0\n");
    fprintf(file, "enable_sound=true\n");
    fprintf(file, "game_title=My Roguelike\n");

    fclose(file);
}

/* ===== Test Functions ===== */

static void test_file_classification(void)
{
    print_test_header("File Classification");

    /* Test item classification */
    assert(rogue_cfg_classify_file("assets/items.cfg") == ROGUE_CFG_CATEGORY_ITEMS);
    assert(rogue_cfg_classify_file("equipment_test.cfg") == ROGUE_CFG_CATEGORY_ITEMS);
    assert(rogue_cfg_classify_file("weapons.cfg") == ROGUE_CFG_CATEGORY_ITEMS);

    /* Test affix classification */
    assert(rogue_cfg_classify_file("affixes.cfg") == ROGUE_CFG_CATEGORY_AFFIXES);
    assert(rogue_cfg_classify_file("modifiers.cfg") == ROGUE_CFG_CATEGORY_AFFIXES);

    /* Test loot table classification */
    assert(rogue_cfg_classify_file("loot_tables.cfg") == ROGUE_CFG_CATEGORY_LOOT_TABLES);
    assert(rogue_cfg_classify_file("test_loot.cfg") == ROGUE_CFG_CATEGORY_LOOT_TABLES);

    /* Test tile classification */
    assert(rogue_cfg_classify_file("tiles.cfg") == ROGUE_CFG_CATEGORY_TILES);
    assert(rogue_cfg_classify_file("tileset_config.cfg") == ROGUE_CFG_CATEGORY_TILES);

    /* Test sound classification */
    assert(rogue_cfg_classify_file("sounds.cfg") == ROGUE_CFG_CATEGORY_SOUNDS);
    assert(rogue_cfg_classify_file("audio_config.cfg") == ROGUE_CFG_CATEGORY_SOUNDS);

    /* Test dialogue classification */
    assert(rogue_cfg_classify_file("dialogue.cfg") == ROGUE_CFG_CATEGORY_DIALOGUE);
    assert(rogue_cfg_classify_file("avatars.cfg") == ROGUE_CFG_CATEGORY_DIALOGUE);

    /* Test skills classification */
    assert(rogue_cfg_classify_file("skills.cfg") == ROGUE_CFG_CATEGORY_SKILLS);
    assert(rogue_cfg_classify_file("abilities.cfg") == ROGUE_CFG_CATEGORY_SKILLS);

    /* Test player classification */
    assert(rogue_cfg_classify_file("player_stats.cfg") == ROGUE_CFG_CATEGORY_PLAYER);
    assert(rogue_cfg_classify_file("player_config.cfg") == ROGUE_CFG_CATEGORY_PLAYER);

    /* Test miscellaneous */
    assert(rogue_cfg_classify_file("unknown_file.cfg") == ROGUE_CFG_CATEGORY_MISC);
    assert(rogue_cfg_classify_file("config.cfg") == ROGUE_CFG_CATEGORY_MISC);

    print_test_result("File Classification", true);
}

static void test_format_detection(void)
{
    print_test_header("Format Detection");

    /* Create test files */
    create_test_affix_file("test_affixes.cfg");
    create_test_keyvalue_file("test_keyvalue.cfg");

    /* Test CSV format detection */
    RogueCfgFormat format = rogue_cfg_detect_format("test_affixes.cfg");
    assert(format == ROGUE_CFG_FORMAT_CSV);

    /* Test key-value format detection */
    format = rogue_cfg_detect_format("test_keyvalue.cfg");
    assert(format == ROGUE_CFG_FORMAT_KEY_VALUE);

    /* Clean up */
    remove("test_affixes.cfg");
    remove("test_keyvalue.cfg");

    print_test_result("Format Detection", true);
}

static void test_file_analysis(void)
{
    print_test_header("File Analysis");

    /* Create test file */
    create_test_affix_file("test_analysis.cfg");

    /* Analyze the file */
    RogueCfgFileAnalysis* analysis = rogue_cfg_analyze_file("test_analysis.cfg");
    assert(analysis != NULL);
    assert(analysis->category == ROGUE_CFG_CATEGORY_AFFIXES);
    assert(analysis->format == ROGUE_CFG_FORMAT_CSV);
    assert(analysis->has_header_comment == true);
    assert(analysis->data_lines == 4);
    assert(analysis->comment_lines == 1);
    assert(analysis->field_count > 0); /* Should have detected fields from first data line */

    /* Check that we detected some reasonable field types */
    bool found_id_field = false;
    bool found_integer_field = false;
    for (int i = 0; i < analysis->field_count; i++)
    {
        if (analysis->fields[i].type == ROGUE_CFG_DATA_TYPE_ID)
        {
            found_id_field = true;
        }
        if (analysis->fields[i].type == ROGUE_CFG_DATA_TYPE_INTEGER)
        {
            found_integer_field = true;
        }
    }

    assert(found_id_field == true);      /* Should detect "sharp", "of_the_fox" as IDs */
    assert(found_integer_field == true); /* Should detect numeric values */

    printf("  File: %s\n", analysis->filename);
    printf("  Category: %s\n", rogue_cfg_category_to_string(analysis->category));
    printf("  Format: %s\n", rogue_cfg_format_to_string(analysis->format));
    printf("  Data lines: %d\n", analysis->data_lines);
    printf("  Comment lines: %d\n", analysis->comment_lines);
    printf("  Fields detected: %d\n", analysis->field_count);

    /* Clean up */
    free(analysis);
    remove("test_analysis.cfg");

    print_test_result("File Analysis", true);
}

static void test_csv_parsing(void)
{
    print_test_header("CSV Parsing");

    /* Test CSV line parsing */
    RogueCfgRecord record = {0};

    bool result = rogue_cfg_parse_csv_line("PREFIX,sharp,damage_flat,1,3", &record);
    assert(result == true);
    assert(record.count == 5);
    assert(strcmp(record.values[0], "PREFIX") == 0);
    assert(strcmp(record.values[1], "sharp") == 0);
    assert(strcmp(record.values[2], "damage_flat") == 0);
    assert(strcmp(record.values[3], "1") == 0);
    assert(strcmp(record.values[4], "3") == 0);

    /* Test parsing with whitespace */
    result = rogue_cfg_parse_csv_line("  SUFFIX  ,  of_the_fox  ,  agility_flat  ", &record);
    assert(result == true);
    assert(record.count == 3);
    assert(strcmp(record.values[0], "SUFFIX") == 0);
    assert(strcmp(record.values[1], "of_the_fox") == 0);
    assert(strcmp(record.values[2], "agility_flat") == 0);

    /* Test empty line */
    result = rogue_cfg_parse_csv_line("", &record);
    assert(result == false);

    print_test_result("CSV Parsing", true);
}

static void test_key_value_parsing(void)
{
    print_test_header("Key-Value Parsing");

    /* Test key-value line parsing */
    RogueCfgKeyValuePair pair = {0};

    bool result = rogue_cfg_parse_key_value_line("max_health=100", &pair);
    assert(result == true);
    assert(strcmp(pair.key, "max_health") == 0);
    assert(strcmp(pair.value, "100") == 0);

    /* Test parsing with whitespace */
    result = rogue_cfg_parse_key_value_line("  player_speed  =  5.0  ", &pair);
    assert(result == true);
    assert(strcmp(pair.key, "player_speed") == 0);
    assert(strcmp(pair.value, "5.0") == 0);

    /* Test parsing with complex value */
    result = rogue_cfg_parse_key_value_line("game_title=My Awesome Roguelike Game", &pair);
    assert(result == true);
    assert(strcmp(pair.key, "game_title") == 0);
    assert(strcmp(pair.value, "My Awesome Roguelike Game") == 0);

    /* Test invalid format */
    result = rogue_cfg_parse_key_value_line("no_equals_sign", &pair);
    assert(result == false);

    print_test_result("Key-Value Parsing", true);
}

static void test_line_classification(void)
{
    print_test_header("Line Classification");

    /* Test comment line detection */
    assert(rogue_cfg_is_comment_line("# This is a comment") == true);
    assert(rogue_cfg_is_comment_line("  # Indented comment  ") == true);
    assert(rogue_cfg_is_comment_line("not a comment") == false);
    assert(rogue_cfg_is_comment_line("PREFIX,sharp,damage") == false);

    /* Test empty line detection */
    assert(rogue_cfg_is_empty_line("") == true);
    assert(rogue_cfg_is_empty_line("   ") == true);
    assert(rogue_cfg_is_empty_line("\t\t") == true);
    assert(rogue_cfg_is_empty_line("not empty") == false);

    print_test_result("Line Classification", true);
}

static void test_full_file_parsing(void)
{
    print_test_header("Full File Parsing");

    /* Create test files */
    create_test_affix_file("test_full_csv.cfg");
    create_test_keyvalue_file("test_full_kv.cfg");

    /* Test CSV file parsing */
    RogueCfgParseResult* csv_result = rogue_cfg_parse_file("test_full_csv.cfg");
    assert(csv_result != NULL);
    assert(csv_result->parse_success == true);
    assert(csv_result->detected_format == ROGUE_CFG_FORMAT_CSV);
    assert(csv_result->data.csv.record_count == 4); /* 4 data lines */

    /* Verify first record */
    RogueCfgRecord* first_record = &csv_result->data.csv.records[0];
    assert(first_record->count >= 5);
    assert(strcmp(first_record->values[0], "PREFIX") == 0);
    assert(strcmp(first_record->values[1], "sharp") == 0);

    printf("  CSV: Parsed %d records successfully\n", csv_result->data.csv.record_count);

    /* Test key-value file parsing */
    RogueCfgParseResult* kv_result = rogue_cfg_parse_file("test_full_kv.cfg");
    assert(kv_result != NULL);
    assert(kv_result->parse_success == true);
    assert(kv_result->detected_format == ROGUE_CFG_FORMAT_KEY_VALUE);
    assert(kv_result->data.key_value.pair_count == 4); /* 4 key-value pairs */

    /* Verify first pair */
    RogueCfgKeyValuePair* first_pair = &kv_result->data.key_value.pairs[0];
    assert(strcmp(first_pair->key, "max_health") == 0);
    assert(strcmp(first_pair->value, "100") == 0);

    printf("  Key-Value: Parsed %d pairs successfully\n", kv_result->data.key_value.pair_count);

    /* Clean up */
    rogue_cfg_free_parse_result(csv_result);
    rogue_cfg_free_parse_result(kv_result);
    remove("test_full_csv.cfg");
    remove("test_full_kv.cfg");

    print_test_result("Full File Parsing", true);
}

static void test_utility_functions(void)
{
    print_test_header("Utility Functions");

    /* Test data type to string conversion */
    assert(strcmp(rogue_cfg_data_type_to_string(ROGUE_CFG_DATA_TYPE_INTEGER), "integer") == 0);
    assert(strcmp(rogue_cfg_data_type_to_string(ROGUE_CFG_DATA_TYPE_FLOAT), "float") == 0);
    assert(strcmp(rogue_cfg_data_type_to_string(ROGUE_CFG_DATA_TYPE_STRING), "string") == 0);
    assert(strcmp(rogue_cfg_data_type_to_string(ROGUE_CFG_DATA_TYPE_ID), "id") == 0);
    assert(strcmp(rogue_cfg_data_type_to_string(ROGUE_CFG_DATA_TYPE_PATH), "path") == 0);
    assert(strcmp(rogue_cfg_data_type_to_string(ROGUE_CFG_DATA_TYPE_BOOLEAN), "boolean") == 0);

    /* Test format to string conversion */
    assert(strcmp(rogue_cfg_format_to_string(ROGUE_CFG_FORMAT_CSV), "CSV") == 0);
    assert(strcmp(rogue_cfg_format_to_string(ROGUE_CFG_FORMAT_KEY_VALUE), "Key-Value") == 0);
    assert(strcmp(rogue_cfg_format_to_string(ROGUE_CFG_FORMAT_LIST), "List") == 0);

    /* Test category to string conversion */
    assert(strcmp(rogue_cfg_category_to_string(ROGUE_CFG_CATEGORY_ITEMS), "Items") == 0);
    assert(strcmp(rogue_cfg_category_to_string(ROGUE_CFG_CATEGORY_AFFIXES), "Affixes") == 0);
    assert(strcmp(rogue_cfg_category_to_string(ROGUE_CFG_CATEGORY_LOOT_TABLES), "Loot Tables") ==
           0);

    print_test_result("Utility Functions", true);
}

static void test_error_handling(void)
{
    print_test_header("Error Handling");

    /* Test parsing non-existent file */
    RogueCfgParseResult* result = rogue_cfg_parse_file("nonexistent.cfg");
    assert(result != NULL);
    assert(result->parse_success == false);
    rogue_cfg_free_parse_result(result);

    /* Test analyzing non-existent file */
    RogueCfgFileAnalysis* analysis = rogue_cfg_analyze_file("nonexistent.cfg");
    assert(analysis != NULL);
    assert(analysis->validation_error_count > 0);
    free(analysis);

    /* Test NULL parameter handling */
    assert(rogue_cfg_classify_file(NULL) == ROGUE_CFG_CATEGORY_MISC);
    assert(rogue_cfg_parse_csv_line(NULL, NULL) == false);
    assert(rogue_cfg_parse_key_value_line(NULL, NULL) == false);
    assert(rogue_cfg_is_comment_line(NULL) == false);
    assert(rogue_cfg_is_empty_line(NULL) == true);

    print_test_result("Error Handling", true);
}

/* ===== Main Test Runner ===== */

int main(void)
{
    printf("=== CFG Parser Unit Tests ===\n\n");

    test_file_classification();
    test_format_detection();
    test_file_analysis();
    test_csv_parsing();
    test_key_value_parsing();
    test_line_classification();
    test_full_file_parsing();
    test_utility_functions();
    test_error_handling();

    printf("\n=== Test Results ===\n");
    printf("Tests run: 9\n");
    printf("Tests passed: 9\n");
    printf("Tests failed: 0\n");
    printf("All tests PASSED!\n");

    return 0;
}
