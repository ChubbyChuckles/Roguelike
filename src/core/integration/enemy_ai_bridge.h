/**
 * Enemy Integration ↔ AI System Bridge (Phase 3.1)
 *
 * Provides seamless integration between the enemy system and AI system,
 * handling lifecycle events, state synchronization, and performance coordination.
 *
 * Features:
 * - Enemy spawn event handling for AI behavior tree activation
 * - AI state synchronization with enemy integration registry
 * - Enemy death event propagation for AI system cleanup
 * - Enemy modifier application hooks for AI decision making
 * - AI behavior intensity scaling based on enemy difficulty
 * - Enemy group coordination through shared AI blackboards
 * - AI performance metrics integration with enemy analytics
 */

#ifndef ROGUE_CORE_INTEGRATION_ENEMY_AI_BRIDGE_H
#define ROGUE_CORE_INTEGRATION_ENEMY_AI_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declarations
    struct RogueEnemy;
    struct RogueBehaviorTree;
    struct RogueBlackboard;
    struct RogueEventBus;

    // Enemy AI Bridge Event Types (for event bus integration)
    typedef enum RogueEnemyAIEventType
    {
        ROGUE_ENEMY_AI_EVENT_SPAWN = 3100,            // Enemy spawned, needs AI activation
        ROGUE_ENEMY_AI_EVENT_DEATH = 3101,            // Enemy died, needs AI cleanup
        ROGUE_ENEMY_AI_EVENT_MODIFIER_APPLIED = 3102, // Modifier applied, AI needs update
        ROGUE_ENEMY_AI_EVENT_STATE_SYNC = 3103,       // AI state changed, sync to enemy
        ROGUE_ENEMY_AI_EVENT_INTENSITY_CHANGE = 3104, // AI intensity level changed
        ROGUE_ENEMY_AI_EVENT_GROUP_FORMED = 3105,     // Enemy group formed, need coordination
        ROGUE_ENEMY_AI_EVENT_PERFORMANCE_ALERT = 3106 // Performance threshold exceeded
    } RogueEnemyAIEventType;

    // AI Behavior Intensity Levels (matches enemy difficulty)
    typedef enum RogueEnemyAIIntensity
    {
        ROGUE_AI_INTENSITY_PASSIVE = 0,    // Basic patrol behavior
        ROGUE_AI_INTENSITY_NORMAL = 1,     // Standard aggro behavior
        ROGUE_AI_INTENSITY_AGGRESSIVE = 2, // Enhanced decision making
        ROGUE_AI_INTENSITY_ELITE = 3,      // Advanced coordination
        ROGUE_AI_INTENSITY_BOSS = 4        // Maximum AI capabilities
    } RogueEnemyAIIntensity;

    // Enemy Group Coordination Data
    typedef struct RogueEnemyGroup
    {
        uint32_t group_id;
        uint32_t member_ids[16]; // Enemy IDs in this group
        uint8_t member_count;
        float center_x, center_y;                  // Group center position
        struct RogueBlackboard* shared_blackboard; // Shared AI blackboard
        RogueEnemyAIIntensity group_intensity;
        uint32_t formation_pattern; // Tactical formation ID
        float last_update_time;
    } RogueEnemyGroup;

    // AI Performance Metrics (Phase 3.1.7)
    typedef struct RogueAIPerformanceMetrics
    {
        uint32_t total_ai_agents;
        uint32_t active_behavior_trees;
        uint32_t ticks_per_second;
        float average_tick_time_us;
        float peak_tick_time_us;
        uint32_t performance_warnings;
        uint32_t performance_errors;
        time_t last_metrics_reset;

        // Per-intensity metrics
        struct
        {
            uint32_t agent_count;
            float average_tick_time_us;
            uint32_t total_ticks;
        } intensity_metrics[5]; // One per RogueEnemyAIIntensity

        // Memory usage tracking
        uint32_t blackboard_memory_bytes;
        uint32_t behavior_tree_memory_bytes;
        uint32_t group_coordination_memory_bytes;
    } RogueAIPerformanceMetrics;

    // Enemy AI Bridge State
    typedef struct RogueEnemyAIBridge
    {
        bool initialized;
        struct RogueEventBus* event_bus; // For event-driven communication

        // Group coordination management
        RogueEnemyGroup groups[64]; // Support up to 64 simultaneous groups
        uint8_t active_group_count;
        uint32_t next_group_id;

        // Performance monitoring
        RogueAIPerformanceMetrics metrics;
        float metrics_update_interval; // Seconds between metric updates
        float last_metrics_update;

        // AI State synchronization tracking
        uint32_t state_sync_requests;
        uint32_t successful_syncs;
        uint32_t failed_syncs;

        // Modifier application hooks
        uint32_t modifier_hooks_registered;
        void* modifier_hook_context;

        // Performance thresholds
        float max_tick_time_warning_us; // Warning threshold
        float max_tick_time_error_us;   // Error threshold
        uint32_t max_agents_per_frame;  // Agent processing limit

        // Debug and profiling
        bool debug_logging_enabled;
        bool performance_profiling_enabled;
        char debug_log_buffer[1024];
    } RogueEnemyAIBridge;

    // === Core Bridge API ===

    /**
     * Initialize the Enemy-AI Bridge system
     * @param bridge Bridge instance to initialize
     * @param event_bus Event bus for communication
     * @return 1 on success, 0 on failure
     */
    int rogue_enemy_ai_bridge_init(RogueEnemyAIBridge* bridge, struct RogueEventBus* event_bus);

    /**
     * Shutdown the Enemy-AI Bridge system
     * @param bridge Bridge instance to shutdown
     */
    void rogue_enemy_ai_bridge_shutdown(RogueEnemyAIBridge* bridge);

    /**
     * Update the bridge system (call each frame)
     * @param bridge Bridge instance
     * @param dt Delta time in seconds
     */
    void rogue_enemy_ai_bridge_update(RogueEnemyAIBridge* bridge, float dt);

    // === Enemy Lifecycle Integration (Phase 3.1.1, 3.1.3) ===

    /**
     * Handle enemy spawn event - activate AI behavior tree
     * @param bridge Bridge instance
     * @param enemy Newly spawned enemy
     * @return 1 on successful AI activation, 0 on failure
     */
    int rogue_enemy_ai_bridge_handle_spawn(RogueEnemyAIBridge* bridge, struct RogueEnemy* enemy);

    /**
     * Handle enemy death event - cleanup AI resources
     * @param bridge Bridge instance
     * @param enemy Dying enemy
     * @return 1 on successful cleanup, 0 on failure
     */
    int rogue_enemy_ai_bridge_handle_death(RogueEnemyAIBridge* bridge, struct RogueEnemy* enemy);

    // === AI State Synchronization (Phase 3.1.2) ===

    /**
     * Synchronize AI state with enemy integration registry
     * @param bridge Bridge instance
     * @param enemy Enemy to synchronize
     * @return 1 on success, 0 on failure
     */
    int rogue_enemy_ai_bridge_sync_state(RogueEnemyAIBridge* bridge, struct RogueEnemy* enemy);

    /**
     * Request AI state synchronization for all active enemies
     * @param bridge Bridge instance
     * @return Number of enemies synchronized
     */
    uint32_t rogue_enemy_ai_bridge_sync_all_states(RogueEnemyAIBridge* bridge);

    // === Modifier Application Hooks (Phase 3.1.4) ===

    /**
     * Apply enemy modifier effects to AI decision making
     * @param bridge Bridge instance
     * @param enemy Enemy with applied modifiers
     * @param modifier_id ID of the applied modifier
     * @return 1 on success, 0 on failure
     */
    int rogue_enemy_ai_bridge_apply_modifier_hook(RogueEnemyAIBridge* bridge,
                                                  struct RogueEnemy* enemy, uint32_t modifier_id);

    // === AI Behavior Intensity Scaling (Phase 3.1.5) ===

    /**
     * Scale AI behavior intensity based on enemy difficulty
     * @param bridge Bridge instance
     * @param enemy Enemy to scale
     * @param difficulty_level Enemy difficulty level (0-100)
     * @return New AI intensity level
     */
    RogueEnemyAIIntensity rogue_enemy_ai_bridge_scale_intensity(RogueEnemyAIBridge* bridge,
                                                                struct RogueEnemy* enemy,
                                                                uint32_t difficulty_level);

    /**
     * Update AI intensity for enemy based on current conditions
     * @param bridge Bridge instance
     * @param enemy Enemy to update
     * @return 1 if intensity changed, 0 if unchanged
     */
    int rogue_enemy_ai_bridge_update_intensity(RogueEnemyAIBridge* bridge,
                                               struct RogueEnemy* enemy);

    // === Enemy Group Coordination (Phase 3.1.6) ===

    /**
     * Create enemy group with shared AI blackboard
     * @param bridge Bridge instance
     * @param enemy_ids Array of enemy IDs to group
     * @param enemy_count Number of enemies in group
     * @param formation_pattern Formation pattern ID
     * @return Group ID on success, 0 on failure
     */
    uint32_t rogue_enemy_ai_bridge_create_group(RogueEnemyAIBridge* bridge, uint32_t* enemy_ids,
                                                uint8_t enemy_count, uint32_t formation_pattern);

    /**
     * Destroy enemy group and cleanup shared resources
     * @param bridge Bridge instance
     * @param group_id Group to destroy
     * @return 1 on success, 0 on failure
     */
    int rogue_enemy_ai_bridge_destroy_group(RogueEnemyAIBridge* bridge, uint32_t group_id);

    /**
     * Update group coordination and shared blackboard
     * @param bridge Bridge instance
     * @param group_id Group to update
     * @return 1 on success, 0 on failure
     */
    int rogue_enemy_ai_bridge_update_group_coordination(RogueEnemyAIBridge* bridge,
                                                        uint32_t group_id);

    // === Performance Metrics Integration (Phase 3.1.7) ===

    /**
     * Get current AI performance metrics
     * @param bridge Bridge instance
     * @return Pointer to metrics structure
     */
    const RogueAIPerformanceMetrics*
    rogue_enemy_ai_bridge_get_metrics(const RogueEnemyAIBridge* bridge);

    /**
     * Reset performance metrics counters
     * @param bridge Bridge instance
     */
    void rogue_enemy_ai_bridge_reset_metrics(RogueEnemyAIBridge* bridge);

    /**
     * Check if performance thresholds are exceeded
     * @param bridge Bridge instance
     * @return 1 if thresholds exceeded, 0 if within limits
     */
    int rogue_enemy_ai_bridge_check_performance_thresholds(const RogueEnemyAIBridge* bridge);

    /**
     * Update performance metrics (internal, called during update)
     * @param bridge Bridge instance
     * @param dt Delta time in seconds
     */
    void rogue_enemy_ai_bridge_update_metrics(RogueEnemyAIBridge* bridge, float dt);

    // === Debug and Diagnostic Tools ===

    /**
     * Enable/disable debug logging
     * @param bridge Bridge instance
     * @param enabled True to enable logging
     */
    void rogue_enemy_ai_bridge_set_debug_logging(RogueEnemyAIBridge* bridge, bool enabled);

    /**
     * Get debug status string for display
     * @param bridge Bridge instance
     * @param buffer Output buffer
     * @param buffer_size Buffer size
     * @return Number of characters written
     */
    int rogue_enemy_ai_bridge_get_debug_status(const RogueEnemyAIBridge* bridge, char* buffer,
                                               size_t buffer_size);

    /**
     * Validate bridge integrity (for testing)
     * @param bridge Bridge instance
     * @return 1 if valid, 0 if corrupted
     */
    int rogue_enemy_ai_bridge_validate(const RogueEnemyAIBridge* bridge);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_CORE_INTEGRATION_ENEMY_AI_BRIDGE_H
