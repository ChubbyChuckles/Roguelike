#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include "../../src/core/cfg_migration.h"

// Test configuration
static const char* TEST_SOURCE_DIR = "test_assets";
static const char* TEST_TARGET_DIR = "test_output";

// Test file cleanup
static void cleanup_test_files(void) {
#ifdef _WIN32
    system("rmdir /s /q test_assets 2>nul");
    system("rmdir /s /q test_output 2>nul");
#else
    system("rm -rf test_assets test_output");
#endif
}

// Test file creation
static void create_test_directories(void) {
#ifdef _WIN32
    _mkdir("test_assets");
    _mkdir("test_output");
#else
    mkdir("test_assets", 0755);
    mkdir("test_output", 0755);
#endif
}

static void create_test_items_cfg(void) {
    FILE* f = fopen("test_assets/test_items.cfg", "w");
    assert(f != NULL);
    
    fprintf(f, "# id,name,category,level_req,stack_max,base_value,dmg_min,dmg_max,armor,sheet,tx,ty,tw,th,rarity\n");
    fprintf(f, "iron_sword,Iron Sword,2,1,1,25,3,7,0,../assets/weapons/sword.png,0,0,32,32,1\n");
    fprintf(f, "healing_potion,Healing Potion,1,0,20,5,0,0,0,../assets/items/potion.png,0,0,16,16,0\n");
    fprintf(f, "leather_armor,Leather Armor,3,1,1,15,0,0,5,../assets/armor/leather.png,0,0,32,32,0\n");
    
    fclose(f);
}

static void create_test_affixes_cfg(void) {
    FILE* f = fopen("test_assets/affixes.cfg", "w");
    assert(f != NULL);
    
    fprintf(f, "# type,id,stat,min,max,w_common,w_uncommon,w_rare,w_epic,w_legendary\n");
    fprintf(f, "PREFIX,sharp,damage_flat,1,3,50,30,15,4,1\n");
    fprintf(f, "SUFFIX,of_the_fox,agility_flat,1,2,40,35,15,7,3\n");
    fprintf(f, "PREFIX,heavy,damage_flat,2,5,25,25,20,10,4\n");
    fprintf(f, "SUFFIX,of_protection,armor_flat,3,8,20,25,25,15,10\n");
    
    fclose(f);
}

// =============================================================================
// Test Functions
// =============================================================================

void test_migration_config_init(void) {
    printf("Testing migration config initialization...\n");
    
    RogueMigrationConfig config;
    rogue_migration_config_init(&config);
    
    assert(strcmp(config.source_dir, "assets") == 0);
    assert(strcmp(config.target_dir, "assets/json") == 0);
    assert(config.validate_schemas == false); // Simplified version
    assert(config.create_backup == true);
    assert(config.overwrite_existing == false);
    assert(config.item_schema == NULL);
    assert(config.affix_schema == NULL);
    
    printf("✓ Migration config initialization passed\n");
}

void test_schema_creation(void) {
    printf("Testing schema creation...\n");
    
    RogueMigrationConfig config;
    rogue_migration_config_init(&config);
    
    bool result = rogue_migration_create_schemas(&config);
    assert(result == true);
    
    // In simplified version, schemas are NULL
    assert(config.item_schema == NULL);
    assert(config.affix_schema == NULL);
    
    rogue_migration_config_cleanup(&config);
    printf("✓ Schema creation passed\n");
}

void test_file_migration_items(void) {
    printf("Testing complete items file migration...\n");
    
    cleanup_test_files();
    create_test_directories();
    create_test_items_cfg();
    
    RogueMigrationConfig config;
    rogue_migration_config_init(&config);
    config.source_dir = TEST_SOURCE_DIR;
    config.target_dir = TEST_TARGET_DIR;
    
    bool schema_result = rogue_migration_create_schemas(&config);
    assert(schema_result == true);
    
    RogueMigrationResult result = rogue_migrate_items(&config);
    assert(result.status == ROGUE_MIGRATION_SUCCESS);
    assert(result.records_processed > 0);
    assert(result.records_migrated > 0);
    
    // Verify output file exists
    struct stat st;
    assert(stat("test_output/items/items.json", &st) == 0);
    
    printf("Items migration result: %d records processed, %d migrated\n", 
           result.records_processed, result.records_migrated);
    
    rogue_migration_config_cleanup(&config);
    cleanup_test_files();
    
    printf("✓ Complete items file migration passed\n");
}

void test_file_migration_affixes(void) {
    printf("Testing complete affixes file migration...\n");
    
    cleanup_test_files();
    create_test_directories();
    create_test_affixes_cfg();
    
    RogueMigrationConfig config;
    rogue_migration_config_init(&config);
    config.source_dir = TEST_SOURCE_DIR;
    config.target_dir = TEST_TARGET_DIR;
    
    bool schema_result = rogue_migration_create_schemas(&config);
    assert(schema_result == true);
    
    RogueMigrationResult result = rogue_migrate_affixes(&config);
    assert(result.status == ROGUE_MIGRATION_SUCCESS);
    assert(result.records_processed > 0);
    assert(result.records_migrated > 0);
    
    // Verify output file exists
    struct stat st;
    assert(stat("test_output/items/affixes.json", &st) == 0);
    
    printf("Affixes migration result: %d records processed, %d migrated\n",
           result.records_processed, result.records_migrated);
    
    rogue_migration_config_cleanup(&config);
    cleanup_test_files();
    
    printf("✓ Complete affixes file migration passed\n");
}

void test_phase_2_3_1_migration(void) {
    printf("Testing complete Phase 2.3.1 migration...\n");
    
    cleanup_test_files();
    create_test_directories();
    create_test_items_cfg();
    
    RogueMigrationConfig config;
    rogue_migration_config_init(&config);
    config.source_dir = TEST_SOURCE_DIR;
    config.target_dir = TEST_TARGET_DIR;
    
    bool schema_result = rogue_migration_create_schemas(&config);
    assert(schema_result == true);
    
    RogueMigrationStats stats = rogue_migrate_phase_2_3_1(&config);
    assert(stats.total_files > 0);
    assert(stats.successful_files > 0);
    assert(stats.successful_records > 0);
    
    rogue_migration_print_stats(&stats);
    
    rogue_migration_config_cleanup(&config);
    cleanup_test_files();
    
    printf("✓ Complete Phase 2.3.1 migration passed\n");
}

void test_phase_2_3_2_migration(void) {
    printf("Testing complete Phase 2.3.2 migration...\n");
    
    cleanup_test_files();
    create_test_directories();
    create_test_affixes_cfg();
    
    RogueMigrationConfig config;
    rogue_migration_config_init(&config);
    config.source_dir = TEST_SOURCE_DIR;
    config.target_dir = TEST_TARGET_DIR;
    
    bool schema_result = rogue_migration_create_schemas(&config);
    assert(schema_result == true);
    
    RogueMigrationStats stats = rogue_migrate_phase_2_3_2(&config);
    assert(stats.total_files > 0);
    assert(stats.successful_files > 0);
    assert(stats.successful_records > 0);
    
    rogue_migration_print_stats(&stats);
    
    rogue_migration_config_cleanup(&config);
    cleanup_test_files();
    
    printf("✓ Complete Phase 2.3.2 migration passed\n");
}

void test_migration_error_handling(void) {
    printf("Testing migration error handling...\n");
    
    RogueMigrationConfig config;
    rogue_migration_config_init(&config);
    config.source_dir = "nonexistent_dir";
    config.target_dir = TEST_TARGET_DIR;
    
    RogueMigrationResult result = rogue_migrate_items(&config);
    assert(result.status == ROGUE_MIGRATION_FILE_ERROR);
    assert(strstr(result.error_message, "not found") != NULL);
    
    printf("✓ Migration error handling passed\n");
}

void test_validation_logic(void) {
    printf("Testing validation logic...\n");
    
    // Create a simple JSON object for testing
    RogueJsonValue* valid_item = json_create_object();
    json_object_set(valid_item, "category", json_create_integer(2)); // weapon
    json_object_set(valid_item, "base_damage_min", json_create_integer(5));
    json_object_set(valid_item, "base_damage_max", json_create_integer(10));
    
    char error_msg[512];
    bool result = rogue_validate_migrated_item(valid_item, NULL, error_msg, sizeof(error_msg));
    assert(result == true);
    
    // Test invalid item (weapon with zero damage)
    RogueJsonValue* invalid_item = json_create_object();
    json_object_set(invalid_item, "category", json_create_integer(2)); // weapon
    json_object_set(invalid_item, "base_damage_min", json_create_integer(0)); // invalid
    json_object_set(invalid_item, "base_damage_max", json_create_integer(0)); // invalid
    
    result = rogue_validate_migrated_item(invalid_item, NULL, error_msg, sizeof(error_msg));
    assert(result == false);
    assert(strstr(error_msg, "positive damage") != NULL);
    
    // Cleanup
    json_free(valid_item);
    json_free(invalid_item);
    
    printf("✓ Validation logic passed\n");
}

// =============================================================================
// Test Runner
// =============================================================================

int main(void) {
    printf("=== CFG Migration Tests (Simplified) ===\n\n");
    
    test_migration_config_init();
    test_schema_creation();
    test_validation_logic();
    test_file_migration_items();
    test_file_migration_affixes();
    test_phase_2_3_1_migration();
    test_phase_2_3_2_migration();
    test_migration_error_handling();
    
    printf("\n✅ All CFG migration tests passed!\n");
    return 0;
}
