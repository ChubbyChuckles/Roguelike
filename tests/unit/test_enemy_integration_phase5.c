#define SDL_MAIN_HANDLED
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/enemy/enemy_integration.h"

/* Test spawn position solver for basic room layout */
static void test_spawn_position_solver_basic()
{
    printf("  Testing basic spawn position solver...\n");

    RogueRoomEncounterInfo encounter_info = {0};
    encounter_info.room_id = 1;
    encounter_info.depth_level = 3; /* This determines unit count */
    encounter_info.biome_id = 1;
    encounter_info.encounter_template_id = 0;

    RogueRoomDimensions room_dims = {0};
    room_dims.min_x = 0.0f;
    room_dims.min_y = 0.0f;
    room_dims.max_x = 10.0f;
    room_dims.max_y = 10.0f;
    room_dims.obstacle_count = 0;

    RogueSpawnSolution solution = {0};
    int result =
        rogue_enemy_integration_solve_spawn_positions(&encounter_info, &room_dims, &solution);

    assert(result == 1);
    assert(solution.position_count > 0);

    /* Verify all positions are within room bounds */
    for (int i = 0; i < solution.position_count; i++)
    {
        assert(solution.positions[i][0] >= room_dims.min_x);
        assert(solution.positions[i][0] <= room_dims.max_x);
        assert(solution.positions[i][1] >= room_dims.min_y);
        assert(solution.positions[i][1] <= room_dims.max_y);
    }

    printf("    ✓ Basic spawn positions generated within bounds\n");
}

/* Test spawn position solver with boss enemy */
static void test_spawn_position_solver_boss()
{
    printf("  Testing boss spawn positioning...\n");

    RogueRoomEncounterInfo encounter_info = {0};
    encounter_info.room_id = 1;
    encounter_info.depth_level = 6; /* Deep enough to trigger boss spawn */
    encounter_info.biome_id = 1;
    encounter_info.encounter_template_id = 0;

    RogueRoomDimensions room_dims = {0};
    room_dims.min_x = 2.0f;
    room_dims.min_y = 2.0f;
    room_dims.max_x = 12.0f;
    room_dims.max_y = 12.0f;
    room_dims.obstacle_count = 0;

    RogueSpawnSolution solution = {0};
    int result =
        rogue_enemy_integration_solve_spawn_positions(&encounter_info, &room_dims, &solution);

    assert(result == 1);
    assert(solution.position_count > 0);

    /* Boss should be positioned first (center-anchored) for deep levels */
    float center_x = (room_dims.min_x + room_dims.max_x) / 2.0f;
    float center_y = (room_dims.min_y + room_dims.max_y) / 2.0f;

    /* First position should be boss position (close to center) */
    float boss_x = solution.positions[0][0];
    float boss_y = solution.positions[0][1];
    float dist_to_center = sqrtf((boss_x - center_x) * (boss_x - center_x) +
                                 (boss_y - center_y) * (boss_y - center_y));
    assert(dist_to_center < 2.0f); /* Boss should be near center */

    printf("    ✓ Boss positioned near room center\n");
}

/* Test spawn position solver with obstacles */
static void test_spawn_position_solver_obstacles()
{
    printf("  Testing spawn positioning with obstacles...\n");

    RogueRoomEncounterInfo encounter_info = {0};
    encounter_info.room_id = 1;
    encounter_info.depth_level = 2; /* Low level, few enemies */
    encounter_info.biome_id = 1;
    encounter_info.encounter_template_id = 0;

    RogueRoomDimensions room_dims = {0};
    room_dims.min_x = 0.0f;
    room_dims.min_y = 0.0f;
    room_dims.max_x = 8.0f;
    room_dims.max_y = 8.0f;
    room_dims.obstacle_count = 2;

    /* Add obstacle zones */
    room_dims.obstacle_zones[0][0] = 2.0f;
    room_dims.obstacle_zones[0][1] = 2.0f;
    room_dims.obstacle_zones[0][2] = 4.0f;
    room_dims.obstacle_zones[0][3] = 4.0f;

    room_dims.obstacle_zones[1][0] = 5.0f;
    room_dims.obstacle_zones[1][1] = 5.0f;
    room_dims.obstacle_zones[1][2] = 7.0f;
    room_dims.obstacle_zones[1][3] = 7.0f;

    RogueSpawnSolution solution = {0};
    int result =
        rogue_enemy_integration_solve_spawn_positions(&encounter_info, &room_dims, &solution);

    assert(result == 1);
    assert(solution.position_count > 0);

    /* Verify no spawns are inside obstacle zones */
    for (int i = 0; i < solution.position_count; i++)
    {
        float x = solution.positions[i][0];
        float y = solution.positions[i][1];

        /* Check against first obstacle */
        assert(!(x >= 2.0f && x <= 4.0f && y >= 2.0f && y <= 4.0f));
        /* Check against second obstacle */
        assert(!(x >= 5.0f && x <= 7.0f && y >= 5.0f && y <= 7.0f));
    }

    printf("    ✓ Spawns avoid obstacle zones\n");
}

/* Test spawn position validation function */
static void test_spawn_position_validation()
{
    printf("  Testing spawn position validation...\n");

    RogueRoomDimensions room_dims = {0};
    room_dims.min_x = 1.0f;
    room_dims.min_y = 1.0f;
    room_dims.max_x = 9.0f;
    room_dims.max_y = 9.0f;
    room_dims.obstacle_count = 1;
    room_dims.obstacle_zones[0][0] = 4.0f;
    room_dims.obstacle_zones[0][1] = 4.0f;
    room_dims.obstacle_zones[0][2] = 6.0f;
    room_dims.obstacle_zones[0][3] = 6.0f;

    /* Test valid position */
    int valid = rogue_enemy_integration_validate_spawn_position(2.0f, 2.0f, &room_dims, NULL, 0);
    assert(valid == 1);

    /* Test position outside room bounds */
    valid = rogue_enemy_integration_validate_spawn_position(0.5f, 2.0f, &room_dims, NULL, 0);
    assert(valid == 0);

    valid = rogue_enemy_integration_validate_spawn_position(10.0f, 5.0f, &room_dims, NULL, 0);
    assert(valid == 0);

    /* Test position inside obstacle */
    valid = rogue_enemy_integration_validate_spawn_position(5.0f, 5.0f, &room_dims, NULL, 0);
    assert(valid == 0);

    /* Test minimum distance constraint */
    RogueSpawnSolution existing = {0};
    existing.position_count = 1;
    existing.positions[0][0] = 3.0f;
    existing.positions[0][1] = 3.0f;
    existing.min_distance = 2.0f;

    valid = rogue_enemy_integration_validate_spawn_position(4.0f, 3.0f, &room_dims, &existing, 1);
    assert(valid == 0); /* Too close */

    valid = rogue_enemy_integration_validate_spawn_position(6.0f, 3.0f, &room_dims, &existing, 1);
    assert(valid == 1); /* Far enough */

    printf("    ✓ Position validation working correctly\n");
}

/* Test minimum distance enforcement between spawns */
static void test_spawn_minimum_distance()
{
    printf("  Testing minimum distance enforcement...\n");

    RogueRoomEncounterInfo encounter_info = {0};
    encounter_info.room_id = 1;
    encounter_info.depth_level = 4; /* Moderate depth for multiple enemies */
    encounter_info.biome_id = 1;
    encounter_info.encounter_template_id = 0;

    RogueRoomDimensions room_dims = {0};
    room_dims.min_x = 0.0f;
    room_dims.min_y = 0.0f;
    room_dims.max_x = 15.0f; /* Large room to allow proper spacing */
    room_dims.max_y = 15.0f;
    room_dims.obstacle_count = 0;

    RogueSpawnSolution solution = {0};
    int result =
        rogue_enemy_integration_solve_spawn_positions(&encounter_info, &room_dims, &solution);

    assert(result == 1);
    assert(solution.position_count >= 2); /* Need at least 2 to test distance */

    /* Verify minimum distance between all spawn points */
    for (int i = 0; i < solution.position_count; i++)
    {
        for (int j = i + 1; j < solution.position_count; j++)
        {
            float dx = solution.positions[i][0] - solution.positions[j][0];
            float dy = solution.positions[i][1] - solution.positions[j][1];
            float distance = sqrtf(dx * dx + dy * dy);

            /* Allow small tolerance for floating point */
            assert(distance >= (solution.min_distance - 0.1f));
        }
    }

    printf("    ✓ Minimum distance maintained between all spawns\n");
}

/* Test navmesh registration placeholder */
static void test_navmesh_registration()
{
    printf("  Testing navmesh registration...\n");

    RogueSpawnSolution solution = {0};
    solution.position_count = 3;
    solution.positions[0][0] = 2.0f;
    solution.positions[0][1] = 2.0f;
    solution.positions[1][0] = 5.0f;
    solution.positions[1][1] = 5.0f;
    solution.positions[2][0] = 8.0f;
    solution.positions[2][1] = 8.0f;
    solution.success = 1;

    /* Create mock enemies */
    struct RogueEnemy enemies[3] = {0};
    for (int i = 0; i < 3; i++)
    {
        enemies[i].level = 5;
        enemies[i].alive = 1;
    }

    int result = rogue_enemy_integration_register_navmesh_handles(&solution, enemies, 3);
    assert(result == 1);

    printf("    ✓ Navmesh registration succeeds (placeholder)\n");
}

/* Test enemy placement finalization */
static void test_enemy_placement_finalization()
{
    printf("  Testing enemy placement finalization...\n");

    RogueSpawnSolution solution = {0};
    solution.position_count = 2;
    solution.positions[0][0] = 3.0f;
    solution.positions[0][1] = 4.0f;
    solution.positions[1][0] = 7.0f;
    solution.positions[1][1] = 6.0f;
    solution.success = 1;

    /* Create mock enemies */
    struct RogueEnemy enemies[2] = {0};
    enemies[0].level = 4;
    enemies[1].level = 5;

    int result = rogue_enemy_integration_finalize_enemy_placement(&solution, enemies, 2);
    assert(result == 1); /* Should return success flag from solution */

    /* Test with failed solution */
    solution.success = 0;
    result = rogue_enemy_integration_finalize_enemy_placement(&solution, enemies, 2);
    assert(result == 0);

    printf("    ✓ Enemy placement finalization working\n");
}

/* Test error handling for invalid inputs */
static void test_error_handling()
{
    printf("  Testing error handling...\n");

    RogueRoomEncounterInfo encounter_info = {0};
    RogueRoomDimensions room_dims = {0};
    RogueSpawnSolution solution = {0};

    /* NULL pointer tests */
    int result = rogue_enemy_integration_solve_spawn_positions(NULL, &room_dims, &solution);
    assert(result == 0);

    result = rogue_enemy_integration_solve_spawn_positions(&encounter_info, NULL, &solution);
    assert(result == 0);

    result = rogue_enemy_integration_solve_spawn_positions(&encounter_info, &room_dims, NULL);
    assert(result == 0);

    /* Position validation with NULL room */
    result = rogue_enemy_integration_validate_spawn_position(1.0f, 1.0f, NULL, NULL, 0);
    assert(result == 0);

    /* Navmesh registration with NULL inputs */
    result = rogue_enemy_integration_register_navmesh_handles(NULL, NULL, 0);
    assert(result == 0);

    /* Enemy placement with NULL inputs */
    result = rogue_enemy_integration_finalize_enemy_placement(NULL, NULL, 0);
    assert(result == 0);

    printf("    ✓ Error handling robust\n");
}

/* Test deterministic behavior with fixed seed */
static void test_deterministic_spawn_behavior()
{
    printf("  Testing deterministic spawn behavior...\n");

    RogueRoomEncounterInfo encounter_info = {0};
    encounter_info.room_id = 1;
    encounter_info.depth_level = 6; /* Deep enough for boss */
    encounter_info.biome_id = 1;
    encounter_info.encounter_template_id = 0;

    RogueRoomDimensions room_dims = {0};
    room_dims.min_x = 1.0f;
    room_dims.min_y = 1.0f;
    room_dims.max_x = 11.0f;
    room_dims.max_y = 11.0f;
    room_dims.obstacle_count = 0;

    RogueSpawnSolution solution1 = {0};
    RogueSpawnSolution solution2 = {0};

    /* Set same random seed */
    srand(42);
    int result1 =
        rogue_enemy_integration_solve_spawn_positions(&encounter_info, &room_dims, &solution1);

    srand(42);
    int result2 =
        rogue_enemy_integration_solve_spawn_positions(&encounter_info, &room_dims, &solution2);

    assert(result1 == result2);
    assert(solution1.position_count == solution2.position_count);

    /* Boss position should be identical (deterministic center calculation) */
    assert(fabs(solution1.positions[0][0] - solution2.positions[0][0]) < 0.001f);
    assert(fabs(solution1.positions[0][1] - solution2.positions[0][1]) < 0.001f);

    printf("    ✓ Deterministic spawn behavior verified\n");
}

int main(void)
{
    printf("Running Enemy Integration Phase 5 Tests (Spatial Spawn & Navigation)...\n");

    test_spawn_position_solver_basic();
    test_spawn_position_solver_boss();
    test_spawn_position_solver_obstacles();
    test_spawn_position_validation();
    test_spawn_minimum_distance();
    test_navmesh_registration();
    test_enemy_placement_finalization();
    test_error_handling();
    test_deterministic_spawn_behavior();

    printf("All Phase 5 tests passed! ✓\n");
    return 0;
}
