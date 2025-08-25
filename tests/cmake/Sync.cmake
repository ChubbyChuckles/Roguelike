# tests/cmake/Sync.cmake
if(NOT ROGUE_SYNC_CMAKE_GUARD)
    set(ROGUE_SYNC_CMAKE_GUARD ON)

# Integration Plumbing Phase 5.8 Sync system tests

# Sync Phase 5.8 Auto rollback test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_sync_phase5_8_auto_rollback.c AND NOT TARGET test_sync_phase5_8_auto_rollback)
    add_executable(test_sync_phase5_8_auto_rollback unit/test_sync_phase5_8_auto_rollback.c)
    target_link_libraries(test_sync_phase5_8_auto_rollback PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_link_libraries(test_sync_phase5_8_auto_rollback PRIVATE SDL2::SDL2)
        target_compile_definitions(test_sync_phase5_8_auto_rollback PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_sync_phase5_8_auto_rollback COMMAND test_sync_phase5_8_auto_rollback)
endif()

# Sync Phase 5.8 Snapshot test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_sync_phase5_8_snapshot.c AND NOT TARGET test_sync_phase5_8_snapshot)
    add_executable(test_sync_phase5_8_snapshot unit/test_sync_phase5_8_snapshot.c)
    target_link_libraries(test_sync_phase5_8_snapshot PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_link_libraries(test_sync_phase5_8_snapshot PRIVATE SDL2::SDL2)
        target_compile_definitions(test_sync_phase5_8_snapshot PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_sync_phase5_8_snapshot COMMAND test_sync_phase5_8_snapshot)
endif()

# Sync Phase 5.8 Delta test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_sync_phase5_8_delta.c AND NOT TARGET test_sync_phase5_8_delta)
    add_executable(test_sync_phase5_8_delta unit/test_sync_phase5_8_delta.c)
    target_link_libraries(test_sync_phase5_8_delta PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_link_libraries(test_sync_phase5_8_delta PRIVATE SDL2::SDL2)
        target_compile_definitions(test_sync_phase5_8_delta PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_sync_phase5_8_delta COMMAND test_sync_phase5_8_delta)
endif()

# Sync Phase 5.8 Transaction test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_sync_phase5_8_tx.c AND NOT TARGET test_sync_phase5_8_tx)
    add_executable(test_sync_phase5_8_tx unit/test_sync_phase5_8_tx.c)
    target_link_libraries(test_sync_phase5_8_tx PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_link_libraries(test_sync_phase5_8_tx PRIVATE SDL2::SDL2)
        target_compile_definitions(test_sync_phase5_8_tx PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_sync_phase5_8_tx COMMAND test_sync_phase5_8_tx)
endif()

# Sync Phase 5.8 Rollback test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_sync_phase5_8_rollback.c AND NOT TARGET test_sync_phase5_8_rollback)
    add_executable(test_sync_phase5_8_rollback unit/test_sync_phase5_8_rollback.c)
    target_link_libraries(test_sync_phase5_8_rollback PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_link_libraries(test_sync_phase5_8_rollback PRIVATE SDL2::SDL2)
        target_compile_definitions(test_sync_phase5_8_rollback PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_sync_phase5_8_rollback COMMAND test_sync_phase5_8_rollback)
endif()

# Sync Phase 5.8 Integration test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/unit/test_sync_phase5_8_integration.c AND NOT TARGET test_sync_phase5_8_integration)
    add_executable(test_sync_phase5_8_integration unit/test_sync_phase5_8_integration.c)
    target_link_libraries(test_sync_phase5_8_integration PRIVATE rogue_core)
    if(ROGUE_ENABLE_SDL)
        target_link_libraries(test_sync_phase5_8_integration PRIVATE SDL2::SDL2)
        target_compile_definitions(test_sync_phase5_8_integration PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_sync_phase5_8_integration COMMAND test_sync_phase5_8_integration)
endif()

endif() # ROGUE_SYNC_CMAKE_GUARD
