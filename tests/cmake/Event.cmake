# tests/cmake/Event.cmake
if(NOT ROGUE_EVENT_CMAKE_GUARD)
    set(ROGUE_EVENT_CMAKE_GUARD ON)

# Event system tests

# Integration Plumbing Phase 1.1-1.7 Event Bus test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_event_bus.c)
    add_executable(test_event_bus unit/test_event_bus.c)
    target_link_libraries(test_event_bus PRIVATE rogue_core)
    add_test(NAME test_event_bus COMMAND test_event_bus)
endif()

# Combat events test (Phase 1A.5 partial)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_combat_events.c AND NOT TARGET test_combat_events)
    add_executable(test_combat_events unit/test_combat_events.c)
    target_link_libraries(test_combat_events PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_combat_events PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_combat_events COMMAND test_combat_events)
endif()

# Combat Phase 7.7 per-component damage event emission test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_combat_phase7_damage_event_components.c AND NOT TARGET test_combat_phase7_damage_event_components)
    add_executable(test_combat_phase7_damage_event_components unit/test_combat_phase7_damage_event_components.c)
    target_link_libraries(test_combat_phase7_damage_event_components PRIVATE rogue_core)
    target_compile_definitions(test_combat_phase7_damage_event_components PRIVATE TEST_COMBAT_PERMISSIVE=1)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_combat_phase7_damage_event_components PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_combat_phase7_damage_event_components COMMAND test_combat_phase7_damage_event_components)
endif()

# Combat Phase 7.6 infusion split events test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_combat_phase7_infusion_split_events.c AND NOT TARGET test_combat_phase7_infusion_split_events)
    add_executable(test_combat_phase7_infusion_split_events unit/test_combat_phase7_infusion_split_events.c)
    target_link_libraries(test_combat_phase7_infusion_split_events PRIVATE rogue_core)
    target_compile_definitions(test_combat_phase7_infusion_split_events PRIVATE TEST_COMBAT_PERMISSIVE=1)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_combat_phase7_infusion_split_events PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_combat_phase7_infusion_split_events COMMAND test_combat_phase7_infusion_split_events)
endif()

# Determinism damage events test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_determinism_damage_events.c AND NOT TARGET test_determinism_damage_events)
    add_executable(test_determinism_damage_events unit/test_determinism_damage_events.c)
    target_link_libraries(test_determinism_damage_events PRIVATE rogue_core)
    target_compile_definitions(test_determinism_damage_events PRIVATE SDL_MAIN_HANDLED=1)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_determinism_damage_events PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_determinism_damage_events COMMAND test_determinism_damage_events)
endif()

# Rollback events test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_rollback_events.c AND NOT TARGET test_rollback_events)
    add_executable(test_rollback_events unit/test_rollback_events.c)
    target_link_libraries(test_rollback_events PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_link_libraries(test_rollback_events PRIVATE SDL2::SDL2)
        target_compile_definitions(test_rollback_events PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_rollback_events COMMAND test_rollback_events)
endif()

endif() # ROGUE_EVENT_CMAKE_GUARD
