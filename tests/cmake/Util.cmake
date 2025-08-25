# Util Test Suite

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_stat_cache.c AND NOT TARGET test_stat_cache)
    add_executable(test_stat_cache unit/test_stat_cache.c)
    target_link_libraries(test_stat_cache PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_stat_cache PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_stat_cache COMMAND test_stat_cache)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_repair_costs.c AND NOT TARGET test_repair_costs)
    add_executable(test_repair_costs unit/test_repair_costs.c)
    target_link_libraries(test_repair_costs PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_repair_costs PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_repair_costs COMMAND test_repair_costs)
endif()

# Integration Plumbing Phase 4.1 Unified Entity ID System Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_entity_id.c AND NOT TARGET test_entity_id)
    add_executable(test_entity_id unit/test_entity_id.c)
    target_link_libraries(test_entity_id PRIVATE rogue_core)
    add_test(NAME test_entity_id COMMAND test_entity_id)
endif()

# Integration Plumbing Phase 4.2 Shared Memory Pool System Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_memory_pool.c AND NOT TARGET test_memory_pool)
    add_executable(test_memory_pool unit/test_memory_pool.c)
    target_link_libraries(test_memory_pool PRIVATE rogue_core)
    add_test(NAME test_memory_pool COMMAND test_memory_pool)
endif()

# Integration Plumbing Phase 4.3 Cache Management & Invalidation Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_cache_system.c AND NOT TARGET test_cache_system)
    add_executable(test_cache_system unit/test_cache_system.c)
    target_link_libraries(test_cache_system PRIVATE rogue_core)
    add_test(NAME test_cache_system COMMAND test_cache_system)

    # Phase 4.4 reference counting & lifecycle management tests
    add_executable(test_ref_count unit/test_ref_count.c)
    target_include_directories(test_ref_count PRIVATE ${CMAKE_SOURCE_DIR}/src/core/integration)
    target_link_libraries(test_ref_count PRIVATE rogue_core)
    add_test(NAME test_ref_count COMMAND test_ref_count)
    # Phase 4.5 copy-on-write tests
    add_executable(test_cow unit/test_cow.c)
    target_include_directories(test_cow PRIVATE ${CMAKE_SOURCE_DIR}/src/core/integration)
    target_link_libraries(test_cow PRIVATE rogue_core)
    add_test(NAME test_cow COMMAND test_cow)
    # Phase 4.6 data structure versioning & migration tests
    add_executable(test_versioning unit/test_versioning.c)
    target_include_directories(test_versioning PRIVATE ${CMAKE_SOURCE_DIR}/src/core/integration)
    target_link_libraries(test_versioning PRIVATE rogue_core)
    add_test(NAME test_versioning COMMAND test_versioning)
endif()

# Integration Plumbing Phase 0.1 System Taxonomy Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_system_taxonomy.c)
    add_executable(test_system_taxonomy unit/test_system_taxonomy.c)
    target_link_libraries(test_system_taxonomy PRIVATE rogue_core)
    add_test(NAME test_system_taxonomy COMMAND test_system_taxonomy)
endif()

# Integration Plumbing Phase 2.1 JSON Schema Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_json_schema.c)
    add_executable(test_json_schema unit/test_json_schema.c)
    target_link_libraries(test_json_schema PRIVATE rogue_core)
    add_test(NAME test_json_schema COMMAND test_json_schema)
endif()

# Integration Plumbing Phase 2.4 Hot Reload System Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_hot_reload.c)
    add_executable(test_hot_reload unit/test_hot_reload.c)
    target_link_libraries(test_hot_reload PRIVATE rogue_core)
    add_test(NAME test_hot_reload COMMAND test_hot_reload)
endif()

# Integration Plumbing Phase 2.5 Dependency Management System Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_dependency_manager.c)
    add_executable(test_dependency_manager unit/test_dependency_manager.c)
    target_link_libraries(test_dependency_manager PRIVATE rogue_core)
    add_test(NAME test_dependency_manager COMMAND test_dependency_manager)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_reroll_affix.c AND NOT TARGET test_reroll_affix)
    add_executable(test_reroll_affix unit/test_reroll_affix.c)
    target_link_libraries(test_reroll_affix PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_reroll_affix PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_reroll_affix COMMAND test_reroll_affix)
endif()

# Explicitly add new maintainability phase M3.4 asset dependency graph test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_asset_dep.c AND NOT TARGET test_asset_dep)
    add_executable(test_asset_dep unit/test_asset_dep.c)
    target_link_libraries(test_asset_dep PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_asset_dep PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_asset_dep COMMAND test_asset_dep)
endif()

# Explicitly add mob collision test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_mob_collision.c AND NOT TARGET test_mob_collision)
    add_executable(test_mob_collision unit/test_mob_collision.c)
    target_link_libraries(test_mob_collision PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_mob_collision PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_mob_collision COMMAND test_mob_collision)
endif()

# Explicitly add navigation test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_navigation.c AND NOT TARGET test_navigation)
    add_executable(test_navigation unit/test_navigation.c)
    target_link_libraries(test_navigation PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_navigation PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_navigation COMMAND test_navigation)
endif()

# Explicitly add A* pathfinding test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_astar.c AND NOT TARGET test_astar)
    add_executable(test_astar unit/test_astar.c)
    target_link_libraries(test_astar PRIVATE rogue_core)
    # Prevent SDL from redefining main to SDL_main when SDL headers transitively included
    target_compile_definitions(test_astar PRIVATE SDL_MAIN_HANDLED=1)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_astar PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_astar COMMAND test_astar)
endif()

# Phase M4.4 fuzz parsers (affix/persistence/kv)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_fuzz_parsers.c AND NOT TARGET test_fuzz_parsers)
    add_executable(test_fuzz_parsers unit/test_fuzz_parsers.c)
    target_link_libraries(test_fuzz_parsers PRIVATE rogue_core)
    target_compile_definitions(test_fuzz_parsers PRIVATE SDL_MAIN_HANDLED=1)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_fuzz_parsers PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_fuzz_parsers COMMAND test_fuzz_parsers)
endif()

# Explicitly add input buffering cast test (Phase 1A.1 partial)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_input_buffer_cast.c AND NOT TARGET test_input_buffer_cast)
    add_executable(test_input_buffer_cast unit/test_input_buffer_cast.c)
    target_link_libraries(test_input_buffer_cast PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_input_buffer_cast PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_input_buffer_cast COMMAND test_input_buffer_cast)
endif()
