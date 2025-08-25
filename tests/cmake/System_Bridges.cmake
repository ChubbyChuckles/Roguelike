# Test System Bridges

# Integration Plumbing Phase 3.1 Enemy-AI Bridge Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_phase3_1_enemy_ai_bridge.c)
    add_executable(test_phase3_1_enemy_ai_bridge unit/test_phase3_1_enemy_ai_bridge.c)
    target_link_libraries(test_phase3_1_enemy_ai_bridge PRIVATE rogue_core)
    add_test(NAME test_phase3_1_enemy_ai_bridge COMMAND test_phase3_1_enemy_ai_bridge)
endif()

# Integration Plumbing Phase 3.2 Combat-Equipment Bridge Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_phase3_2_combat_equip_bridge.c)
    add_executable(test_phase3_2_combat_equip_bridge unit/test_phase3_2_combat_equip_bridge.c)
    target_link_libraries(test_phase3_2_combat_equip_bridge PRIVATE rogue_core)
    add_test(NAME test_phase3_2_combat_equip_bridge COMMAND test_phase3_2_combat_equip_bridge)
endif()

# Integration Plumbing Phase 0 Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_integration_manager.c)
    add_executable(test_integration_manager unit/test_integration_manager.c)
    target_link_libraries(test_integration_manager PRIVATE rogue_core)
    add_test(NAME test_integration_manager COMMAND test_integration_manager)
endif()

# Integration Plumbing Phase 3.3 Combat-Progression Bridge Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_phase3_3_combat_progression_bridge.c)
    add_executable(test_phase3_3_combat_progression_bridge unit/test_phase3_3_combat_progression_bridge.c)
    target_link_libraries(test_phase3_3_combat_progression_bridge PRIVATE rogue_core)
    add_test(NAME test_phase3_3_combat_progression_bridge COMMAND test_phase3_3_combat_progression_bridge)
endif()

# Integration Plumbing Phase 3.7 WorldGen-Enemy Bridge Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/test_phase3_7_worldgen_enemy_bridge.c)
    add_executable(test_phase3_7_worldgen_enemy_bridge test_phase3_7_worldgen_enemy_bridge.c)
    target_link_libraries(test_phase3_7_worldgen_enemy_bridge PRIVATE rogue_core)
    add_test(NAME test_phase3_7_worldgen_enemy_bridge COMMAND test_phase3_7_worldgen_enemy_bridge)
endif()

# Integration Plumbing Phase 3.8 World Generation ↔ Resource/Gathering Bridge Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_phase3_8_worldgen_resource_bridge.c)
    add_executable(test_phase3_8_worldgen_resource_bridge unit/test_phase3_8_worldgen_resource_bridge.c)
    target_link_libraries(test_phase3_8_worldgen_resource_bridge PRIVATE rogue_core)
    add_test(NAME test_phase3_8_worldgen_resource_bridge COMMAND test_phase3_8_worldgen_resource_bridge)
endif()

# Integration Plumbing Phase 3.9 UI Integration Bridge Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_phase3_9_ui_integration_bridge.c)
    add_executable(test_phase3_9_ui_integration_bridge unit/test_phase3_9_ui_integration_bridge.c)
    target_link_libraries(test_phase3_9_ui_integration_bridge PRIVATE rogue_core)
    add_test(NAME test_phase3_9_ui_integration_bridge COMMAND test_phase3_9_ui_integration_bridge)
endif()

# Integration Plumbing Phase 3.10 Persistence Integration Bridge Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_phase3_10_persistence_integration_bridge.c)
    add_executable(test_phase3_10_persistence_integration_bridge unit/test_phase3_10_persistence_integration_bridge.c)
    target_link_libraries(test_phase3_10_persistence_integration_bridge PRIVATE rogue_core)
    add_test(NAME test_phase3_10_persistence_integration_bridge COMMAND test_phase3_10_persistence_integration_bridge)
endif()

# Integration Plumbing Phase 4.8 Shared Data Structure Validation aggregate tests
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_integration_phase4_8_shared_validation.c AND NOT TARGET test_integration_phase4_8_shared_validation)
    add_executable(test_integration_phase4_8_shared_validation unit/test_integration_phase4_8_shared_validation.c)
    target_link_libraries(test_integration_phase4_8_shared_validation PRIVATE rogue_core)
    add_test(NAME test_integration_phase4_8_shared_validation COMMAND test_integration_phase4_8_shared_validation)
endif()

# Integration Plumbing Phase 3.2 Simple Test (for debugging)
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_phase3_2_simple.c)
    add_executable(test_phase3_2_simple unit/test_phase3_2_simple.c)
    target_link_libraries(test_phase3_2_simple PRIVATE rogue_core)
    add_test(NAME test_phase3_2_simple COMMAND test_phase3_2_simple)
endif()

# Integration Plumbing Phase 3.3 Simple Test (for debugging)
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_phase3_3_simple.c)
    add_executable(test_phase3_3_simple unit/test_phase3_3_simple.c)
    target_link_libraries(test_phase3_3_simple PRIVATE rogue_core)
    add_test(NAME test_phase3_3_simple COMMAND test_phase3_3_simple)
endif()

# Integration Plumbing Phase 2.3.4 Biome asset dependency validation tests
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_biome_assets_validate.c AND NOT TARGET test_biome_assets_validate)
    add_executable(test_biome_assets_validate unit/test_biome_assets_validate.c)
    target_link_libraries(test_biome_assets_validate PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_link_libraries(test_biome_assets_validate PRIVATE SDL2::SDL2)
        target_compile_definitions(test_biome_assets_validate PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_biome_assets_validate COMMAND test_biome_assets_validate)
endif()
