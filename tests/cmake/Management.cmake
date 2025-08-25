# Management Theme - System Management Tests
# Core system management including transactions, rollbacks, snapshots, validation, and deadlock prevention

# Phase 1: Core Transaction Management
# test_transaction_manager - Core transaction system with commit/rollback capabilities
add_executable(test_transaction_manager tests/unit/test_transaction_manager.c)
target_link_libraries(test_transaction_manager rogue_core)

# Phase 2: State Management
# test_snapshot_manager - State snapshot system for save states and rollback points
add_executable(test_snapshot_manager tests/unit/test_snapshot_manager.c)
target_link_libraries(test_snapshot_manager rogue_core)

# test_rollback_manager - Rollback system for state restoration
add_executable(test_rollback_manager tests/unit/test_rollback_manager.c)
target_link_libraries(test_rollback_manager rogue_core)

# Phase 3: Validation and Safety
# test_state_validation_manager - State validation and integrity checking
add_executable(test_state_validation_manager tests/unit/test_state_validation_manager.c)
target_link_libraries(test_state_validation_manager rogue_core)

# test_deadlock_manager - Deadlock detection and prevention system
add_executable(test_deadlock_manager tests/unit/test_deadlock_manager.c)
target_link_libraries(test_deadlock_manager rogue_core)
