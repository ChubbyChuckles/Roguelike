# Crafting Theme - Advanced Crafting System Tests
# Advanced crafting features including UI, automation, economy, and analytics

# Phase 8: User Interface Integration
# test_crafting_phase8_ui - Crafting UI system and player interactions
add_executable(test_crafting_phase8_ui tests/unit/test_crafting_phase8_ui.c)
target_link_libraries(test_crafting_phase8_ui rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_crafting_phase8_ui SDL2::SDL2)
    target_compile_definitions(test_crafting_phase8_ui PRIVATE ROGUE_HAVE_SDL=1)
endif()

# Phase 9: Automation Systems
# test_crafting_phase9_automation - Crafting automation and batch processing
add_executable(test_crafting_phase9_automation tests/unit/test_crafting_phase9_automation.c)
target_link_libraries(test_crafting_phase9_automation rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_crafting_phase9_automation SDL2::SDL2)
    target_compile_definitions(test_crafting_phase9_automation PRIVATE ROGUE_HAVE_SDL=1)
endif()

# Phase 10: Economic Integration
# test_crafting_phase10_economy - Economic impact and resource flow in crafting
add_executable(test_crafting_phase10_economy tests/unit/test_crafting_phase10_economy.c)
target_link_libraries(test_crafting_phase10_economy rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_crafting_phase10_economy SDL2::SDL2)
    target_compile_definitions(test_crafting_phase10_economy PRIVATE ROGUE_HAVE_SDL=1)
endif()

# Phase 11: Analytics and Metrics
# test_crafting_phase11_analytics - Crafting analytics, statistics, and performance metrics
add_executable(test_crafting_phase11_analytics tests/unit/test_crafting_phase11_analytics.c)
target_link_libraries(test_crafting_phase11_analytics rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_crafting_phase11_analytics SDL2::SDL2)
    target_compile_definitions(test_crafting_phase11_analytics PRIVATE ROGUE_HAVE_SDL=1)
endif()
