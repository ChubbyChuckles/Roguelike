# WorldGen Tests

# Integration Plumbing Phase 2.3.4 Biomes JSON loader/validation tests
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_worldgen_phase3_4_biome_json.c AND NOT TARGET test_worldgen_phase3_4_biome_json)
    add_executable(test_worldgen_phase3_4_biome_json unit/test_worldgen_phase3_4_biome_json.c)
    target_link_libraries(test_worldgen_phase3_4_biome_json PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_link_libraries(test_worldgen_phase3_4_biome_json PRIVATE SDL2::SDL2)
        target_compile_definitions(test_worldgen_phase3_4_biome_json PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase3_4_biome_json COMMAND test_worldgen_phase3_4_biome_json)
endif()

# World Generation Phase 1 foundation test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase1_foundation.c AND NOT TARGET test_worldgen_phase1_foundation)
    add_executable(test_worldgen_phase1_foundation unit/test_worldgen_phase1_foundation.c)
    target_link_libraries(test_worldgen_phase1_foundation PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase1_foundation PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase1_foundation COMMAND test_worldgen_phase1_foundation)
endif()

# World Generation Phase 2 macro layout test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase2_macro.c AND NOT TARGET test_worldgen_phase2_macro)
    add_executable(test_worldgen_phase2_macro unit/test_worldgen_phase2_macro.c)
    target_link_libraries(test_worldgen_phase2_macro PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase2_macro PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase2_macro COMMAND test_worldgen_phase2_macro)
endif()

# World Generation Phase 3 biome descriptor & registry test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase3_biome_descriptors.c AND NOT TARGET test_worldgen_phase3_biome_descriptors)
    add_executable(test_worldgen_phase3_biome_descriptors unit/test_worldgen_phase3_biome_descriptors.c)
    target_link_libraries(test_worldgen_phase3_biome_descriptors PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase3_biome_descriptors PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase3_biome_descriptors COMMAND test_worldgen_phase3_biome_descriptors)
endif()

# World Generation Phase 4 local terrain & caves test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase4_local_caves.c AND NOT TARGET test_worldgen_phase4_local_caves)
    add_executable(test_worldgen_phase4_local_caves unit/test_worldgen_phase4_local_caves.c)
    target_link_libraries(test_worldgen_phase4_local_caves PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase4_local_caves PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase4_local_caves COMMAND test_worldgen_phase4_local_caves)
endif()

# World Generation Phase 5 rivers & erosion detailing test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase5_rivers_erosion.c AND NOT TARGET test_worldgen_phase5_rivers_erosion)
    add_executable(test_worldgen_phase5_rivers_erosion unit/test_worldgen_phase5_rivers_erosion.c)
    target_link_libraries(test_worldgen_phase5_rivers_erosion PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase5_rivers_erosion PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase5_rivers_erosion COMMAND test_worldgen_phase5_rivers_erosion)
endif()

# World Generation Phase 6 structures & POIs test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase6_structures.c AND NOT TARGET test_worldgen_phase6_structures)
    add_executable(test_worldgen_phase6_structures unit/test_worldgen_phase6_structures.c)
    target_link_libraries(test_worldgen_phase6_structures PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase6_structures PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase6_structures COMMAND test_worldgen_phase6_structures)
endif()

# World Generation Phase 7 dungeon generator test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase7_dungeon.c AND NOT TARGET test_worldgen_phase7_dungeon)
    add_executable(test_worldgen_phase7_dungeon unit/test_worldgen_phase7_dungeon.c)
    target_link_libraries(test_worldgen_phase7_dungeon PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase7_dungeon PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase7_dungeon COMMAND test_worldgen_phase7_dungeon)
endif()

# World Generation Phase 8 fauna & spawn ecology test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase8_spawns.c AND NOT TARGET test_worldgen_phase8_spawns)
    add_executable(test_worldgen_phase8_spawns unit/test_worldgen_phase8_spawns.c)
    target_link_libraries(test_worldgen_phase8_spawns PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase8_spawns PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase8_spawns COMMAND test_worldgen_phase8_spawns)
endif()

# World Generation Phase 9 resource nodes test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase9_resources.c AND NOT TARGET test_worldgen_phase9_resources)
    add_executable(test_worldgen_phase9_resources unit/test_worldgen_phase9_resources.c)
    target_link_libraries(test_worldgen_phase9_resources PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase9_resources PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase9_resources COMMAND test_worldgen_phase9_resources)
endif()

# World Generation Phase 10 weather simulation test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase10_weather.c AND NOT TARGET test_worldgen_phase10_weather)
    add_executable(test_worldgen_phase10_weather unit/test_worldgen_phase10_weather.c)
    target_link_libraries(test_worldgen_phase10_weather PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase10_weather PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase10_weather COMMAND test_worldgen_phase10_weather)
endif()

# World Generation Phase 11 streaming & caching test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase11_streaming.c AND NOT TARGET test_worldgen_phase11_streaming)
    add_executable(test_worldgen_phase11_streaming unit/test_worldgen_phase11_streaming.c)
    target_link_libraries(test_worldgen_phase11_streaming PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase11_streaming PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase11_streaming COMMAND test_worldgen_phase11_streaming)
endif()

# World Generation Phase 12 telemetry & analytics test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase12_telemetry.c AND NOT TARGET test_worldgen_phase12_telemetry)
    add_executable(test_worldgen_phase12_telemetry unit/test_worldgen_phase12_telemetry.c)
    target_link_libraries(test_worldgen_phase12_telemetry PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase12_telemetry PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase12_telemetry COMMAND test_worldgen_phase12_telemetry)
endif()

# World Generation Phase 13 modding & data extensibility test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase13_modding.c AND NOT TARGET test_worldgen_phase13_modding)
    add_executable(test_worldgen_phase13_modding unit/test_worldgen_phase13_modding.c)
    target_link_libraries(test_worldgen_phase13_modding PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase13_modding PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase13_modding COMMAND test_worldgen_phase13_modding)
endif()

# World Generation Phase 14 optimization & memory test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_worldgen_phase14_optimization.c AND NOT TARGET test_worldgen_phase14_optimization)
    add_executable(test_worldgen_phase14_optimization unit/test_worldgen_phase14_optimization.c)
    target_link_libraries(test_worldgen_phase14_optimization PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_worldgen_phase14_optimization PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_worldgen_phase14_optimization COMMAND test_worldgen_phase14_optimization)
endif()
