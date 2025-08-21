#define SDL_MAIN_HANDLED
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/enemy/enemy_integration.h"

/* Test enemy registry management */
static void test_enemy_registry_management(void)
{
    printf("  Testing enemy registry management...\n");

    RogueEnemyRegistry registry;
    memset(&registry, 0, sizeof(registry));

    /* Create test display info */
    RogueEnemyDisplayInfo display_info;
    memset(&display_info, 0, sizeof(display_info));
    strcpy_s(display_info.name, sizeof(display_info.name), "Test Goblin");
    display_info.delta_level = 3;
    display_info.final_stats.hp = 100.0f;
    display_info.final_stats.damage = 15.0f;
    display_info.final_stats.defense = 5.0f;

    /* Register first enemy */
    float pos1[2] = {10.0f, 20.0f};
    int enemy_id1 = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos1, &display_info);
    assert(enemy_id1 >= 0);
    assert(registry.count == 1);

    /* Register second enemy */
    float pos2[2] = {30.0f, 40.0f};
    strcpy_s(display_info.name, sizeof(display_info.name), "Test Orc");
    display_info.delta_level = 5;
    int enemy_id2 = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos2, &display_info);
    assert(enemy_id2 >= 0);
    assert(enemy_id2 != enemy_id1);
    assert(registry.count == 2);

    /* Verify registry entries */
    assert(registry.entries[0].enemy_id == enemy_id1);
    assert(registry.entries[0].is_alive == 1);
    assert(registry.entries[1].enemy_id == enemy_id2);
    assert(registry.entries[1].is_alive == 1);

    printf("    ✓ Enemy registration working correctly\n");
}

static void test_nearest_enemy_search(void)
{
    printf("  Testing nearest enemy search...\n");

    RogueEnemyRegistry registry;
    memset(&registry, 0, sizeof(registry));

    /* Create test display info */
    RogueEnemyDisplayInfo display_info;
    memset(&display_info, 0, sizeof(display_info));
    strcpy_s(display_info.name, sizeof(display_info.name), "Test Enemy");
    display_info.final_stats.hp = 50.0f;

    /* Register enemies at known positions */
    float pos1[2] = {0.0f, 0.0f};  /* Distance 0 from origin */
    float pos2[2] = {5.0f, 0.0f};  /* Distance 5 from origin */
    float pos3[2] = {0.0f, 10.0f}; /* Distance 10 from origin */

    int id1 = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos1, &display_info);
    int id2 = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos2, &display_info);
    int id3 = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos3, &display_info);

    /* Test nearest search from origin */
    float search_pos[2] = {0.0f, 0.0f};
    int found_id;
    assert(rogue_enemy_integration_find_nearest_enemy(&registry, search_pos, 15.0f, &found_id));
    assert(found_id == id1); /* Should find the enemy at (0,0) */

    /* Test nearest search with limited range */
    assert(rogue_enemy_integration_find_nearest_enemy(&registry, search_pos, 3.0f, &found_id));
    assert(found_id == id1); /* Should still find (0,0) enemy */

    assert(!rogue_enemy_integration_find_nearest_enemy(&registry, search_pos, 0.5f, &found_id));
    /* Should find nothing within 0.5 units */

    /* Test from different position */
    float search_pos2[2] = {4.0f, 0.0f};
    assert(rogue_enemy_integration_find_nearest_enemy(&registry, search_pos2, 10.0f, &found_id));
    assert(found_id == id2); /* Should find enemy at (5,0) - closest to (4,0) */

    printf("    ✓ Nearest enemy search working correctly\n");
}

static void test_enemy_at_position_search(void)
{
    printf("  Testing enemy at position search...\n");

    RogueEnemyRegistry registry;
    memset(&registry, 0, sizeof(registry));

    RogueEnemyDisplayInfo display_info;
    memset(&display_info, 0, sizeof(display_info));
    strcpy_s(display_info.name, sizeof(display_info.name), "Target Enemy");

    /* Register enemy at specific position */
    float enemy_pos[2] = {15.0f, 25.0f};
    int enemy_id =
        rogue_enemy_integration_register_enemy(&registry, 1, 100, enemy_pos, &display_info);

    /* Test exact position match */
    int found_id;
    assert(rogue_enemy_integration_find_enemy_at_position(&registry, enemy_pos, 0.1f, &found_id));
    assert(found_id == enemy_id);

    /* Test nearby position within tolerance */
    float nearby_pos[2] = {15.2f, 25.1f};
    assert(rogue_enemy_integration_find_enemy_at_position(&registry, nearby_pos, 0.5f, &found_id));
    assert(found_id == enemy_id);

    /* Test position outside tolerance */
    float far_pos[2] = {16.0f, 26.0f};
    assert(!rogue_enemy_integration_find_enemy_at_position(&registry, far_pos, 0.5f, &found_id));

    printf("    ✓ Enemy at position search working correctly\n");
}

static void test_combat_stats_retrieval(void)
{
    printf("  Testing combat stats retrieval...\n");

    RogueEnemyRegistry registry;
    memset(&registry, 0, sizeof(registry));

    /* Create enemy with specific stats */
    RogueEnemyDisplayInfo display_info;
    memset(&display_info, 0, sizeof(display_info));
    strcpy_s(display_info.name, sizeof(display_info.name), "Combat Test Enemy");
    display_info.final_stats.hp = 150.0f;
    display_info.final_stats.damage = 25.0f;
    display_info.final_stats.defense = 10.0f;

    float pos[2] = {0.0f, 0.0f};
    int enemy_id = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos, &display_info);

    /* Retrieve combat stats */
    RogueEnemyCombatStats combat_stats;
    assert(rogue_enemy_integration_get_combat_stats(&registry, enemy_id, &combat_stats));

    /* Verify stats transfer correctly */
    assert(fabs(combat_stats.max_health - 150.0f) < 0.001f);
    assert(fabs(combat_stats.current_health - 150.0f) < 0.001f);
    assert(fabs(combat_stats.base_damage - 25.0f) < 0.001f);
    assert(fabs(combat_stats.armor_rating - 10.0f) < 0.001f);

    /* Test invalid enemy ID */
    assert(!rogue_enemy_integration_get_combat_stats(&registry, 999, &combat_stats));

    printf("    ✓ Combat stats retrieval working correctly\n");
}

static void test_damage_application(void)
{
    printf("  Testing damage application and death mechanics...\n");

    RogueEnemyRegistry registry;
    memset(&registry, 0, sizeof(registry));

    /* Create enemy with known health */
    RogueEnemyDisplayInfo display_info;
    memset(&display_info, 0, sizeof(display_info));
    strcpy_s(display_info.name, sizeof(display_info.name), "Damage Test Enemy");
    display_info.final_stats.hp = 100.0f;
    display_info.final_stats.defense = 20.0f; /* 20/(20+100) = 1/6 damage reduction */

    float pos[2] = {0.0f, 0.0f};
    int enemy_id = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos, &display_info);

    /* Apply physical damage (should be reduced by armor) */
    int damage_result =
        rogue_enemy_integration_apply_damage(&registry, enemy_id, 60.0f, 0); /* physical */
    assert(damage_result == 1); /* Enemy damaged but alive */

    /* Verify health reduction (60 * (1 - 1/6) = 60 * 5/6 = 50 damage) */
    /* Enemy should have 100 - 50 = 50 health remaining */
    assert(registry.entries[0].display_info.final_stats.hp > 45.0f); /* Allow for rounding */
    assert(registry.entries[0].display_info.final_stats.hp < 55.0f);
    assert(registry.entries[0].is_alive == 1);

    /* Apply magical damage (no armor reduction) */
    damage_result = rogue_enemy_integration_apply_damage(&registry, enemy_id, 60.0f, 1); /* fire */
    assert(damage_result == 2); /* Enemy should die */
    assert(registry.entries[0].is_alive == 0);

    /* Try to damage dead enemy */
    damage_result = rogue_enemy_integration_apply_damage(&registry, enemy_id, 10.0f, 0);
    assert(damage_result == 0); /* No effect on dead enemy */

    printf("    ✓ Damage application and death mechanics working correctly\n");
}

static void test_position_updates(void)
{
    printf("  Testing enemy position updates...\n");

    RogueEnemyRegistry registry;
    memset(&registry, 0, sizeof(registry));

    RogueEnemyDisplayInfo display_info;
    memset(&display_info, 0, sizeof(display_info));
    strcpy_s(display_info.name, sizeof(display_info.name), "Moving Enemy");

    /* Register enemy at initial position */
    float initial_pos[2] = {5.0f, 10.0f};
    int enemy_id =
        rogue_enemy_integration_register_enemy(&registry, 1, 100, initial_pos, &display_info);

    /* Verify initial position */
    assert(fabs(registry.entries[0].position[0] - 5.0f) < 0.001f);
    assert(fabs(registry.entries[0].position[1] - 10.0f) < 0.001f);

    /* Update position */
    float new_pos[2] = {15.0f, 25.0f};
    rogue_enemy_integration_update_enemy_position(&registry, enemy_id, new_pos);

    /* Verify position update */
    assert(fabs(registry.entries[0].position[0] - 15.0f) < 0.001f);
    assert(fabs(registry.entries[0].position[1] - 25.0f) < 0.001f);

    /* Test nearest search with updated position */
    float search_pos[2] = {16.0f, 24.0f};
    int found_id;
    assert(rogue_enemy_integration_find_nearest_enemy(&registry, search_pos, 5.0f, &found_id));
    assert(found_id == enemy_id);

    printf("    ✓ Enemy position updates working correctly\n");
}

static void test_registry_cleanup(void)
{
    printf("  Testing registry cleanup of dead enemies...\n");

    RogueEnemyRegistry registry;
    memset(&registry, 0, sizeof(registry));

    RogueEnemyDisplayInfo display_info;
    memset(&display_info, 0, sizeof(display_info));
    strcpy_s(display_info.name, sizeof(display_info.name), "Cleanup Test");
    display_info.final_stats.hp = 50.0f;

    /* Register multiple enemies */
    float pos1[2] = {0.0f, 0.0f};
    float pos2[2] = {5.0f, 5.0f};
    float pos3[2] = {10.0f, 10.0f};

    int id1 = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos1, &display_info);
    int id2 = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos2, &display_info);
    int id3 = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos3, &display_info);

    assert(registry.count == 3);

    /* Kill middle enemy */
    rogue_enemy_integration_mark_enemy_dead(&registry, id2);
    assert(registry.entries[1].is_alive == 0);
    assert(registry.count == 3); /* Count doesn't change until cleanup */

    /* Clean up dead enemies */
    rogue_enemy_integration_cleanup_dead_enemies(&registry);
    assert(registry.count == 2);

    /* Verify remaining enemies are still findable */
    int found_id;
    float search_pos[2] = {0.0f, 0.0f};
    assert(rogue_enemy_integration_find_nearest_enemy(&registry, search_pos, 20.0f, &found_id));
    assert(found_id == id1 || found_id == id3); /* Should find one of the living enemies */

    /* Verify dead enemy is not findable */
    float dead_pos[2] = {5.0f, 5.0f};
    assert(!rogue_enemy_integration_find_enemy_at_position(&registry, dead_pos, 0.5f, &found_id));

    printf("    ✓ Registry cleanup working correctly\n");
}

static void test_display_info_retrieval(void)
{
    printf("  Testing display info retrieval for HUD...\n");

    RogueEnemyRegistry registry;
    memset(&registry, 0, sizeof(registry));

    /* Create enemy with specific display properties */
    RogueEnemyDisplayInfo original_display;
    memset(&original_display, 0, sizeof(original_display));
    strcpy_s(original_display.name, sizeof(original_display.name), "HUD Test Enemy");
    original_display.delta_level = 7;
    original_display.is_elite = 1;
    original_display.is_elite = 1;
    original_display.modifier_count = 2;
    strcpy_s(original_display.modifier_tags[0], sizeof(original_display.modifier_tags[0]), "swift");
    strcpy_s(original_display.modifier_tags[1], sizeof(original_display.modifier_tags[1]),
             "armored");

    float pos[2] = {0.0f, 0.0f};
    int enemy_id =
        rogue_enemy_integration_register_enemy(&registry, 1, 100, pos, &original_display);

    /* Retrieve display info */
    RogueEnemyDisplayInfo retrieved_display;
    assert(rogue_enemy_integration_get_enemy_display_info(&registry, enemy_id, &retrieved_display));

    /* Verify all fields transferred correctly */
    assert(strcmp(retrieved_display.name, "HUD Test Enemy") == 0);
    assert(retrieved_display.delta_level == 7);
    assert(retrieved_display.is_elite == 1);
    assert(retrieved_display.is_elite == 1);
    assert(retrieved_display.modifier_count == 2);
    assert(strcmp(retrieved_display.modifier_tags[0], "swift") == 0);
    assert(strcmp(retrieved_display.modifier_tags[1], "armored") == 0);

    /* Test invalid enemy ID */
    assert(!rogue_enemy_integration_get_enemy_display_info(&registry, 999, &retrieved_display));

    printf("    ✓ Display info retrieval working correctly\n");
}

static void test_error_handling_edge_cases(void)
{
    printf("  Testing error handling and edge cases...\n");

    /* Test null pointer handling */
    assert(!rogue_enemy_integration_find_nearest_enemy(NULL, NULL, 10.0f, NULL));
    assert(!rogue_enemy_integration_find_enemy_at_position(NULL, NULL, 1.0f, NULL));
    assert(rogue_enemy_integration_register_enemy(NULL, 1, 100, NULL, NULL) == -1);

    RogueEnemyRegistry registry;
    memset(&registry, 0, sizeof(registry));

    /* Test registry overflow */
    registry.count = MAX_REGISTERED_ENEMIES;
    RogueEnemyDisplayInfo display_info;
    memset(&display_info, 0, sizeof(display_info));
    float pos[2] = {0.0f, 0.0f};

    assert(rogue_enemy_integration_register_enemy(&registry, 1, 100, pos, &display_info) == -1);

    /* Test invalid damage values */
    registry.count = 0; /* Reset registry */
    int enemy_id = rogue_enemy_integration_register_enemy(&registry, 1, 100, pos, &display_info);
    assert(enemy_id >= 0);

    assert(rogue_enemy_integration_apply_damage(&registry, enemy_id, -10.0f, 0) ==
           0); /* Negative damage */

    printf("    ✓ Error handling working correctly\n");
}

int main(void)
{
    printf("Running Enemy Integration Phase 6 Tests (Target Acquisition & Combat Hook)...\n");

    test_enemy_registry_management();
    test_nearest_enemy_search();
    test_enemy_at_position_search();
    test_combat_stats_retrieval();
    test_damage_application();
    test_position_updates();
    test_registry_cleanup();
    test_display_info_retrieval();
    test_error_handling_edge_cases();

    printf("All Phase 6 tests passed! ✓\n");
    return 0;
}
