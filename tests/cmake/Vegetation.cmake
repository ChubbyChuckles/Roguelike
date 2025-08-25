# Vegetation Test Suite

# Integration Plumbing Phase 2.3.4 Vegetation JSON ingestion tests
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_vegetation_json.c AND NOT TARGET test_vegetation_json)
    add_executable(test_vegetation_json unit/test_vegetation_json.c)
    target_link_libraries(test_vegetation_json PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_link_libraries(test_vegetation_json PRIVATE SDL2::SDL2)
        target_compile_definitions(test_vegetation_json PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_vegetation_json COMMAND test_vegetation_json)
endif()

# Explicitly add vegetation collision test (ensure included even if glob cache stale)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_vegetation_collision.c AND NOT TARGET test_vegetation_collision)
    add_executable(test_vegetation_collision unit/test_vegetation_collision.c)
    target_link_libraries(test_vegetation_collision PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_vegetation_collision PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_vegetation_collision COMMAND test_vegetation_collision)
endif()

# Explicitly add vegetation canopy block test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_vegetation_canopy_block.c AND NOT TARGET test_vegetation_canopy_block)
    add_executable(test_vegetation_canopy_block unit/test_vegetation_canopy_block.c)
    target_link_libraries(test_vegetation_canopy_block PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_vegetation_canopy_block PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_vegetation_canopy_block COMMAND test_vegetation_canopy_block)
endif()

# Explicitly add vegetation trunk collision behavioural test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_vegetation_trunk_collision.c AND NOT TARGET test_vegetation_trunk_collision)
    add_executable(test_vegetation_trunk_collision unit/test_vegetation_trunk_collision.c)
    target_link_libraries(test_vegetation_trunk_collision PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_compile_definitions(test_vegetation_trunk_collision PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_vegetation_trunk_collision COMMAND test_vegetation_trunk_collision)
endif()
