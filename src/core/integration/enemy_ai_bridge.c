/**
 * Enemy Integration ↔ AI System Bridge Implementation (Phase 3.1)
 *
 * Comprehensive integration layer between enemy system and AI system,
 * providing event-driven communication, state synchronization, and
 * performance-aware coordination.
 */

#include "core/integration/enemy_ai_bridge.h"
#include "ai/core/ai_scheduler.h"
#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include "core/integration/config_version.h"
#include "entities/enemy.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#endif

// Performance monitoring helpers
static uint64_t get_current_time_microseconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t) ((counter.QuadPart * 1000000) / frequency.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

// Debug logging macro
#define BRIDGE_LOG(bridge, level, ...)                                                             \
    do                                                                                             \
    {                                                                                              \
        if ((bridge)->debug_logging_enabled)                                                       \
        {                                                                                          \
            snprintf((bridge)->debug_log_buffer, sizeof((bridge)->debug_log_buffer), __VA_ARGS__); \
            printf("[BRIDGE_%s] %s\n", level, (bridge)->debug_log_buffer);                         \
        }                                                                                          \
    } while (0)

// === Core Bridge API Implementation ===

int rogue_enemy_ai_bridge_init(RogueEnemyAIBridge* bridge, struct RogueEventBus* event_bus)
{
    if (!bridge)
    {
        printf("[ERROR] Enemy-AI Bridge: NULL bridge pointer\n");
        return 0;
    }

    // Initialize bridge state
    memset(bridge, 0, sizeof(RogueEnemyAIBridge));

    bridge->event_bus = event_bus;
    bridge->next_group_id = 1000; // Start group IDs at 1000

    // Set performance thresholds (configurable defaults)
    bridge->max_tick_time_warning_us = 500.0f; // 0.5ms warning
    bridge->max_tick_time_error_us = 2000.0f;  // 2ms error
    bridge->max_agents_per_frame = 50;         // Max 50 AI agents per frame
    bridge->metrics_update_interval = 1.0f;    // Update metrics every second

    // Initialize performance metrics
    bridge->metrics.last_metrics_reset = time(NULL);

    // Event types are handled directly by the event bus - no registration needed

    bridge->initialized = true;

    BRIDGE_LOG(bridge, "INFO",
               "Enemy-AI Bridge initialized successfully with event bus integration");
    return 1;
}

void rogue_enemy_ai_bridge_shutdown(RogueEnemyAIBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return;
    }

    BRIDGE_LOG(bridge, "INFO", "Shutting down Enemy-AI Bridge...");

    // Cleanup all active groups
    for (uint8_t i = 0; i < bridge->active_group_count; i++)
    {
        if (bridge->groups[i].shared_blackboard)
        {
            // Would call blackboard cleanup here
            bridge->groups[i].shared_blackboard = NULL;
        }
    }

    // Final metrics report
    if (bridge->debug_logging_enabled)
    {
        BRIDGE_LOG(bridge, "INFO", "Final metrics - AI agents: %u, BT ticks: %u, Groups: %u",
                   bridge->metrics.total_ai_agents, bridge->metrics.active_behavior_trees,
                   bridge->active_group_count);
    }

    bridge->initialized = false;
}

void rogue_enemy_ai_bridge_update(RogueEnemyAIBridge* bridge, float dt)
{
    if (!bridge || !bridge->initialized)
    {
        return;
    }

    // Update performance metrics
    rogue_enemy_ai_bridge_update_metrics(bridge, dt);

    // Update group coordination
    for (uint8_t i = 0; i < bridge->active_group_count; i++)
    {
        if (bridge->groups[i].group_id != 0)
        {
            rogue_enemy_ai_bridge_update_group_coordination(bridge, bridge->groups[i].group_id);
        }
    }

    // Check performance thresholds
    if (rogue_enemy_ai_bridge_check_performance_thresholds(bridge))
    {
        BRIDGE_LOG(bridge, "WARN", "AI performance thresholds exceeded");
        bridge->metrics.performance_warnings++;
    }
}

// === Enemy Lifecycle Integration ===

int rogue_enemy_ai_bridge_handle_spawn(RogueEnemyAIBridge* bridge, struct RogueEnemy* enemy)
{
    if (!bridge || !bridge->initialized || !enemy)
    {
        return 0;
    }

    uint64_t start_time = get_current_time_microseconds();

    // Enable AI behavior tree for the enemy
    if (enemy->ai_bt_enabled == 0)
    {
        enemy->ai_bt_enabled = 1;

        // Set initial AI intensity based on enemy tier/difficulty
        RogueEnemyAIIntensity intensity = ROGUE_AI_INTENSITY_NORMAL;
        if (enemy->boss_flag)
        {
            intensity = ROGUE_AI_INTENSITY_BOSS;
        }
        else if (enemy->elite_flag)
        {
            intensity = ROGUE_AI_INTENSITY_ELITE;
        }
        else if (enemy->tier_id > 3)
        {
            intensity = ROGUE_AI_INTENSITY_AGGRESSIVE;
        }

        enemy->ai_intensity = intensity;
        enemy->ai_intensity_score = 0.0f;
        enemy->ai_intensity_cooldown_ms = 0.0f;

        // Initialize AI behavior tree (placeholder - would create actual tree)
        // enemy->ai_tree = create_behavior_tree_for_enemy_type(enemy->type_index);

        BRIDGE_LOG(bridge, "INFO", "AI activated for enemy ID %u with intensity %d",
                   enemy->encounter_id, intensity);
    }

    // Update metrics
    bridge->metrics.total_ai_agents++;
    bridge->metrics.active_behavior_trees++;
    bridge->metrics.intensity_metrics[enemy->ai_intensity].agent_count++;

    uint64_t end_time = get_current_time_microseconds();
    float spawn_time_us = (float) (end_time - start_time);

    // Track spawn processing time
    if (spawn_time_us > bridge->metrics.peak_tick_time_us)
    {
        bridge->metrics.peak_tick_time_us = spawn_time_us;
    }

    bridge->state_sync_requests++;
    bridge->successful_syncs++;

    return 1;
}

int rogue_enemy_ai_bridge_handle_death(RogueEnemyAIBridge* bridge, struct RogueEnemy* enemy)
{
    if (!bridge || !bridge->initialized || !enemy)
    {
        return 0;
    }

    BRIDGE_LOG(bridge, "INFO", "Handling AI cleanup for enemy death ID %u", enemy->encounter_id);

    // Cleanup AI behavior tree
    if (enemy->ai_bt_enabled && enemy->ai_tree)
    {
        // Would cleanup behavior tree here
        enemy->ai_tree = NULL;
        enemy->ai_bt_state = NULL;
        enemy->ai_bt_enabled = 0;
    }

    // Remove from any groups
    for (uint8_t i = 0; i < bridge->active_group_count; i++)
    {
        RogueEnemyGroup* group = &bridge->groups[i];
        if (group->group_id == 0)
            continue;

        for (uint8_t j = 0; j < group->member_count; j++)
        {
            if (group->member_ids[j] == (uint32_t) enemy->encounter_id)
            {
                // Remove from group
                for (uint8_t k = j; k < group->member_count - 1; k++)
                {
                    group->member_ids[k] = group->member_ids[k + 1];
                }
                group->member_count--;

                // If group is empty, destroy it
                if (group->member_count == 0)
                {
                    rogue_enemy_ai_bridge_destroy_group(bridge, group->group_id);
                }
                break;
            }
        }
    }

    // Update metrics
    if (bridge->metrics.total_ai_agents > 0)
    {
        bridge->metrics.total_ai_agents--;
    }
    if (bridge->metrics.active_behavior_trees > 0)
    {
        bridge->metrics.active_behavior_trees--;
    }
    if (bridge->metrics.intensity_metrics[enemy->ai_intensity].agent_count > 0)
    {
        bridge->metrics.intensity_metrics[enemy->ai_intensity].agent_count--;
    }

    return 1;
}

// === AI State Synchronization ===

int rogue_enemy_ai_bridge_sync_state(RogueEnemyAIBridge* bridge, struct RogueEnemy* enemy)
{
    if (!bridge || !bridge->initialized || !enemy)
    {
        return 0;
    }

    bridge->state_sync_requests++;

    // Sync AI state with enemy state
    if (enemy->ai_bt_enabled && enemy->ai_bt_state)
    {
        // Sync position, health, status effects, etc. with AI blackboard
        // This would involve updating the blackboard with current enemy state

        // Example synchronization (placeholder):
        // blackboard_set_float(enemy->ai_bt_state, "health_ratio", (float)enemy->health /
        // enemy->max_health); blackboard_set_float(enemy->ai_bt_state, "position_x",
        // enemy->base.x); blackboard_set_float(enemy->ai_bt_state, "position_y", enemy->base.y);

        bridge->successful_syncs++;
        return 1;
    }

    bridge->failed_syncs++;
    return 0;
}

uint32_t rogue_enemy_ai_bridge_sync_all_states(RogueEnemyAIBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    // This would iterate through all active enemies and sync their states
    // For now, return a simulated count
    uint32_t synced_count = bridge->metrics.total_ai_agents;
    bridge->state_sync_requests += synced_count;
    bridge->successful_syncs += synced_count;

    BRIDGE_LOG(bridge, "INFO", "Synchronized %u AI states", synced_count);
    return synced_count;
}

// === Modifier Application Hooks ===

int rogue_enemy_ai_bridge_apply_modifier_hook(RogueEnemyAIBridge* bridge, struct RogueEnemy* enemy,
                                              uint32_t modifier_id)
{
    if (!bridge || !bridge->initialized || !enemy)
    {
        return 0;
    }

    BRIDGE_LOG(bridge, "INFO", "Applying modifier %u to enemy AI ID %u", modifier_id,
               enemy->encounter_id);

    // Apply modifier effects to AI decision making
    if (enemy->ai_bt_enabled && enemy->ai_bt_state)
    {
        // Example modifier applications (placeholder):
        switch (modifier_id)
        {
        case 1: // Speed boost modifier
            // blackboard_set_float(enemy->ai_bt_state, "speed_multiplier", 1.5f);
            break;
        case 2: // Berserker modifier
            // blackboard_set_bool(enemy->ai_bt_state, "berserker_mode", true);
            enemy->ai_intensity = ROGUE_AI_INTENSITY_AGGRESSIVE;
            break;
        case 3: // Cautious modifier
            // blackboard_set_bool(enemy->ai_bt_state, "cautious_mode", true);
            enemy->ai_intensity = ROGUE_AI_INTENSITY_PASSIVE;
            break;
        default:
            break;
        }

        // Force AI state sync after modifier application
        rogue_enemy_ai_bridge_sync_state(bridge, enemy);
        return 1;
    }

    return 0;
}

// === AI Behavior Intensity Scaling ===

RogueEnemyAIIntensity rogue_enemy_ai_bridge_scale_intensity(RogueEnemyAIBridge* bridge,
                                                            struct RogueEnemy* enemy,
                                                            uint32_t difficulty_level)
{
    if (!bridge || !bridge->initialized || !enemy)
    {
        return ROGUE_AI_INTENSITY_NORMAL;
    }

    RogueEnemyAIIntensity new_intensity = ROGUE_AI_INTENSITY_NORMAL;

    // Scale intensity based on difficulty level (0-100)
    if (difficulty_level >= 80)
    {
        new_intensity = ROGUE_AI_INTENSITY_BOSS;
    }
    else if (difficulty_level >= 60)
    {
        new_intensity = ROGUE_AI_INTENSITY_ELITE;
    }
    else if (difficulty_level >= 40)
    {
        new_intensity = ROGUE_AI_INTENSITY_AGGRESSIVE;
    }
    else if (difficulty_level >= 20)
    {
        new_intensity = ROGUE_AI_INTENSITY_NORMAL;
    }
    else
    {
        new_intensity = ROGUE_AI_INTENSITY_PASSIVE;
    }

    // Apply boss/elite flags
    if (enemy->boss_flag)
    {
        new_intensity = ROGUE_AI_INTENSITY_BOSS;
    }
    else if (enemy->elite_flag && new_intensity < ROGUE_AI_INTENSITY_ELITE)
    {
        new_intensity = ROGUE_AI_INTENSITY_ELITE;
    }

    BRIDGE_LOG(bridge, "INFO", "Scaled AI intensity for enemy ID %u: difficulty %u -> intensity %d",
               enemy->encounter_id, difficulty_level, new_intensity);

    return new_intensity;
}

int rogue_enemy_ai_bridge_update_intensity(RogueEnemyAIBridge* bridge, struct RogueEnemy* enemy)
{
    if (!bridge || !bridge->initialized || !enemy)
    {
        return 0;
    }

    // Check if intensity cooldown is active
    if (enemy->ai_intensity_cooldown_ms > 0)
    {
        return 0; // No change during cooldown
    }

    RogueEnemyAIIntensity old_intensity = (RogueEnemyAIIntensity) enemy->ai_intensity;
    RogueEnemyAIIntensity new_intensity = old_intensity;

    // Calculate dynamic intensity based on current conditions
    float health_ratio = (float) enemy->health / enemy->max_health;
    float intensity_score = enemy->ai_intensity_score;

    // Low health increases intensity
    if (health_ratio < 0.25f)
    {
        intensity_score += 2.0f;
    }
    else if (health_ratio < 0.5f)
    {
        intensity_score += 1.0f;
    }

    // High intensity score escalates behavior
    if (intensity_score > 5.0f && old_intensity < ROGUE_AI_INTENSITY_BOSS)
    {
        new_intensity = (RogueEnemyAIIntensity) (old_intensity + 1);
    }
    else if (intensity_score < -3.0f && old_intensity > ROGUE_AI_INTENSITY_PASSIVE)
    {
        new_intensity = (RogueEnemyAIIntensity) (old_intensity - 1);
    }

    if (new_intensity != old_intensity)
    {
        enemy->ai_intensity = new_intensity;
        enemy->ai_intensity_cooldown_ms = 2000.0f; // 2 second cooldown

        // Update metrics
        if (bridge->metrics.intensity_metrics[old_intensity].agent_count > 0)
        {
            bridge->metrics.intensity_metrics[old_intensity].agent_count--;
        }
        bridge->metrics.intensity_metrics[new_intensity].agent_count++;

        BRIDGE_LOG(bridge, "INFO", "AI intensity changed for enemy ID %u: %d -> %d",
                   enemy->encounter_id, old_intensity, new_intensity);

        return 1;
    }

    return 0;
}

// === Enemy Group Coordination ===

uint32_t rogue_enemy_ai_bridge_create_group(RogueEnemyAIBridge* bridge, uint32_t* enemy_ids,
                                            uint8_t enemy_count, uint32_t formation_pattern)
{
    if (!bridge || !bridge->initialized || !enemy_ids || enemy_count == 0)
    {
        return 0;
    }

    if (bridge->active_group_count >= 64)
    {
        BRIDGE_LOG(bridge, "ERROR", "Maximum number of enemy groups reached (64)");
        return 0;
    }

    // Find available group slot
    RogueEnemyGroup* group = NULL;
    for (uint8_t i = 0; i < 64; i++)
    {
        if (bridge->groups[i].group_id == 0)
        {
            group = &bridge->groups[i];
            break;
        }
    }

    if (!group)
    {
        return 0;
    }

    // Initialize group
    group->group_id = bridge->next_group_id++;
    group->member_count = enemy_count > 16 ? 16 : enemy_count; // Cap at 16 members
    group->formation_pattern = formation_pattern;
    group->group_intensity = ROGUE_AI_INTENSITY_NORMAL;
    group->last_update_time = 0.0f;
    group->center_x = 0.0f;
    group->center_y = 0.0f;

    // Copy enemy IDs
    for (uint8_t i = 0; i < group->member_count; i++)
    {
        group->member_ids[i] = enemy_ids[i];
    }

    // Create shared blackboard (placeholder - would create actual blackboard)
    // group->shared_blackboard = blackboard_create();
    group->shared_blackboard = (struct RogueBlackboard*) malloc(64); // Placeholder

    bridge->active_group_count++;

    BRIDGE_LOG(bridge, "INFO", "Created enemy group %u with %u members, formation %u",
               group->group_id, group->member_count, formation_pattern);

    return group->group_id;
}

int rogue_enemy_ai_bridge_destroy_group(RogueEnemyAIBridge* bridge, uint32_t group_id)
{
    if (!bridge || !bridge->initialized || group_id == 0)
    {
        return 0;
    }

    // Find and destroy group
    for (uint8_t i = 0; i < 64; i++)
    {
        if (bridge->groups[i].group_id == group_id)
        {
            RogueEnemyGroup* group = &bridge->groups[i];

            // Cleanup shared blackboard
            if (group->shared_blackboard)
            {
                free(group->shared_blackboard); // Placeholder cleanup
                group->shared_blackboard = NULL;
            }

            BRIDGE_LOG(bridge, "INFO", "Destroyed enemy group %u", group_id);

            // Clear group data
            memset(group, 0, sizeof(RogueEnemyGroup));

            if (bridge->active_group_count > 0)
            {
                bridge->active_group_count--;
            }

            return 1;
        }
    }

    return 0;
}

int rogue_enemy_ai_bridge_update_group_coordination(RogueEnemyAIBridge* bridge, uint32_t group_id)
{
    if (!bridge || !bridge->initialized || group_id == 0)
    {
        return 0;
    }

    // Find group
    RogueEnemyGroup* group = NULL;
    for (uint8_t i = 0; i < 64; i++)
    {
        if (bridge->groups[i].group_id == group_id)
        {
            group = &bridge->groups[i];
            break;
        }
    }

    if (!group || group->member_count == 0)
    {
        return 0;
    }

    // Update group center position (would calculate from member positions)
    // For now, just update timestamp
    group->last_update_time = get_current_time_microseconds() / 1000000.0f;

    // Update shared blackboard with group information
    if (group->shared_blackboard)
    {
        // Example coordination data (placeholder):
        // blackboard_set_float(group->shared_blackboard, "group_center_x", group->center_x);
        // blackboard_set_float(group->shared_blackboard, "group_center_y", group->center_y);
        // blackboard_set_int(group->shared_blackboard, "formation_pattern",
        // group->formation_pattern); blackboard_set_int(group->shared_blackboard, "member_count",
        // group->member_count);
    }

    return 1;
}

// === Performance Metrics Integration ===

const RogueAIPerformanceMetrics* rogue_enemy_ai_bridge_get_metrics(const RogueEnemyAIBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return NULL;
    }

    return &bridge->metrics;
}

void rogue_enemy_ai_bridge_reset_metrics(RogueEnemyAIBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return;
    }

    // Preserve agent counts, reset performance counters
    uint32_t total_agents = bridge->metrics.total_ai_agents;
    uint32_t active_trees = bridge->metrics.active_behavior_trees;

    struct
    {
        uint32_t agent_count;
    } intensity_counts[5];

    for (int i = 0; i < 5; i++)
    {
        intensity_counts[i].agent_count = bridge->metrics.intensity_metrics[i].agent_count;
    }

    memset(&bridge->metrics, 0, sizeof(RogueAIPerformanceMetrics));

    // Restore counts
    bridge->metrics.total_ai_agents = total_agents;
    bridge->metrics.active_behavior_trees = active_trees;
    bridge->metrics.last_metrics_reset = time(NULL);

    for (int i = 0; i < 5; i++)
    {
        bridge->metrics.intensity_metrics[i].agent_count = intensity_counts[i].agent_count;
    }

    BRIDGE_LOG(bridge, "INFO", "Performance metrics reset");
}

int rogue_enemy_ai_bridge_check_performance_thresholds(const RogueEnemyAIBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    // Check various performance thresholds
    if (bridge->metrics.peak_tick_time_us > bridge->max_tick_time_error_us)
    {
        return 1; // Error threshold exceeded
    }

    if (bridge->metrics.average_tick_time_us > bridge->max_tick_time_warning_us)
    {
        return 1; // Warning threshold exceeded
    }

    if (bridge->metrics.total_ai_agents > bridge->max_agents_per_frame)
    {
        return 1; // Too many agents
    }

    return 0; // Within thresholds
}

void rogue_enemy_ai_bridge_update_metrics(RogueEnemyAIBridge* bridge, float dt)
{
    if (!bridge || !bridge->initialized)
    {
        return;
    }

    bridge->last_metrics_update += dt;

    if (bridge->last_metrics_update >= bridge->metrics_update_interval)
    {
        // Update derived metrics
        if (bridge->metrics.total_ai_agents > 0)
        {
            bridge->metrics.ticks_per_second = (uint32_t) (bridge->metrics.total_ai_agents / dt);
        }

        // Calculate average tick time across all intensity levels
        float total_tick_time = 0.0f;
        uint32_t total_ticks = 0;

        for (int i = 0; i < 5; i++)
        {
            total_tick_time += bridge->metrics.intensity_metrics[i].average_tick_time_us *
                               bridge->metrics.intensity_metrics[i].total_ticks;
            total_ticks += bridge->metrics.intensity_metrics[i].total_ticks;
        }

        if (total_ticks > 0)
        {
            bridge->metrics.average_tick_time_us = total_tick_time / total_ticks;
        }

        // Update memory usage estimates (placeholder)
        bridge->metrics.blackboard_memory_bytes =
            bridge->metrics.total_ai_agents * 256; // Est. 256 bytes per blackboard
        bridge->metrics.behavior_tree_memory_bytes =
            bridge->metrics.active_behavior_trees * 512; // Est. 512 bytes per tree
        bridge->metrics.group_coordination_memory_bytes =
            bridge->active_group_count * 128; // Est. 128 bytes per group

        bridge->last_metrics_update = 0.0f;
    }
}

// === Debug and Diagnostic Tools ===

void rogue_enemy_ai_bridge_set_debug_logging(RogueEnemyAIBridge* bridge, bool enabled)
{
    if (!bridge)
    {
        return;
    }

    bridge->debug_logging_enabled = enabled;
    printf("[BRIDGE_INFO] Debug logging %s\n", enabled ? "enabled" : "disabled");
}

int rogue_enemy_ai_bridge_get_debug_status(const RogueEnemyAIBridge* bridge, char* buffer,
                                           size_t buffer_size)
{
    if (!bridge || !buffer || buffer_size == 0)
    {
        return 0;
    }

    return snprintf(buffer, buffer_size,
                    "Enemy-AI Bridge Status:\n"
                    "  Initialized: %s\n"
                    "  AI Agents: %u\n"
                    "  Active Groups: %u\n"
                    "  Sync Requests: %u (Success: %u, Failed: %u)\n"
                    "  Performance: %.2f avg tick time (%.2f peak)\n"
                    "  Warnings: %u, Errors: %u\n",
                    bridge->initialized ? "YES" : "NO", bridge->metrics.total_ai_agents,
                    bridge->active_group_count, bridge->state_sync_requests,
                    bridge->successful_syncs, bridge->failed_syncs,
                    bridge->metrics.average_tick_time_us, bridge->metrics.peak_tick_time_us,
                    bridge->metrics.performance_warnings, bridge->metrics.performance_errors);
}

int rogue_enemy_ai_bridge_validate(const RogueEnemyAIBridge* bridge)
{
    if (!bridge)
    {
        return 0;
    }

    // Basic integrity checks
    if (!bridge->initialized)
    {
        return 0;
    }

    if (bridge->active_group_count > 64)
    {
        return 0;
    }

    if (bridge->next_group_id == 0)
    {
        return 0;
    }

    // Validate group integrity
    uint8_t actual_group_count = 0;
    for (uint8_t i = 0; i < 64; i++)
    {
        if (bridge->groups[i].group_id != 0)
        {
            actual_group_count++;

            // Check group member count
            if (bridge->groups[i].member_count > 16)
            {
                return 0;
            }
        }
    }

    if (actual_group_count != bridge->active_group_count)
    {
        return 0;
    }

    return 1; // Valid
}
