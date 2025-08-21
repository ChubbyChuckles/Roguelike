#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "util/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct RogueEventBus RogueEventBus;
typedef struct RogueEvent RogueEvent;
typedef struct RogueEventSubscription RogueEventSubscription;

/* Constants (Phase 1.1.1) */
#define ROGUE_MAX_EVENT_SUBSCRIBERS 256
#define ROGUE_MAX_EVENT_QUEUE_SIZE 4096
#define ROGUE_MAX_EVENT_TYPES 512
#define ROGUE_MAX_EVENT_PAYLOAD_SIZE 512
#define ROGUE_EVENT_BUS_NAME_MAX 64

/* Event Bus Statistics (Phase 1.1.5) */
typedef struct {
    uint64_t events_published;
    uint64_t events_processed;
    uint64_t events_dropped;
    uint64_t events_failed;
    uint64_t total_processing_time_us;
    uint32_t current_queue_depth;
    uint32_t max_queue_depth_reached;
    double average_latency_us;
    double peak_latency_us;
    uint32_t active_subscribers;
} RogueEventBusStats;

/* Event Priority Levels (Phase 1.3.1) */
typedef enum {
    ROGUE_EVENT_PRIORITY_CRITICAL = 0,  /* Must process immediately */
    ROGUE_EVENT_PRIORITY_HIGH,          /* Process before normal events */
    ROGUE_EVENT_PRIORITY_NORMAL,        /* Standard priority */
    ROGUE_EVENT_PRIORITY_LOW,           /* Process when queue is light */
    ROGUE_EVENT_PRIORITY_BACKGROUND,    /* Process during idle time */
    ROGUE_EVENT_PRIORITY_COUNT
} RogueEventPriority;

/* Event Processing Strategies (Phase 1.5.4) */
typedef enum {
    ROGUE_EVENT_STRATEGY_FIFO,      /* First In, First Out */
    ROGUE_EVENT_STRATEGY_PRIORITY,  /* Priority-based ordering */
    ROGUE_EVENT_STRATEGY_DEADLINE,  /* Deadline-driven processing */
    ROGUE_EVENT_STRATEGY_COUNT
} RogueEventProcessingStrategy;

/* Event Type ID (Phase 1.1.2) - Compile-time assigned */
typedef uint32_t RogueEventTypeId;

/* Core Entity Lifecycle Events (Phase 1.2.1) */
#define ROGUE_EVENT_ENTITY_CREATED    0x0001
#define ROGUE_EVENT_ENTITY_DESTROYED  0x0002
#define ROGUE_EVENT_ENTITY_MODIFIED   0x0003

/* Player Action Events (Phase 1.2.2) */
#define ROGUE_EVENT_PLAYER_MOVED      0x0101
#define ROGUE_EVENT_PLAYER_ATTACKED   0x0102
#define ROGUE_EVENT_PLAYER_EQUIPPED   0x0103
#define ROGUE_EVENT_PLAYER_SKILLED    0x0104

/* Combat Events (Phase 1.2.3) */
#define ROGUE_EVENT_DAMAGE_DEALT      0x0201
#define ROGUE_EVENT_DAMAGE_TAKEN      0x0202
#define ROGUE_EVENT_CRITICAL_HIT      0x0203
#define ROGUE_EVENT_STATUS_APPLIED    0x0204

/* Progression Events (Phase 1.2.4) */
#define ROGUE_EVENT_XP_GAINED         0x0301
#define ROGUE_EVENT_LEVEL_UP          0x0302
#define ROGUE_EVENT_SKILL_UNLOCKED    0x0303
#define ROGUE_EVENT_MASTERY_INCREASED 0x0304

/* Economy Events (Phase 1.2.5) */
#define ROGUE_EVENT_ITEM_DROPPED      0x0401
#define ROGUE_EVENT_ITEM_PICKED_UP    0x0402
#define ROGUE_EVENT_TRADE_COMPLETED   0x0403
#define ROGUE_EVENT_CURRENCY_CHANGED  0x0404

/* World Events (Phase 1.2.6) */
#define ROGUE_EVENT_AREA_ENTERED      0x0501
#define ROGUE_EVENT_AREA_EXITED       0x0502
#define ROGUE_EVENT_RESOURCE_SPAWNED  0x0503
#define ROGUE_EVENT_STRUCTURE_GENERATED 0x0504

/* System Events (Phase 1.2.7) */
#define ROGUE_EVENT_CONFIG_RELOADED   0x0601
#define ROGUE_EVENT_SAVE_COMPLETED    0x0602
#define ROGUE_EVENT_ERROR_OCCURRED    0x0603
#define ROGUE_EVENT_PERFORMANCE_ALERT 0x0604

/* Event Payload Union (Phase 1.1.3) - Type-safe payload system */
typedef union {
    /* Entity Events */
    struct {
        uint32_t entity_id;
        uint32_t entity_type;
        void* entity_data;
    } entity;
    
    /* Player Events */
    struct {
        float x, y;
        float prev_x, prev_y;
        uint32_t area_id;
    } player_moved;
    
    struct {
        uint32_t target_entity_id;
        uint32_t weapon_id;
        uint32_t skill_id;
    } player_attacked;
    
    struct {
        uint32_t item_id;
        uint8_t slot_type;
        uint8_t slot_index;
        bool equipped; /* true = equipped, false = unequipped */
    } player_equipped;
    
    /* Combat Events */
    struct {
        uint32_t source_entity_id;
        uint32_t target_entity_id;
        float damage_amount;
        uint32_t damage_type;
        bool is_critical;
    } damage_event;
    
    /* Progression Events */
    struct {
        uint32_t player_id;
        uint32_t xp_amount;
        uint32_t source_type; /* monster, quest, etc. */
        uint32_t source_id;
    } xp_gained;
    
    struct {
        uint32_t player_id;
        uint8_t old_level;
        uint8_t new_level;
    } level_up;
    
    /* Economy Events */
    struct {
        uint32_t item_id;
        float x, y;
        uint32_t area_id;
        uint32_t source_entity_id;
    } item_dropped;
    
    struct {
        uint32_t item_id;
        uint32_t player_id;
        bool auto_pickup;
    } item_picked_up;
    
    /* World Events */
    struct {
        uint32_t area_id;
        uint32_t player_id;
        uint32_t previous_area_id;
    } area_transition;
    
    /* System Events */
    struct {
        char config_file[64];
        bool success;
        char error_message[128];
    } config_reloaded;
    
    struct {
        char save_file[64];
        bool success;
        double save_time_seconds;
    } save_completed;
    
    struct {
        uint32_t error_code;
        uint32_t system_id;
        char error_message[128];
        char function_name[64];
    } error_occurred;
    
    /* Raw payload for custom events */
    uint8_t raw_data[ROGUE_MAX_EVENT_PAYLOAD_SIZE];
} RogueEventPayload;

/* Event Structure (Phase 1.1.4 & 1.3.2) */
typedef struct RogueEvent {
    RogueEventTypeId type_id;
    RogueEventPriority priority;
    RogueEventPayload payload;
    
    /* Source tracking (Phase 1.1.4) */
    uint32_t source_system_id;
    char source_name[32];
    
    /* Ordering & Replay (Phase 1.3.2) */
    uint64_t timestamp_us;   /* Microsecond timestamp */
    uint64_t sequence_number; /* For deterministic ordering */
    
    /* Processing metadata */
    uint64_t deadline_us;    /* Must process by this time (Phase 1.3.6) */
    uint8_t retry_count;     /* Number of processing attempts */
    uint8_t max_retries;     /* Maximum retry attempts */
    bool processed;          /* Has been processed successfully */
    
    /* Internal linked list for queue management */
    struct RogueEvent* next;
} RogueEvent;

/* Event Callback Function Type (Phase 1.4.1) */
typedef bool (*RogueEventCallback)(const RogueEvent* event, void* user_data);

/* Event Subscription Predicate (Phase 1.4.3) */
typedef bool (*RogueEventPredicate)(const RogueEvent* event);

/* Subscription Structure (Phase 1.4.1) */
typedef struct RogueEventSubscription {
    uint32_t subscription_id;
    uint32_t subscriber_system_id;
    RogueEventTypeId event_type_id;
    RogueEventCallback callback;
    void* user_data;
    
    /* Conditional subscriptions (Phase 1.4.3) */
    RogueEventPredicate predicate;
    
    /* Subscription configuration (Phase 1.4.5 & 1.4.6) */
    RogueEventPriority min_priority;
    uint32_t rate_limit_per_second; /* 0 = no limit */
    uint64_t last_callback_time_us;
    uint32_t callback_count_this_second;
    
    /* Analytics (Phase 1.4.7) */
    uint64_t total_callbacks;
    uint64_t total_processing_time_us;
    uint64_t last_processing_time_us;
    
    /* Active flag */
    bool active;
    
    /* Internal linked list */
    struct RogueEventSubscription* next;
} RogueEventSubscription;

/* Event Bus Configuration (Phase 1.1.7) */
typedef struct {
    char name[ROGUE_EVENT_BUS_NAME_MAX];
    RogueEventProcessingStrategy processing_strategy;
    uint32_t max_queue_size;
    uint32_t max_processing_time_per_frame_us;
    uint32_t worker_thread_count;
    bool enable_persistence;
    bool enable_analytics;
    bool enable_replay_recording;
    uint32_t replay_history_depth;
} RogueEventBusConfig;

/* Main Event Bus Structure (Phase 1.1.1) */
typedef struct RogueEventBus {
    RogueEventBusConfig config;
    
    /* Event queue management */
    RogueEvent* event_queue_heads[ROGUE_EVENT_PRIORITY_COUNT];
    RogueEvent* event_queue_tails[ROGUE_EVENT_PRIORITY_COUNT];
    uint32_t queue_sizes[ROGUE_EVENT_PRIORITY_COUNT];
    uint32_t total_queue_size;
    
    /* Subscription management */
    RogueEventSubscription* subscriptions[ROGUE_MAX_EVENT_TYPES];
    uint32_t subscription_count;
    uint32_t next_subscription_id;
    
    /* Threading & synchronization */
    bool thread_safe_mode;
    void* mutex; /* Platform-specific mutex */
    
    /* Statistics & monitoring (Phase 1.1.5) */
    RogueEventBusStats stats;
    
    /* Event replay system (Phase 1.6.1) */
    RogueEvent** replay_history;
    uint32_t replay_history_size;
    uint32_t replay_history_index;
    bool replay_recording_enabled;
    
    /* Sequence number generation */
    uint64_t next_sequence_number;
    
    /* Initialization state */
    bool initialized;
} RogueEventBus;

/* ===== Core Event Bus API (Phase 1.1.1) ===== */

/**
 * Initialize the global event bus system
 * Thread-safe initialization with configurable parameters
 */
bool rogue_event_bus_init(const RogueEventBusConfig* config);

/**
 * Shutdown the event bus system
 * Flushes all pending events and cleans up resources
 */
void rogue_event_bus_shutdown(void);

/**
 * Get the global event bus instance
 */
RogueEventBus* rogue_event_bus_get_instance(void);

/* ===== Event Publishing API (Phase 1.1.1) ===== */

/**
 * Publish an event to the bus
 * Returns true if event was queued successfully
 */
bool rogue_event_publish(RogueEventTypeId type_id, 
                        const RogueEventPayload* payload,
                        RogueEventPriority priority,
                        uint32_t source_system_id,
                        const char* source_name);

/**
 * Publish an event with deadline constraint (Phase 1.3.6)
 */
bool rogue_event_publish_with_deadline(RogueEventTypeId type_id,
                                      const RogueEventPayload* payload,
                                      RogueEventPriority priority,
                                      uint64_t deadline_us,
                                      uint32_t source_system_id,
                                      const char* source_name);

/**
 * Publish a batch of events atomically (Phase 1.3.4)
 */
bool rogue_event_publish_batch(const RogueEvent* events, uint32_t event_count);

/* ===== Event Subscription API (Phase 1.4.1) ===== */

/**
 * Subscribe to events of a specific type
 * Returns subscription ID or 0 on failure
 */
uint32_t rogue_event_subscribe(RogueEventTypeId type_id,
                              RogueEventCallback callback,
                              void* user_data,
                              uint32_t subscriber_system_id);

/**
 * Subscribe with conditional predicate (Phase 1.4.3)
 */
uint32_t rogue_event_subscribe_conditional(RogueEventTypeId type_id,
                                          RogueEventCallback callback,
                                          void* user_data,
                                          RogueEventPredicate predicate,
                                          uint32_t subscriber_system_id);

/**
 * Subscribe with rate limiting (Phase 1.4.6)
 */
uint32_t rogue_event_subscribe_rate_limited(RogueEventTypeId type_id,
                                           RogueEventCallback callback,
                                           void* user_data,
                                           uint32_t rate_limit_per_second,
                                           uint32_t subscriber_system_id);

/**
 * Unsubscribe from events
 */
bool rogue_event_unsubscribe(uint32_t subscription_id);

/**
 * Unsubscribe all subscriptions for a system (Phase 1.4.2)
 */
void rogue_event_unsubscribe_system(uint32_t system_id);

/* ===== Event Processing API (Phase 1.5.1 & 1.5.2) ===== */

/**
 * Process events synchronously
 * Processes up to max_events or until time budget exhausted
 */
uint32_t rogue_event_process_sync(uint32_t max_events, uint32_t time_budget_us);

/**
 * Process events asynchronously (requires worker threads)
 */
bool rogue_event_process_async(uint32_t worker_count);

/**
 * Process all events of a specific priority level
 */
uint32_t rogue_event_process_priority(RogueEventPriority priority, uint32_t time_budget_us);

/* ===== Event Bus Statistics & Monitoring (Phase 1.1.5 & 1.7) ===== */

/**
 * Get current event bus statistics
 */
const RogueEventBusStats* rogue_event_bus_get_stats(void);

/**
 * Reset event bus statistics
 */
void rogue_event_bus_reset_stats(void);

/**
 * Get current queue depth for priority level
 */
uint32_t rogue_event_bus_get_queue_depth(RogueEventPriority priority);

/**
 * Check if event bus is over capacity
 */
bool rogue_event_bus_is_overloaded(void);

/* ===== Event Replay & Debugging (Phase 1.6) ===== */

/**
 * Enable/disable event recording for replay
 */
void rogue_event_bus_set_replay_recording(bool enabled);

/**
 * Get recorded event history
 */
const RogueEvent** rogue_event_bus_get_replay_history(uint32_t* history_size);

/**
 * Replay events from history
 */
bool rogue_event_bus_replay_events(uint32_t start_index, uint32_t count);

/**
 * Clear replay history
 */
void rogue_event_bus_clear_replay_history(void);

/* ===== Event Type Registry (Phase 1.1.2) ===== */

/**
 * Register a new event type with name for debugging
 */
bool rogue_event_register_type(RogueEventTypeId type_id, const char* type_name);

/**
 * Get event type name for debugging
 */
const char* rogue_event_get_type_name(RogueEventTypeId type_id);

/* ===== Event Bus Configuration (Phase 1.1.7) ===== */

/**
 * Update event bus configuration hot-swap
 */
bool rogue_event_bus_update_config(const RogueEventBusConfig* new_config);

/**
 * Get current event bus configuration
 */
const RogueEventBusConfig* rogue_event_bus_get_config(void);

/* ===== Utility Functions ===== */

/**
 * Create default event bus configuration
 */
RogueEventBusConfig rogue_event_bus_create_default_config(const char* name);

/**
 * Get current timestamp in microseconds
 */
uint64_t rogue_event_get_timestamp_us(void);

/**
 * Validate event payload for type safety
 */
bool rogue_event_validate_payload(RogueEventTypeId type_id, const RogueEventPayload* payload);

#ifdef __cplusplus
}
#endif
