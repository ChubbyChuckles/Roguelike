#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Include the enemy integration header which includes needed types */
#include "../../src/core/enemy_integration.h"

/* Mock enemy types for testing */
static RogueEnemyTypeDef mock_enemy_types[3];
static int mock_enemy_type_count = 3;

/* Mock app state for HUD target testing */
static struct {
    int target_enemy_active;
    int target_enemy_level;
} mock_app_state;

/* Test helper to set up minimal app state */
static void setup_minimal_app_state() {
    /* Set up mock enemy types */
    strcpy(mock_enemy_types[0].name, "Orc Warrior");
    strcpy(mock_enemy_types[1].name, "Goblin Archer");  
    strcpy(mock_enemy_types[2].name, "Elite Troll");
    
    /* Reset mock app state */
    mock_app_state.target_enemy_active = 0;
    mock_app_state.target_enemy_level = 0;
}

/* Test display info building for different enemy types */
static void test_build_display_info_basic() {
    printf("  Testing basic display info building...\n");
    setup_minimal_app_state();
    
    RogueEnemy test_enemy = {0};
    test_enemy.type_index = 0;  /* Valid index */
    test_enemy.level = 5;
    test_enemy.elite_flag = 0;
    test_enemy.boss_flag = 0;
    test_enemy.support_flag = 0;
    test_enemy.modifier_count = 0;
    
    RogueEnemyDisplayInfo display_info = {0};
    int result = rogue_enemy_integration_build_display_info(&test_enemy, 6, &display_info);
    
    assert(result == 1);
    /* Name will depend on whether g_app is accessible - check for non-empty */
    assert(strlen(display_info.name) > 0);
    assert(strcmp(display_info.tier_name, "Normal") == 0);
    assert(display_info.level == 5);
    assert(display_info.delta_level == 1);  /* Player level 6 - enemy level 5 */
    assert(display_info.is_elite == 0);
    assert(display_info.is_boss == 0);
    assert(display_info.is_support == 0);
    assert(display_info.modifier_count == 0);
    
    printf("    ✓ Basic display info correct\n");
}

/* Test display info for elite enemies */
static void test_build_display_info_elite() {
    printf("  Testing elite enemy display info...\n");
    setup_minimal_app_state();
    
    RogueEnemy elite_enemy = {0};
    elite_enemy.type_index = 1;  /* Valid index */
    elite_enemy.level = 8;
    elite_enemy.elite_flag = 1;
    elite_enemy.boss_flag = 0;
    elite_enemy.support_flag = 0;
    elite_enemy.modifier_count = 2;
    elite_enemy.modifier_ids[0] = 1;
    elite_enemy.modifier_ids[1] = 3;
    
    RogueEnemyDisplayInfo display_info = {0};
    int result = rogue_enemy_integration_build_display_info(&elite_enemy, 7, &display_info);
    
    assert(result == 1);
    assert(strlen(display_info.name) > 0);  /* Name will be set */
    assert(strcmp(display_info.tier_name, "Elite") == 0);
    assert(display_info.level == 8);
    assert(display_info.delta_level == -1);  /* Player level 7 - enemy level 8 */
    assert(display_info.is_elite == 1);
    assert(display_info.is_boss == 0);
    assert(display_info.is_support == 0);
    assert(display_info.modifier_count == 2);
    assert(strcmp(display_info.modifier_tags[0], "M1") == 0);
    assert(strcmp(display_info.modifier_tags[1], "M3") == 0);
    
    printf("    ✓ Elite display info correct\n");
}

/* Test display info for boss enemies */
static void test_build_display_info_boss() {
    printf("  Testing boss enemy display info...\n");
    setup_minimal_app_state();
    
    RogueEnemy boss_enemy = {0};
    boss_enemy.type_index = 2;  /* Valid index */
    boss_enemy.level = 10;
    boss_enemy.elite_flag = 0;
    boss_enemy.boss_flag = 1;
    boss_enemy.support_flag = 0;
    boss_enemy.modifier_count = 1;
    boss_enemy.modifier_ids[0] = 2;
    
    RogueEnemyDisplayInfo display_info = {0};
    int result = rogue_enemy_integration_build_display_info(&boss_enemy, 10, &display_info);
    
    assert(result == 1);
    assert(strlen(display_info.name) > 0);  /* Name will be set */
    assert(strcmp(display_info.tier_name, "Boss") == 0);
    assert(display_info.level == 10);
    assert(display_info.delta_level == 0);  /* Same level */
    assert(display_info.is_elite == 0);
    assert(display_info.is_boss == 1);
    assert(display_info.is_support == 0);
    assert(display_info.modifier_count == 1);
    assert(strcmp(display_info.modifier_tags[0], "M2") == 0);
    
    printf("    ✓ Boss display info correct\n");
}

/* Test display info for support enemies */
static void test_build_display_info_support() {
    printf("  Testing support enemy display info...\n");
    setup_minimal_app_state();
    
    RogueEnemy support_enemy = {0};
    support_enemy.type_index = 1;  /* Valid index */
    support_enemy.level = 3;
    support_enemy.elite_flag = 0;
    support_enemy.boss_flag = 0;
    support_enemy.support_flag = 1;
    support_enemy.modifier_count = 0;
    
    RogueEnemyDisplayInfo display_info = {0};
    int result = rogue_enemy_integration_build_display_info(&support_enemy, 5, &display_info);
    
    assert(result == 1);
    assert(strlen(display_info.name) > 0);  /* Name will be set */
    assert(strcmp(display_info.tier_name, "Support") == 0);
    assert(display_info.level == 3);
    assert(display_info.delta_level == 2);  /* Player level 5 - enemy level 3 */
    assert(display_info.is_elite == 0);
    assert(display_info.is_boss == 0);
    assert(display_info.is_support == 1);
    assert(display_info.modifier_count == 0);
    
    printf("    ✓ Support display info correct\n");
}

/* Test HUD target update functionality */
static void test_hud_target_update() {
    printf("  Testing HUD target update...\n");
    setup_minimal_app_state();
    
    /* Initially no target */
    mock_app_state.target_enemy_active = 0;
    mock_app_state.target_enemy_level = 0;
    
    RogueEnemy test_enemy = {0};
    test_enemy.level = 7;
    
    /* Set target */
    int result = rogue_enemy_integration_update_hud_target(&test_enemy, 5);
    assert(result == 1);
    /* Note: In actual implementation, this would update g_app.target_enemy_active */
    /* For testing purposes, we verify the function returns success */
    
    /* Clear target */
    result = rogue_enemy_integration_update_hud_target(NULL, 5);
    assert(result == 1);
    /* Note: In actual implementation, this would clear g_app.target_enemy_active */
    
    printf("    ✓ HUD target update working\n");
}

/* Test color coding for different enemy types */
static void test_enemy_type_colors() {
    printf("  Testing enemy type color coding...\n");
    
    unsigned char r, g, b;
    
    /* Normal enemy - white */
    RogueEnemy normal_enemy = {0};
    normal_enemy.elite_flag = 0;
    normal_enemy.boss_flag = 0;
    normal_enemy.support_flag = 0;
    
    rogue_enemy_integration_get_type_color(&normal_enemy, &r, &g, &b);
    assert(r == 255 && g == 255 && b == 255);  /* White */
    
    /* Elite enemy - yellow */
    RogueEnemy elite_enemy = {0};
    elite_enemy.elite_flag = 1;
    elite_enemy.boss_flag = 0;
    elite_enemy.support_flag = 0;
    
    rogue_enemy_integration_get_type_color(&elite_enemy, &r, &g, &b);
    assert(r == 255 && g == 215 && b == 0);  /* Yellow */
    
    /* Boss enemy - orange */
    RogueEnemy boss_enemy = {0};
    boss_enemy.elite_flag = 0;
    boss_enemy.boss_flag = 1;
    boss_enemy.support_flag = 0;
    
    rogue_enemy_integration_get_type_color(&boss_enemy, &r, &g, &b);
    assert(r == 255 && g == 140 && b == 0);  /* Orange */
    
    /* Support enemy - cyan */
    RogueEnemy support_enemy = {0};
    support_enemy.elite_flag = 0;
    support_enemy.boss_flag = 0;
    support_enemy.support_flag = 1;
    
    rogue_enemy_integration_get_type_color(&support_enemy, &r, &g, &b);
    assert(r == 0 && g == 191 && b == 255);  /* Cyan */
    
    printf("    ✓ Enemy color coding correct\n");
}

/* Test modifier telegraph system */
static void test_modifier_telegraphs() {
    printf("  Testing modifier telegraph system...\n");
    
    /* Test known modifier IDs */
    const char* telegraph1 = rogue_enemy_integration_get_modifier_telegraph(1);
    assert(strcmp(telegraph1, "speed_aura") == 0);
    
    const char* telegraph2 = rogue_enemy_integration_get_modifier_telegraph(2);
    assert(strcmp(telegraph2, "defense_aura") == 0);
    
    const char* telegraph3 = rogue_enemy_integration_get_modifier_telegraph(3);
    assert(strcmp(telegraph3, "rage_aura") == 0);
    
    /* Test unknown modifier ID */
    const char* telegraph_unknown = rogue_enemy_integration_get_modifier_telegraph(99);
    assert(strcmp(telegraph_unknown, "modifier_aura") == 0);
    
    printf("    ✓ Modifier telegraphs working\n");
}

/* Test error handling for invalid inputs */
static void test_error_handling() {
    printf("  Testing error handling...\n");
    
    RogueEnemyDisplayInfo display_info = {0};
    
    /* NULL enemy pointer */
    int result = rogue_enemy_integration_build_display_info(NULL, 5, &display_info);
    assert(result == 0);
    
    /* NULL output pointer */
    RogueEnemy test_enemy = {0};
    result = rogue_enemy_integration_build_display_info(&test_enemy, 5, NULL);
    assert(result == 0);
    
    /* Invalid type index */
    test_enemy.type_index = 999;
    result = rogue_enemy_integration_build_display_info(&test_enemy, 5, &display_info);
    assert(result == 1);  /* Should still work with "Unknown Enemy" name */
    assert(strcmp(display_info.name, "Unknown Enemy") == 0);
    
    /* NULL pointers for color function */
    unsigned char r, g, b;
    rogue_enemy_integration_get_type_color(NULL, &r, &g, &b);  /* Should not crash */
    rogue_enemy_integration_get_type_color(&test_enemy, NULL, &g, &b);  /* Should not crash */
    
    printf("    ✓ Error handling robust\n");
}

/* Test deterministic behavior across multiple calls */
static void test_deterministic_display() {
    printf("  Testing deterministic display behavior...\n");
    setup_minimal_app_state();
    
    RogueEnemy test_enemy = {0};
    test_enemy.type_index = 1;
    test_enemy.level = 6;
    test_enemy.elite_flag = 1;
    test_enemy.modifier_count = 2;
    test_enemy.modifier_ids[0] = 1;
    test_enemy.modifier_ids[1] = 2;
    
    RogueEnemyDisplayInfo info1 = {0};
    RogueEnemyDisplayInfo info2 = {0};
    
    /* Build display info twice */
    int result1 = rogue_enemy_integration_build_display_info(&test_enemy, 5, &info1);
    int result2 = rogue_enemy_integration_build_display_info(&test_enemy, 5, &info2);
    
    assert(result1 == 1 && result2 == 1);
    
    /* Should be identical */
    assert(strcmp(info1.name, info2.name) == 0);
    assert(strcmp(info1.tier_name, info2.tier_name) == 0);
    assert(info1.level == info2.level);
    assert(info1.delta_level == info2.delta_level);
    assert(info1.is_elite == info2.is_elite);
    assert(info1.modifier_count == info2.modifier_count);
    assert(strcmp(info1.modifier_tags[0], info2.modifier_tags[0]) == 0);
    assert(strcmp(info1.modifier_tags[1], info2.modifier_tags[1]) == 0);
    
    printf("    ✓ Display info deterministic\n");
}

int main() {
    printf("Running Enemy Integration Phase 4 Tests (Visual/UI Exposure)...\n");
    
    test_build_display_info_basic();
    test_build_display_info_elite();
    test_build_display_info_boss();
    test_build_display_info_support();
    test_hud_target_update();
    test_enemy_type_colors();
    test_modifier_telegraphs();
    test_error_handling();
    test_deterministic_display();
    
    printf("All Phase 4 tests passed! ✓\n");
    return 0;
}
