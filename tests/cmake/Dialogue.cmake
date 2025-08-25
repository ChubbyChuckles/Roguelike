# tests/cmake/Dialogue.cmake
if(NOT ROGUE_DIALOGUE_CMAKE_GUARD)
    set(ROGUE_DIALOGUE_CMAKE_GUARD ON)

# Dialogue system tests

# Dialogue JSON test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dialogue_json.c AND NOT TARGET test_dialogue_json)
    add_executable(test_dialogue_json unit/test_dialogue_json.c)
    target_link_libraries(test_dialogue_json PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_dialogue_json PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_dialogue_json COMMAND test_dialogue_json)
endif()

# Dialogue Phase 0 test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dialogue_phase0.c AND NOT TARGET test_dialogue_phase0)
    add_executable(test_dialogue_phase0 unit/test_dialogue_phase0.c)
    target_link_libraries(test_dialogue_phase0 PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_dialogue_phase0 PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_dialogue_phase0 COMMAND test_dialogue_phase0)
endif()

# Dialogue Phase 1 test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dialogue_phase1.c AND NOT TARGET test_dialogue_phase1)
    add_executable(test_dialogue_phase1 unit/test_dialogue_phase1.c)
    target_link_libraries(test_dialogue_phase1 PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_dialogue_phase1 PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_dialogue_phase1 COMMAND test_dialogue_phase1)
endif()

# Dialogue Phase 2 tokens test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dialogue_phase2_tokens.c AND NOT TARGET test_dialogue_phase2_tokens)
    add_executable(test_dialogue_phase2_tokens unit/test_dialogue_phase2_tokens.c)
    target_link_libraries(test_dialogue_phase2_tokens PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_dialogue_phase2_tokens PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_dialogue_phase2_tokens COMMAND test_dialogue_phase2_tokens)
endif()

# Dialogue Phase 3 effects test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dialogue_phase3_effects.c AND NOT TARGET test_dialogue_phase3_effects)
    add_executable(test_dialogue_phase3_effects unit/test_dialogue_phase3_effects.c)
    target_link_libraries(test_dialogue_phase3_effects PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_dialogue_phase3_effects PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_dialogue_phase3_effects COMMAND test_dialogue_phase3_effects)
endif()

# Dialogue Phase 4 persistence test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dialogue_phase4_persistence.c AND NOT TARGET test_dialogue_phase4_persistence)
    add_executable(test_dialogue_phase4_persistence unit/test_dialogue_phase4_persistence.c)
    target_link_libraries(test_dialogue_phase4_persistence PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_dialogue_phase4_persistence PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_dialogue_phase4_persistence COMMAND test_dialogue_phase4_persistence)
endif()

# Dialogue Phase 5 localization test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dialogue_phase5_localization.c AND NOT TARGET test_dialogue_phase5_localization)
    add_executable(test_dialogue_phase5_localization unit/test_dialogue_phase5_localization.c)
    target_link_libraries(test_dialogue_phase5_localization PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_dialogue_phase5_localization PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_dialogue_phase5_localization COMMAND test_dialogue_phase5_localization)
endif()

# Dialogue Phase 6 typewriter test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dialogue_phase6_typewriter.c AND NOT TARGET test_dialogue_phase6_typewriter)
    add_executable(test_dialogue_phase6_typewriter unit/test_dialogue_phase6_typewriter.c)
    target_link_libraries(test_dialogue_phase6_typewriter PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_dialogue_phase6_typewriter PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_dialogue_phase6_typewriter COMMAND test_dialogue_phase6_typewriter)
endif()

# Dialogue Phase 7 analytics test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dialogue_phase7_analytics.c AND NOT TARGET test_dialogue_phase7_analytics)
    add_executable(test_dialogue_phase7_analytics unit/test_dialogue_phase7_analytics.c)
    target_link_libraries(test_dialogue_phase7_analytics PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_compile_definitions(test_dialogue_phase7_analytics PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_dialogue_phase7_analytics COMMAND test_dialogue_phase7_analytics)
endif()

endif() # ROGUE_DIALOGUE_CMAKE_GUARD
