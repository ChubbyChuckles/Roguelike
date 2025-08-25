# Skills-related unit tests

# Phase 1A haste snapshot & drift correction test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_skills_phase1a_haste_snapshot_and_drift.c AND NOT TARGET test_skills_phase1a_haste_snapshot_and_drift)
    add_executable(test_skills_phase1a_haste_snapshot_and_drift unit/test_skills_phase1a_haste_snapshot_and_drift.c)
    target_link_libraries(test_skills_phase1a_haste_snapshot_and_drift PRIVATE rogue_core)
    target_compile_definitions(test_skills_phase1a_haste_snapshot_and_drift PRIVATE SDL_MAIN_HANDLED=1)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_skills_phase1a_haste_snapshot_and_drift PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_skills_phase1a_haste_snapshot_and_drift COMMAND test_skills_phase1a_haste_snapshot_and_drift)
endif()

# Skills Phase 2 costs & refunds test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_skills_phase2_costs_refunds.c AND NOT TARGET test_skills_phase2_costs_refunds)
    add_executable(test_skills_phase2_costs_refunds unit/test_skills_phase2_costs_refunds.c)
    target_link_libraries(test_skills_phase2_costs_refunds PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_skills_phase2_costs_refunds PRIVATE ROGUE_HAVE_SDL=1 SDL_MAIN_HANDLED=1)
    else()
        target_compile_definitions(test_skills_phase2_costs_refunds PRIVATE SDL_MAIN_HANDLED=1)
    endif()
    add_test(NAME test_skills_phase2_costs_refunds COMMAND test_skills_phase2_costs_refunds)
endif()

# Skills Phase 2.4/2.5 Overdrive + Heat test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_skills_phase2_overdrive_heat.c AND NOT TARGET test_skills_phase2_overdrive_heat)
    add_executable(test_skills_phase2_overdrive_heat unit/test_skills_phase2_overdrive_heat.c)
    target_link_libraries(test_skills_phase2_overdrive_heat PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_skills_phase2_overdrive_heat PRIVATE ROGUE_HAVE_SDL=1 SDL_MAIN_HANDLED=1)
    else()
        target_compile_definitions(test_skills_phase2_overdrive_heat PRIVATE SDL_MAIN_HANDLED=1)
    endif()
    add_test(NAME test_skills_phase2_overdrive_heat COMMAND test_skills_phase2_overdrive_heat)
endif()
