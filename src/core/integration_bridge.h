#ifndef ROGUE_INTEGRATION_BRIDGE_H
#define ROGUE_INTEGRATION_BRIDGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct RogueIntegrationBridge RogueIntegrationBridge;
typedef struct RogueBridgeEvent RogueBridgeEvent;
typedef struct RogueBridgeListener RogueBridgeListener;
typedef struct RogueEnemyAIBridge RogueEnemyAIBridge;
typedef struct RogueSystemBridge RogueSystemBridge;

// Maximum limits for integration bridge system
#define ROGUE_BRIDGE_MAX_EVENTS 512
#define ROGUE_BRIDGE_MAX_LISTENERS 128
#define ROGUE_BRIDGE_MAX_SYSTEMS 32
#define ROGUE_BRIDGE_MAX_DATA_SIZE 1024
#define ROGUE_BRIDGE_MAX_NAME_LENGTH 64

// Event types for cross-system communication
typedef enum {
    ROGUE_BRIDGE_EVENT_ENEMY_SPAWN,         // Enemy spawned, notify AI system
    ROGUE_BRIDGE_EVENT_ENEMY_DEATH,         // Enemy died, cleanup AI state
    ROGUE_BRIDGE_EVENT_ENEMY_MODIFIER,      // Enemy modifier applied, update AI behavior
    ROGUE_BRIDGE_EVENT_EQUIPMENT_CHANGE,    // Equipment changed, update combat stats
    ROGUE_BRIDGE_EVENT_EQUIPMENT_PROC,      // Equipment proc triggered, apply effect
    ROGUE_BRIDGE_EVENT_COMBAT_DAMAGE,       // Combat damage dealt, update XP/durability
    ROGUE_BRIDGE_EVENT_COMBAT_KILL,         // Combat kill achieved, trigger progression
    ROGUE_BRIDGE_EVENT_LOOT_DROP,           // Loot dropped, notify crafting system
    ROGUE_BRIDGE_EVENT_CRAFTING_COMPLETE,   // Crafting complete, update vendor prices
    ROGUE_BRIDGE_EVENT_CUSTOM              // Custom user-defined events
} RogueBridgeEventType;

// Event priorities
typedef enum {
    ROGUE_BRIDGE_PRIORITY_CRITICAL,        // Must be processed immediately
    ROGUE_BRIDGE_PRIORITY_HIGH,            // Process ASAP
    ROGUE_BRIDGE_PRIORITY_NORMAL,          // Standard processing
    ROGUE_BRIDGE_PRIORITY_LOW,             // Can be delayed
    ROGUE_BRIDGE_PRIORITY_BACKGROUND       // Process when resources available
} RogueBridgePriority;

// System types for bridge management
typedef enum {
    ROGUE_BRIDGE_SYSTEM_ENEMY_INTEGRATION,
    ROGUE_BRIDGE_SYSTEM_AI,
    ROGUE_BRIDGE_SYSTEM_COMBAT,
    ROGUE_BRIDGE_SYSTEM_EQUIPMENT,
    ROGUE_BRIDGE_SYSTEM_PROGRESSION,
    ROGUE_BRIDGE_SYSTEM_LOOT,
    ROGUE_BRIDGE_SYSTEM_CRAFTING,
    ROGUE_BRIDGE_SYSTEM_VENDOR,
    ROGUE_BRIDGE_SYSTEM_COUNT
} RogueBridgeSystemType;

// Bridge event structure
typedef struct RogueBridgeEvent {
    RogueBridgeEventType event_type;        // Type of event
    RogueBridgePriority priority;           // Processing priority
    RogueBridgeSystemType source_system;    // System that generated the event
    RogueBridgeSystemType target_system;    // System that should handle the event
    uint32_t event_id;                      // Unique event identifier
    uint64_t timestamp;                     // Event creation timestamp
    size_t data_size;                       // Size of event data
    uint8_t event_data[ROGUE_BRIDGE_MAX_DATA_SIZE]; // Event payload
    bool processed;                         // Event processing status
    char description[ROGUE_BRIDGE_MAX_NAME_LENGTH]; // Human-readable description
} RogueBridgeEvent;

// Event listener callback function
typedef bool (*RogueBridgeListenerCallback)(const RogueBridgeEvent* event, void* user_data);

// Bridge event listener
typedef struct RogueBridgeListener {
    RogueBridgeEventType event_type;        // Type of event to listen for
    RogueBridgeSystemType system_type;      // System this listener belongs to
    RogueBridgeListenerCallback callback;   // Function to call when event occurs
    void* user_data;                        // User data passed to callback
    bool active;                           // Listener is active
    uint32_t listener_id;                   // Unique listener identifier
    char name[ROGUE_BRIDGE_MAX_NAME_LENGTH]; // Listener name for debugging
} RogueBridgeListener;

// Enemy-AI bridge specific data structures
typedef struct RogueEnemySpawnData {
    uint32_t enemy_id;                      // Enemy instance ID
    uint32_t enemy_type_id;                 // Enemy type definition ID
    float position_x, position_y;           // Spawn position
    int difficulty_level;                   // Enemy difficulty
    uint32_t modifier_flags;                // Applied modifiers
    bool requires_ai;                       // Needs AI behavior tree
} RogueEnemySpawnData;

typedef struct RogueEnemyDeathData {
    uint32_t enemy_id;                      // Enemy instance ID
    uint32_t killer_id;                     // ID of entity that killed enemy
    float death_position_x, death_position_y; // Death position
    uint64_t experience_reward;             // XP to award
    bool cleanup_ai;                        // Should cleanup AI state
} RogueEnemyDeathData;

typedef struct RogueEnemyModifierData {
    uint32_t enemy_id;                      // Enemy instance ID
    uint32_t modifier_id;                   // Modifier definition ID
    float intensity_multiplier;             // AI intensity scaling
    bool affects_behavior;                  // Modifies AI behavior
    char modifier_name[32];                 // Modifier name
} RogueEnemyModifierData;

// System bridge interface
typedef struct RogueSystemBridge {
    RogueBridgeSystemType system_type;      // Type of system
    bool active;                           // System is active
    uint32_t events_sent;                   // Events sent by this system
    uint32_t events_received;               // Events received by this system
    uint32_t listener_count;                // Number of active listeners
    char system_name[ROGUE_BRIDGE_MAX_NAME_LENGTH]; // System name
} RogueSystemBridge;

// Main integration bridge structure
typedef struct RogueIntegrationBridge {
    // Event management
    RogueBridgeEvent events[ROGUE_BRIDGE_MAX_EVENTS]; // Event queue
    int event_count;                        // Current number of events
    int event_head;                         // Queue head index
    int event_tail;                         // Queue tail index
    uint32_t next_event_id;                 // Next event ID to assign
    
    // Listener management
    RogueBridgeListener listeners[ROGUE_BRIDGE_MAX_LISTENERS]; // Event listeners
    int listener_count;                     // Number of active listeners
    uint32_t next_listener_id;              // Next listener ID to assign
    
    // System management
    RogueSystemBridge systems[ROGUE_BRIDGE_SYSTEM_COUNT]; // Registered systems
    int active_system_count;                // Number of active systems
    
    // Configuration
    bool auto_process_events;               // Automatically process events
    bool debug_mode;                        // Enable debug logging
    int max_events_per_frame;               // Max events to process per frame
    
    // Statistics
    uint64_t total_events_processed;        // Total events processed
    uint64_t total_events_dropped;          // Events dropped due to queue full
    uint64_t last_process_time_ms;          // Time for last processing cycle
    uint32_t processing_errors;             // Number of processing errors
} RogueIntegrationBridge;

// Core bridge functions
RogueIntegrationBridge* rogue_integration_bridge_create(void);
void rogue_integration_bridge_destroy(RogueIntegrationBridge* bridge);
bool rogue_integration_bridge_initialize(RogueIntegrationBridge* bridge);
void rogue_integration_bridge_shutdown(RogueIntegrationBridge* bridge);
void rogue_integration_bridge_reset(RogueIntegrationBridge* bridge);

// Configuration functions
void rogue_integration_bridge_set_auto_process(RogueIntegrationBridge* bridge, bool auto_process);
void rogue_integration_bridge_set_debug_mode(RogueIntegrationBridge* bridge, bool debug_mode);
void rogue_integration_bridge_set_max_events_per_frame(RogueIntegrationBridge* bridge, int max_events);

// System registration and management
bool rogue_integration_bridge_register_system(RogueIntegrationBridge* bridge, RogueBridgeSystemType system_type, const char* system_name);
bool rogue_integration_bridge_unregister_system(RogueIntegrationBridge* bridge, RogueBridgeSystemType system_type);
bool rogue_integration_bridge_is_system_active(RogueIntegrationBridge* bridge, RogueBridgeSystemType system_type);
RogueSystemBridge* rogue_integration_bridge_get_system(RogueIntegrationBridge* bridge, RogueBridgeSystemType system_type);

// Event listener management
uint32_t rogue_integration_bridge_add_listener(RogueIntegrationBridge* bridge, RogueBridgeEventType event_type, 
                                              RogueBridgeSystemType system_type, RogueBridgeListenerCallback callback, 
                                              void* user_data, const char* listener_name);
bool rogue_integration_bridge_remove_listener(RogueIntegrationBridge* bridge, uint32_t listener_id);
bool rogue_integration_bridge_enable_listener(RogueIntegrationBridge* bridge, uint32_t listener_id, bool enabled);
int rogue_integration_bridge_get_listeners_for_event(RogueIntegrationBridge* bridge, RogueBridgeEventType event_type, 
                                                    RogueBridgeListener** listeners, int max_listeners);

// Event creation and posting
uint32_t rogue_integration_bridge_create_event(RogueIntegrationBridge* bridge, RogueBridgeEventType event_type,
                                              RogueBridgeSystemType source_system, RogueBridgeSystemType target_system,
                                              RogueBridgePriority priority, const void* data, size_t data_size,
                                              const char* description);
bool rogue_integration_bridge_post_event(RogueIntegrationBridge* bridge, uint32_t event_id);
bool rogue_integration_bridge_post_event_immediate(RogueIntegrationBridge* bridge, RogueBridgeEventType event_type,
                                                  RogueBridgeSystemType source_system, RogueBridgeSystemType target_system,
                                                  RogueBridgePriority priority, const void* data, size_t data_size,
                                                  const char* description);

// Event processing
int rogue_integration_bridge_process_events(RogueIntegrationBridge* bridge, int max_events);
bool rogue_integration_bridge_process_single_event(RogueIntegrationBridge* bridge, const RogueBridgeEvent* event);
void rogue_integration_bridge_clear_processed_events(RogueIntegrationBridge* bridge);
int rogue_integration_bridge_get_pending_event_count(RogueIntegrationBridge* bridge);

// Specific bridge implementations - Enemy Integration ↔ AI System Bridge
bool rogue_integration_bridge_enemy_spawn(RogueIntegrationBridge* bridge, const RogueEnemySpawnData* spawn_data);
bool rogue_integration_bridge_enemy_death(RogueIntegrationBridge* bridge, const RogueEnemyDeathData* death_data);
bool rogue_integration_bridge_enemy_modifier_applied(RogueIntegrationBridge* bridge, const RogueEnemyModifierData* modifier_data);

// Enemy-AI Bridge Listener Helpers
uint32_t rogue_integration_bridge_add_enemy_spawn_listener(RogueIntegrationBridge* bridge, RogueBridgeListenerCallback callback, void* user_data);
uint32_t rogue_integration_bridge_add_enemy_death_listener(RogueIntegrationBridge* bridge, RogueBridgeListenerCallback callback, void* user_data);
uint32_t rogue_integration_bridge_add_enemy_modifier_listener(RogueIntegrationBridge* bridge, RogueBridgeListenerCallback callback, void* user_data);

// Combat ↔ Equipment Bridge events
typedef struct RogueEquipmentChangeData {
    uint32_t entity_id;                     // Entity that changed equipment
    uint32_t item_id;                       // Item being equipped/unequipped
    uint32_t slot_id;                       // Equipment slot
    bool equipped;                          // true if equipped, false if unequipped
    int stat_changes[16];                   // Stat changes from equipment
} RogueEquipmentChangeData;

typedef struct RogueCombatDamageData {
    uint32_t attacker_id;                   // Entity dealing damage
    uint32_t target_id;                     // Entity receiving damage
    int damage_amount;                      // Damage dealt
    uint32_t damage_type;                   // Type of damage
    bool was_critical;                      // Critical hit flag
    uint64_t experience_gained;             // XP gained from damage
} RogueCombatDamageData;

bool rogue_integration_bridge_equipment_changed(RogueIntegrationBridge* bridge, const RogueEquipmentChangeData* equipment_data);
bool rogue_integration_bridge_combat_damage(RogueIntegrationBridge* bridge, const RogueCombatDamageData* damage_data);

// Utility functions
const char* rogue_integration_bridge_get_event_type_name(RogueBridgeEventType event_type);
const char* rogue_integration_bridge_get_system_type_name(RogueBridgeSystemType system_type);
const char* rogue_integration_bridge_get_priority_name(RogueBridgePriority priority);
bool rogue_integration_bridge_validate_event_data(const RogueBridgeEvent* event);

// Debug and monitoring functions
void rogue_integration_bridge_print_statistics(RogueIntegrationBridge* bridge);
void rogue_integration_bridge_print_active_listeners(RogueIntegrationBridge* bridge);
void rogue_integration_bridge_print_event_queue(RogueIntegrationBridge* bridge);
bool rogue_integration_bridge_export_event_log(RogueIntegrationBridge* bridge, const char* filename);

// Performance monitoring
void rogue_integration_bridge_get_performance_stats(RogueIntegrationBridge* bridge, uint64_t* events_processed, 
                                                   uint64_t* events_dropped, uint64_t* processing_time_ms, 
                                                   uint32_t* processing_errors);
void rogue_integration_bridge_reset_performance_stats(RogueIntegrationBridge* bridge);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_INTEGRATION_BRIDGE_H
