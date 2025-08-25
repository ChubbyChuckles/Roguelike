# Guard against multiple includes
if(RESOURCE_THEME_INCLUDED)
    return()
endif()
set(RESOURCE_THEME_INCLUDED TRUE)

# ========================================
# Resource Theme - Resource Management Tests
# ========================================
# This file contains all resource-related tests including
# JSON loading and resource locking mechanisms.

# Resource JSON Loader Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../unit/test_resource_json.c AND NOT TARGET test_resource_json)
    add_executable(test_resource_json unit/test_resource_json.c)
    target_link_libraries(test_resource_json PRIVATE rogue_core)
    if(TARGET SDL2::SDL2)
        target_link_libraries(test_resource_json PRIVATE SDL2::SDL2)
        target_compile_definitions(test_resource_json PRIVATE ROGUE_HAVE_SDL=1)
    endif()
    add_test(NAME test_resource_json COMMAND test_resource_json)
endif()

# Resource Lock System Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../unit/test_resource_lock.c)
    add_executable(test_resource_lock unit/test_resource_lock.c)
    target_include_directories(test_resource_lock PRIVATE ${CMAKE_SOURCE_DIR}/src/core/integration)
    target_link_libraries(test_resource_lock PRIVATE rogue_core)
    add_test(NAME test_resource_lock COMMAND test_resource_lock)
endif()
