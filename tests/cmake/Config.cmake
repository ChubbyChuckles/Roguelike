# Guard against multiple includes
if(CONFIG_THEME_INCLUDED)
    return()
endif()
set(CONFIG_THEME_INCLUDED TRUE)

# ========================================
# Config Theme - Configuration System Tests
# ========================================
# This file contains all configuration-related tests including
# parsing, migration, version management, and validation.

# Integration Plumbing Phase 2.2 CFG Parser Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../unit/test_cfg_parser.c)
    add_executable(test_cfg_parser unit/test_cfg_parser.c)
    target_link_libraries(test_cfg_parser PRIVATE rogue_core)
    add_test(NAME test_cfg_parser COMMAND test_cfg_parser)
endif()

# Integration Plumbing Phase 2.3 CFG Migration Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../unit/test_cfg_migration.c)
    add_executable(test_cfg_migration unit/test_cfg_migration.c)
    target_link_libraries(test_cfg_migration PRIVATE rogue_core)
    add_test(NAME test_cfg_migration COMMAND test_cfg_migration)
endif()

# Integration Plumbing Phase 2.6 Configuration Version Management Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../unit/test_config_version.c)
    add_executable(test_config_version unit/test_config_version.c)
    target_link_libraries(test_config_version PRIVATE rogue_core)
    add_test(NAME test_config_version COMMAND test_config_version)
endif()

# Integration Plumbing Phase 2.8 Configuration System Validation Test
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../unit/test_config_system_validation.c)
    add_executable(test_config_system_validation unit/test_config_system_validation.c)
    target_link_libraries(test_config_system_validation PRIVATE rogue_core)
    add_test(NAME test_config_system_validation COMMAND test_config_system_validation)
endif()

# Integration Plumbing Phase 2.2 CFG File Analysis Test (manual run)
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../unit/test_cfg_file_analysis.c)
    add_executable(test_cfg_file_analysis unit/test_cfg_file_analysis.c)
    target_link_libraries(test_cfg_file_analysis PRIVATE rogue_core)
endif()
