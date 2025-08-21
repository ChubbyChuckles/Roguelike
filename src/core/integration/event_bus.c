#include "core/integration/event_bus.h"
#include "core/integration/config_version.h" /* Phase 2.6 - Configuration Version Management */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define ROGUE_EVENT_MUTEX_TYPE CRITICAL_SECTION
#define rogue_event_mutex_init(m) InitializeCriticalSection((CRITICAL_SECTION*) m)
#define rogue_event_mutex_destroy(m) DeleteCriticalSection((CRITICAL_SECTION*) m)
#define rogue_event_mutex_lock(m) EnterCriticalSection((CRITICAL_SECTION*) m)
#define rogue_event_mutex_unlock(m) LeaveCriticalSection((CRITICAL_SECTION*) m)
#else
#include <pthread.h>
#include <sys/time.h>
#define ROGUE_EVENT_MUTEX_TYPE pthread_mutex_t
#define rogue_event_mutex_init(m) pthread_mutex_init((pthread_mutex_t*) m, NULL)
#define rogue_event_mutex_destroy(m) pthread_mutex_destroy((pthread_mutex_t*) m)
#define rogue_event_mutex_lock(m) pthread_mutex_lock((pthread_mutex_t*) m)
#define rogue_event_mutex_unlock(m) pthread_mutex_unlock((pthread_mutex_t*) m)
#endif

/* Global event bus instance */
static RogueEventBus g_event_bus = {0};

/* Event type name registry (Phase 1.1.2) */
static char g_event_type_names[ROGUE_MAX_EVENT_TYPES][64];
static bool g_event_type_registered[ROGUE_MAX_EVENT_TYPES];

/* Forward declarations */
static void event_bus_lock(void);
static void event_bus_unlock(void);
static RogueEvent* create_event(RogueEventTypeId type_id, const RogueEventPayload* payload,
                                RogueEventPriority priority, uint32_t source_system_id,
                                const char* source_name);
static void free_event(RogueEvent* event);
static void enqueue_event(RogueEvent* event);
static RogueEvent* dequeue_event(RogueEventPriority priority);
static void record_event_for_replay(const RogueEvent* event);
static void update_statistics_on_publish(void);
static void update_statistics_on_process(const RogueEvent* event, uint64_t processing_time_us);
static bool is_subscription_rate_limited(RogueEventSubscription* subscription);
static uint32_t hash_event_type(RogueEventTypeId type_id);

/* ===== Core Event Bus API Implementation ===== */

bool rogue_event_bus_init(const RogueEventBusConfig* config)
{
    if (g_event_bus.initialized)
    {
        ROGUE_LOG_WARN("Event bus already initialized");
        return true;
    }

    if (!config)
    {
        ROGUE_LOG_ERROR("Event bus configuration is NULL");
        return false;
    }

    /* Clear the event bus structure */
    memset(&g_event_bus, 0, sizeof(RogueEventBus));

    /* Copy configuration */
    g_event_bus.config = *config;

    /* Initialize mutex for thread safety */
    if (config->worker_thread_count > 0)
    {
        g_event_bus.thread_safe_mode = true;
        g_event_bus.mutex = malloc(sizeof(ROGUE_EVENT_MUTEX_TYPE));
        if (!g_event_bus.mutex)
        {
            ROGUE_LOG_ERROR("Failed to allocate mutex for event bus");
            return false;
        }
        rogue_event_mutex_init(g_event_bus.mutex);
    }

    /* Initialize replay history if enabled (Phase 1.6.1) */
    if (config->enable_replay_recording && config->replay_history_depth > 0)
    {
        g_event_bus.replay_history = malloc(sizeof(RogueEvent*) * config->replay_history_depth);
        if (!g_event_bus.replay_history)
        {
            ROGUE_LOG_ERROR("Failed to allocate replay history buffer");
            if (g_event_bus.mutex)
            {
                rogue_event_mutex_destroy(g_event_bus.mutex);
                free(g_event_bus.mutex);
            }
            return false;
        }
        memset(g_event_bus.replay_history, 0, sizeof(RogueEvent*) * config->replay_history_depth);
        g_event_bus.replay_recording_enabled = true;
    }

    /* Initialize statistics */
    memset(&g_event_bus.stats, 0, sizeof(RogueEventBusStats));

    /* Initialize sequence number */
    g_event_bus.next_sequence_number = 1;
    g_event_bus.next_subscription_id = 1;

    /* Initialize configuration version manager (Phase 2.6) */
    if (!rogue_config_version_init("./config"))
    {
        ROGUE_LOG_WARN("Failed to initialize configuration version manager, using fallback limits");
    }

    /* Clear event type registry */
    memset(g_event_type_registered, 0, sizeof(g_event_type_registered));

    /* Register core event types using safe registration (Phase 2.6) */
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_ENTITY_CREATED, "ENTITY_CREATED");
    rogue_event_register_type(ROGUE_EVENT_ENTITY_CREATED, "ENTITY_CREATED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_ENTITY_DESTROYED, "ENTITY_DESTROYED");
    rogue_event_register_type(ROGUE_EVENT_ENTITY_DESTROYED, "ENTITY_DESTROYED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_ENTITY_MODIFIED, "ENTITY_MODIFIED");
    rogue_event_register_type(ROGUE_EVENT_ENTITY_MODIFIED, "ENTITY_MODIFIED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_PLAYER_MOVED, "PLAYER_MOVED");
    rogue_event_register_type(ROGUE_EVENT_PLAYER_MOVED, "PLAYER_MOVED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_PLAYER_ATTACKED, "PLAYER_ATTACKED");
    rogue_event_register_type(ROGUE_EVENT_PLAYER_ATTACKED, "PLAYER_ATTACKED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_PLAYER_EQUIPPED, "PLAYER_EQUIPPED");
    rogue_event_register_type(ROGUE_EVENT_PLAYER_EQUIPPED, "PLAYER_EQUIPPED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_PLAYER_SKILLED, "PLAYER_SKILLED");
    rogue_event_register_type(ROGUE_EVENT_PLAYER_SKILLED, "PLAYER_SKILLED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_DAMAGE_DEALT, "DAMAGE_DEALT");
    rogue_event_register_type(ROGUE_EVENT_DAMAGE_DEALT, "DAMAGE_DEALT");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_DAMAGE_TAKEN, "DAMAGE_TAKEN");
    rogue_event_register_type(ROGUE_EVENT_DAMAGE_TAKEN, "DAMAGE_TAKEN");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_CRITICAL_HIT, "CRITICAL_HIT");
    rogue_event_register_type(ROGUE_EVENT_CRITICAL_HIT, "CRITICAL_HIT");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_STATUS_APPLIED, "STATUS_APPLIED");
    rogue_event_register_type(ROGUE_EVENT_STATUS_APPLIED, "STATUS_APPLIED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_XP_GAINED, "XP_GAINED");
    rogue_event_register_type(ROGUE_EVENT_XP_GAINED, "XP_GAINED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_LEVEL_UP, "LEVEL_UP");
    rogue_event_register_type(ROGUE_EVENT_LEVEL_UP, "LEVEL_UP");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_SKILL_UNLOCKED, "SKILL_UNLOCKED");
    rogue_event_register_type(ROGUE_EVENT_SKILL_UNLOCKED, "SKILL_UNLOCKED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_MASTERY_INCREASED, "MASTERY_INCREASED");
    rogue_event_register_type(ROGUE_EVENT_MASTERY_INCREASED, "MASTERY_INCREASED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_ITEM_DROPPED, "ITEM_DROPPED");
    rogue_event_register_type(ROGUE_EVENT_ITEM_DROPPED, "ITEM_DROPPED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_ITEM_PICKED_UP, "ITEM_PICKED_UP");
    rogue_event_register_type(ROGUE_EVENT_ITEM_PICKED_UP, "ITEM_PICKED_UP");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_TRADE_COMPLETED, "TRADE_COMPLETED");
    rogue_event_register_type(ROGUE_EVENT_TRADE_COMPLETED, "TRADE_COMPLETED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_CURRENCY_CHANGED, "CURRENCY_CHANGED");
    rogue_event_register_type(ROGUE_EVENT_CURRENCY_CHANGED, "CURRENCY_CHANGED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_AREA_ENTERED, "AREA_ENTERED");
    rogue_event_register_type(ROGUE_EVENT_AREA_ENTERED, "AREA_ENTERED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_AREA_EXITED, "AREA_EXITED");
    rogue_event_register_type(ROGUE_EVENT_AREA_EXITED, "AREA_EXITED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_RESOURCE_SPAWNED, "RESOURCE_SPAWNED");
    rogue_event_register_type(ROGUE_EVENT_RESOURCE_SPAWNED, "RESOURCE_SPAWNED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_STRUCTURE_GENERATED, "STRUCTURE_GENERATED");
    rogue_event_register_type(ROGUE_EVENT_STRUCTURE_GENERATED, "STRUCTURE_GENERATED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_CONFIG_RELOADED, "CONFIG_RELOADED");
    rogue_event_register_type(ROGUE_EVENT_CONFIG_RELOADED, "CONFIG_RELOADED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_SAVE_COMPLETED, "SAVE_COMPLETED");
    rogue_event_register_type(ROGUE_EVENT_SAVE_COMPLETED, "SAVE_COMPLETED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_ERROR_OCCURRED, "ERROR_OCCURRED");
    rogue_event_register_type(ROGUE_EVENT_ERROR_OCCURRED, "ERROR_OCCURRED");
    ROGUE_EVENT_REGISTER_SAFE(ROGUE_EVENT_PERFORMANCE_ALERT, "PERFORMANCE_ALERT");
    rogue_event_register_type(ROGUE_EVENT_PERFORMANCE_ALERT, "PERFORMANCE_ALERT");

    g_event_bus.initialized = true;

    ROGUE_LOG_INFO("Event bus '%s' initialized (Strategy: %d, Max Queue: %u, Threads: %u)",
                   config->name, (int) config->processing_strategy, config->max_queue_size,
                   config->worker_thread_count);

    return true;
}

void rogue_event_bus_shutdown(void)
{
    if (!g_event_bus.initialized)
    {
        return;
    }

    event_bus_lock();

    /* Process any remaining events */
    uint32_t remaining_events = 0;
    for (int priority = 0; priority < ROGUE_EVENT_PRIORITY_COUNT; priority++)
    {
        while (g_event_bus.event_queue_heads[priority])
        {
            RogueEvent* event = dequeue_event((RogueEventPriority) priority);
            free_event(event);
            remaining_events++;
        }
    }

    if (remaining_events > 0)
    {
        ROGUE_LOG_WARN("Event bus shutdown with %u unprocessed events", remaining_events);
    }

    /* Clean up subscriptions */
    for (uint32_t type_idx = 0; type_idx < ROGUE_MAX_EVENT_TYPES; type_idx++)
    {
        RogueEventSubscription* sub = g_event_bus.subscriptions[type_idx];
        while (sub)
        {
            RogueEventSubscription* next = sub->next;
            free(sub);
            sub = next;
        }
        g_event_bus.subscriptions[type_idx] = NULL;
    }

    /* Clean up replay history */
    if (g_event_bus.replay_history)
    {
        for (uint32_t i = 0; i < g_event_bus.config.replay_history_depth; i++)
        {
            if (g_event_bus.replay_history[i])
            {
                free_event(g_event_bus.replay_history[i]);
            }
        }
        free(g_event_bus.replay_history);
        g_event_bus.replay_history = NULL;
    }

    event_bus_unlock();

    /* Clean up mutex */
    if (g_event_bus.mutex)
    {
        rogue_event_mutex_destroy(g_event_bus.mutex);
        free(g_event_bus.mutex);
        g_event_bus.mutex = NULL;
    }

    ROGUE_LOG_INFO("Event bus '%s' shutdown complete (Processed: %llu events)",
                   g_event_bus.config.name,
                   (unsigned long long) g_event_bus.stats.events_processed);

    /* Clear the structure */
    memset(&g_event_bus, 0, sizeof(RogueEventBus));
}

RogueEventBus* rogue_event_bus_get_instance(void)
{
    return g_event_bus.initialized ? &g_event_bus : NULL;
}

/* ===== Event Publishing API Implementation ===== */

bool rogue_event_publish(RogueEventTypeId type_id, const RogueEventPayload* payload,
                         RogueEventPriority priority, uint32_t source_system_id,
                         const char* source_name)
{
    return rogue_event_publish_with_deadline(type_id, payload, priority, 0, source_system_id,
                                             source_name);
}

bool rogue_event_publish_with_deadline(RogueEventTypeId type_id, const RogueEventPayload* payload,
                                       RogueEventPriority priority, uint64_t deadline_us,
                                       uint32_t source_system_id, const char* source_name)
{
    if (!g_event_bus.initialized)
    {
        ROGUE_LOG_ERROR("Event bus not initialized");
        return false;
    }

    if (!payload)
    {
        ROGUE_LOG_ERROR("Event payload is NULL");
        return false;
    }

    if (priority >= ROGUE_EVENT_PRIORITY_COUNT)
    {
        ROGUE_LOG_ERROR("Invalid event priority: %d", priority);
        return false;
    }

    event_bus_lock();

    /* Check queue capacity */
    if (g_event_bus.total_queue_size >= g_event_bus.config.max_queue_size)
    {
        event_bus_unlock();
        ROGUE_LOG_WARN("Event queue full, dropping event type %u", type_id);
        g_event_bus.stats.events_dropped++;
        return false;
    }

    /* Create event */
    RogueEvent* event = create_event(type_id, payload, priority, source_system_id, source_name);
    if (!event)
    {
        event_bus_unlock();
        ROGUE_LOG_ERROR("Failed to create event");
        return false;
    }

    /* Set deadline if specified */
    if (deadline_us > 0)
    {
        event->deadline_us = deadline_us;
    }

    /* Enqueue event */
    enqueue_event(event);

    /* Record for replay if enabled */
    if (g_event_bus.replay_recording_enabled)
    {
        record_event_for_replay(event);
    }

    /* Update statistics */
    update_statistics_on_publish();

    event_bus_unlock();

    ROGUE_LOG_DEBUG("Published event type %u from system %u (Queue depth: %u)", type_id,
                    source_system_id, g_event_bus.total_queue_size);

    return true;
}

bool rogue_event_publish_batch(const RogueEvent* events, uint32_t event_count)
{
    if (!g_event_bus.initialized)
    {
        ROGUE_LOG_ERROR("Event bus not initialized");
        return false;
    }

    if (!events || event_count == 0)
    {
        ROGUE_LOG_ERROR("Invalid batch parameters");
        return false;
    }

    event_bus_lock();

    /* Check if we have capacity for all events */
    if (g_event_bus.total_queue_size + event_count > g_event_bus.config.max_queue_size)
    {
        event_bus_unlock();
        ROGUE_LOG_WARN("Insufficient queue capacity for batch of %u events", event_count);
        return false;
    }

    /* Process all events in batch */
    uint32_t successfully_queued = 0;
    for (uint32_t i = 0; i < event_count; i++)
    {
        const RogueEvent* source_event = &events[i];

        /* Create copy of event */
        RogueEvent* event =
            create_event(source_event->type_id, &source_event->payload, source_event->priority,
                         source_event->source_system_id, source_event->source_name);
        if (event)
        {
            event->deadline_us = source_event->deadline_us;
            enqueue_event(event);

            if (g_event_bus.replay_recording_enabled)
            {
                record_event_for_replay(event);
            }

            successfully_queued++;
        }
    }

    /* Update statistics */
    g_event_bus.stats.events_published += successfully_queued;

    event_bus_unlock();

    ROGUE_LOG_DEBUG("Published batch: %u/%u events queued successfully", successfully_queued,
                    event_count);

    return successfully_queued == event_count;
}

/* ===== Event Subscription API Implementation ===== */

uint32_t rogue_event_subscribe(RogueEventTypeId type_id, RogueEventCallback callback,
                               void* user_data, uint32_t subscriber_system_id)
{
    return rogue_event_subscribe_conditional(type_id, callback, user_data, NULL,
                                             subscriber_system_id);
}

uint32_t rogue_event_subscribe_conditional(RogueEventTypeId type_id, RogueEventCallback callback,
                                           void* user_data, RogueEventPredicate predicate,
                                           uint32_t subscriber_system_id)
{
    if (!g_event_bus.initialized)
    {
        ROGUE_LOG_ERROR("Event bus not initialized");
        return 0;
    }

    if (!callback)
    {
        ROGUE_LOG_ERROR("Event callback is NULL");
        return 0;
    }

    event_bus_lock();

    /* Create subscription */
    RogueEventSubscription* subscription = malloc(sizeof(RogueEventSubscription));
    if (!subscription)
    {
        event_bus_unlock();
        ROGUE_LOG_ERROR("Failed to allocate subscription");
        return 0;
    }

    memset(subscription, 0, sizeof(RogueEventSubscription));
    subscription->subscription_id = g_event_bus.next_subscription_id++;
    subscription->subscriber_system_id = subscriber_system_id;
    subscription->event_type_id = type_id;
    subscription->callback = callback;
    subscription->user_data = user_data;
    subscription->predicate = predicate;
    subscription->min_priority = ROGUE_EVENT_PRIORITY_BACKGROUND; /* Accept all by default */
    subscription->rate_limit_per_second = 0;                      /* No limit by default */
    subscription->active = true;

    /* Add to subscription list for this event type */
    uint32_t type_hash = hash_event_type(type_id);
    subscription->next = g_event_bus.subscriptions[type_hash];
    g_event_bus.subscriptions[type_hash] = subscription;

    g_event_bus.subscription_count++;
    g_event_bus.stats.active_subscribers++;

    event_bus_unlock();

    ROGUE_LOG_DEBUG("System %u subscribed to event type %u (Subscription ID: %u)",
                    subscriber_system_id, type_id, subscription->subscription_id);

    return subscription->subscription_id;
}

uint32_t rogue_event_subscribe_rate_limited(RogueEventTypeId type_id, RogueEventCallback callback,
                                            void* user_data, uint32_t rate_limit_per_second,
                                            uint32_t subscriber_system_id)
{
    uint32_t subscription_id =
        rogue_event_subscribe_conditional(type_id, callback, user_data, NULL, subscriber_system_id);

    if (subscription_id > 0)
    {
        event_bus_lock();

        /* Find the subscription and set rate limit */
        uint32_t type_hash = hash_event_type(type_id);
        RogueEventSubscription* sub = g_event_bus.subscriptions[type_hash];
        while (sub)
        {
            if (sub->subscription_id == subscription_id)
            {
                sub->rate_limit_per_second = rate_limit_per_second;
                break;
            }
            sub = sub->next;
        }

        event_bus_unlock();

        ROGUE_LOG_DEBUG("Set rate limit %u/sec for subscription %u", rate_limit_per_second,
                        subscription_id);
    }

    return subscription_id;
}

bool rogue_event_unsubscribe(uint32_t subscription_id)
{
    if (!g_event_bus.initialized || subscription_id == 0)
    {
        return false;
    }

    event_bus_lock();

    /* Search all subscription lists */
    for (uint32_t type_idx = 0; type_idx < ROGUE_MAX_EVENT_TYPES; type_idx++)
    {
        RogueEventSubscription** current = &g_event_bus.subscriptions[type_idx];

        while (*current)
        {
            if ((*current)->subscription_id == subscription_id)
            {
                RogueEventSubscription* to_remove = *current;
                *current = to_remove->next;

                ROGUE_LOG_DEBUG("Unsubscribed subscription %u (System %u, Type %u)",
                                subscription_id, to_remove->subscriber_system_id,
                                to_remove->event_type_id);

                free(to_remove);
                g_event_bus.subscription_count--;
                g_event_bus.stats.active_subscribers--;

                event_bus_unlock();
                return true;
            }
            current = &(*current)->next;
        }
    }

    event_bus_unlock();
    ROGUE_LOG_WARN("Subscription %u not found for unsubscribe", subscription_id);
    return false;
}

void rogue_event_unsubscribe_system(uint32_t system_id)
{
    if (!g_event_bus.initialized)
    {
        return;
    }

    event_bus_lock();

    uint32_t removed_count = 0;

    /* Search all subscription lists */
    for (uint32_t type_idx = 0; type_idx < ROGUE_MAX_EVENT_TYPES; type_idx++)
    {
        RogueEventSubscription** current = &g_event_bus.subscriptions[type_idx];

        while (*current)
        {
            if ((*current)->subscriber_system_id == system_id)
            {
                RogueEventSubscription* to_remove = *current;
                *current = to_remove->next;

                free(to_remove);
                g_event_bus.subscription_count--;
                g_event_bus.stats.active_subscribers--;
                removed_count++;
            }
            else
            {
                current = &(*current)->next;
            }
        }
    }

    event_bus_unlock();

    if (removed_count > 0)
    {
        ROGUE_LOG_INFO("Unsubscribed %u subscriptions for system %u", removed_count, system_id);
    }
}

/* ===== Event Processing API Implementation ===== */

uint32_t rogue_event_process_sync(uint32_t max_events, uint32_t time_budget_us)
{
    if (!g_event_bus.initialized)
    {
        return 0;
    }

    uint64_t start_time = rogue_event_get_timestamp_us();
    uint32_t processed_count = 0;

    event_bus_lock();

    /* Process events by priority (Phase 1.3.1) */
    for (int priority = 0; priority < ROGUE_EVENT_PRIORITY_COUNT && processed_count < max_events;
         priority++)
    {
        while (g_event_bus.event_queue_heads[priority] && processed_count < max_events)
        {
            uint64_t current_time = rogue_event_get_timestamp_us();

            /* Check time budget */
            if (time_budget_us > 0 && (current_time - start_time) >= time_budget_us)
            {
                break;
            }

            RogueEvent* event = dequeue_event((RogueEventPriority) priority);
            if (!event)
                break;

            /* Check deadline (Phase 1.3.6) */
            if (event->deadline_us > 0 && current_time > event->deadline_us)
            {
                ROGUE_LOG_WARN("Event type %u missed deadline by %llu microseconds", event->type_id,
                               (unsigned long long) (current_time - event->deadline_us));
                free_event(event);
                g_event_bus.stats.events_failed++;
                continue;
            }

            event_bus_unlock();

            /* Process event with all subscribers */
            uint64_t process_start = rogue_event_get_timestamp_us();
            bool event_processed = false;

            uint32_t type_hash = hash_event_type(event->type_id);
            RogueEventSubscription* sub = g_event_bus.subscriptions[type_hash];

            while (sub)
            {
                if (sub->active && sub->event_type_id == event->type_id)
                {
                    /* Check priority filter */
                    if (event->priority > sub->min_priority)
                    {
                        sub = sub->next;
                        continue;
                    }

                    /* Check rate limiting */
                    if (is_subscription_rate_limited(sub))
                    {
                        sub = sub->next;
                        continue;
                    }

                    /* Check predicate if present */
                    if (sub->predicate && !sub->predicate(event))
                    {
                        sub = sub->next;
                        continue;
                    }

                    /* Call subscriber */
                    uint64_t callback_start = rogue_event_get_timestamp_us();
                    bool callback_result = sub->callback(event, sub->user_data);
                    uint64_t callback_end = rogue_event_get_timestamp_us();

                    /* Update subscription statistics */
                    sub->total_callbacks++;
                    sub->total_processing_time_us += (callback_end - callback_start);
                    sub->last_processing_time_us = callback_end - callback_start;
                    sub->last_callback_time_us = callback_end;

                    if (callback_result)
                    {
                        event_processed = true;
                    }
                }

                sub = sub->next;
            }

            uint64_t process_end = rogue_event_get_timestamp_us();

            event_bus_lock();

            /* Update statistics */
            update_statistics_on_process(event, process_end - process_start);

            if (event_processed)
            {
                event->processed = true;
                processed_count++;
            }
            else
            {
                /* No subscriber processed the event successfully */
                if (event->retry_count < event->max_retries)
                {
                    event->retry_count++;
                    enqueue_event(event); /* Re-queue for retry */
                    ROGUE_LOG_DEBUG("Re-queued event type %u for retry (%u/%u)", event->type_id,
                                    event->retry_count, event->max_retries);
                }
                else
                {
                    ROGUE_LOG_WARN("Event type %u failed after %u retries", event->type_id,
                                   event->max_retries);
                    free_event(event);
                    g_event_bus.stats.events_failed++;
                }
                continue;
            }

            free_event(event);
        }
    }

    event_bus_unlock();

    if (processed_count > 0)
    {
        ROGUE_LOG_DEBUG("Processed %u events in %llu microseconds", processed_count,
                        (unsigned long long) (rogue_event_get_timestamp_us() - start_time));
    }

    return processed_count;
}

uint32_t rogue_event_process_priority(RogueEventPriority priority, uint32_t time_budget_us)
{
    if (!g_event_bus.initialized || priority >= ROGUE_EVENT_PRIORITY_COUNT)
    {
        return 0;
    }

    uint64_t start_time = rogue_event_get_timestamp_us();
    uint32_t processed_count = 0;

    event_bus_lock();

    while (g_event_bus.event_queue_heads[priority])
    {
        uint64_t current_time = rogue_event_get_timestamp_us();

        /* Check time budget */
        if (time_budget_us > 0 && (current_time - start_time) >= time_budget_us)
        {
            break;
        }

        RogueEvent* event = dequeue_event(priority);
        if (!event)
            break;

        event_bus_unlock();

        /* Process event (simplified version for priority-specific processing) */
        uint64_t process_start = rogue_event_get_timestamp_us();
        bool event_processed = false;

        uint32_t type_hash = hash_event_type(event->type_id);
        RogueEventSubscription* sub = g_event_bus.subscriptions[type_hash];

        while (sub)
        {
            if (sub->active && sub->event_type_id == event->type_id &&
                event->priority <= sub->min_priority)
            {

                if (!is_subscription_rate_limited(sub) &&
                    (!sub->predicate || sub->predicate(event)))
                {

                    if (sub->callback(event, sub->user_data))
                    {
                        event_processed = true;
                    }
                }
            }
            sub = sub->next;
        }

        uint64_t process_end = rogue_event_get_timestamp_us();

        event_bus_lock();

        update_statistics_on_process(event, process_end - process_start);

        if (event_processed)
        {
            processed_count++;
        }

        free_event(event);
    }

    event_bus_unlock();

    return processed_count;
}

bool rogue_event_process_async(uint32_t worker_count)
{
    /* Async processing would require thread management - for now, stub implementation */
    (void) worker_count; /* Suppress unused parameter warning */
    ROGUE_LOG_WARN("Async event processing not yet implemented");
    return false;
}

/* ===== Statistics & Monitoring Implementation ===== */

const RogueEventBusStats* rogue_event_bus_get_stats(void)
{
    return g_event_bus.initialized ? &g_event_bus.stats : NULL;
}

void rogue_event_bus_reset_stats(void)
{
    if (!g_event_bus.initialized)
    {
        return;
    }

    event_bus_lock();
    memset(&g_event_bus.stats, 0, sizeof(RogueEventBusStats));
    g_event_bus.stats.active_subscribers = g_event_bus.subscription_count;
    event_bus_unlock();

    ROGUE_LOG_INFO("Event bus statistics reset");
}

uint32_t rogue_event_bus_get_queue_depth(RogueEventPriority priority)
{
    if (!g_event_bus.initialized || priority >= ROGUE_EVENT_PRIORITY_COUNT)
    {
        return 0;
    }

    return g_event_bus.queue_sizes[priority];
}

bool rogue_event_bus_is_overloaded(void)
{
    if (!g_event_bus.initialized)
    {
        return false;
    }

    return g_event_bus.total_queue_size >= (g_event_bus.config.max_queue_size * 0.9f);
}

/* ===== Event Type Registry Implementation ===== */

bool rogue_event_register_type(RogueEventTypeId type_id, const char* type_name)
{
    if (type_id >= ROGUE_MAX_EVENT_TYPES)
    {
        ROGUE_LOG_ERROR("Event type ID %u exceeds maximum %u", type_id, ROGUE_MAX_EVENT_TYPES);
        return false;
    }

    if (!type_name)
    {
        ROGUE_LOG_ERROR("Event type name is NULL");
        return false;
    }

    if (g_event_type_registered[type_id])
    {
        ROGUE_LOG_WARN("Event type %u already registered as '%s'", type_id,
                       g_event_type_names[type_id]);
        return true;
    }

#ifdef _MSC_VER
    strncpy_s(g_event_type_names[type_id], sizeof(g_event_type_names[type_id]), type_name,
              _TRUNCATE);
#else
    strncpy(g_event_type_names[type_id], type_name, sizeof(g_event_type_names[type_id]) - 1);
    g_event_type_names[type_id][sizeof(g_event_type_names[type_id]) - 1] = '\0';
#endif

    g_event_type_registered[type_id] = true;

    ROGUE_LOG_DEBUG("Registered event type %u: '%s'", type_id, type_name);
    return true;
}

const char* rogue_event_get_type_name(RogueEventTypeId type_id)
{
    if (type_id >= ROGUE_MAX_EVENT_TYPES || !g_event_type_registered[type_id])
    {
        return "UNKNOWN_EVENT_TYPE";
    }

    return g_event_type_names[type_id];
}

/* ===== Configuration & Replay Implementation ===== */

bool rogue_event_bus_update_config(const RogueEventBusConfig* new_config)
{
    if (!g_event_bus.initialized || !new_config)
    {
        return false;
    }

    event_bus_lock();

    /* Update safe configuration options */
    g_event_bus.config.processing_strategy = new_config->processing_strategy;
    g_event_bus.config.max_processing_time_per_frame_us =
        new_config->max_processing_time_per_frame_us;
    g_event_bus.config.enable_analytics = new_config->enable_analytics;

    /* Note: Some config changes (like thread count, queue size) would require restart */

    event_bus_unlock();

    ROGUE_LOG_INFO("Event bus configuration updated");
    return true;
}

const RogueEventBusConfig* rogue_event_bus_get_config(void)
{
    return g_event_bus.initialized ? &g_event_bus.config : NULL;
}

void rogue_event_bus_set_replay_recording(bool enabled)
{
    if (!g_event_bus.initialized)
    {
        return;
    }

    event_bus_lock();
    g_event_bus.replay_recording_enabled = enabled && (g_event_bus.replay_history != NULL);
    event_bus_unlock();

    ROGUE_LOG_INFO("Event replay recording %s", enabled ? "enabled" : "disabled");
}

const RogueEvent** rogue_event_bus_get_replay_history(uint32_t* history_size)
{
    if (!g_event_bus.initialized || !history_size)
    {
        return NULL;
    }

    *history_size = g_event_bus.replay_history_size;
    return (const RogueEvent**) g_event_bus.replay_history;
}

bool rogue_event_bus_replay_events(uint32_t start_index, uint32_t count)
{
    /* Replay implementation would be complex - stub for now */
    (void) start_index; /* Suppress unused parameter warning */
    (void) count;       /* Suppress unused parameter warning */
    ROGUE_LOG_WARN("Event replay not yet implemented");
    return false;
}

void rogue_event_bus_clear_replay_history(void)
{
    if (!g_event_bus.initialized || !g_event_bus.replay_history)
    {
        return;
    }

    event_bus_lock();

    for (uint32_t i = 0; i < g_event_bus.replay_history_size; i++)
    {
        if (g_event_bus.replay_history[i])
        {
            free_event(g_event_bus.replay_history[i]);
            g_event_bus.replay_history[i] = NULL;
        }
    }

    g_event_bus.replay_history_size = 0;
    g_event_bus.replay_history_index = 0;

    event_bus_unlock();

    ROGUE_LOG_INFO("Event replay history cleared");
}

/* ===== Utility Functions Implementation ===== */

RogueEventBusConfig rogue_event_bus_create_default_config(const char* name)
{
    RogueEventBusConfig config = {0};

    if (name)
    {
#ifdef _MSC_VER
        strncpy_s(config.name, sizeof(config.name), name, _TRUNCATE);
#else
        strncpy(config.name, name, sizeof(config.name) - 1);
        config.name[sizeof(config.name) - 1] = '\0';
#endif
    }
    else
    {
#ifdef _MSC_VER
        strcpy_s(config.name, sizeof(config.name), "DefaultEventBus");
#else
        strcpy(config.name, "DefaultEventBus");
#endif
    }

    config.processing_strategy = ROGUE_EVENT_STRATEGY_PRIORITY;
    config.max_queue_size = ROGUE_MAX_EVENT_QUEUE_SIZE;
    config.max_processing_time_per_frame_us = 5000; /* 5ms per frame */
    config.worker_thread_count = 0;                 /* Synchronous by default */
    config.enable_persistence = false;
    config.enable_analytics = true;
    config.enable_replay_recording = true;
    config.replay_history_depth = 1000;

    return config;
}

uint64_t rogue_event_get_timestamp_us(void)
{
#ifdef _WIN32
    static LARGE_INTEGER frequency = {0};
    if (frequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&frequency);
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t) ((counter.QuadPart * 1000000LL) / frequency.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * 1000000ULL + (uint64_t) tv.tv_usec;
#endif
}

bool rogue_event_validate_payload(RogueEventTypeId type_id, const RogueEventPayload* payload)
{
    /* Basic validation - could be extended with type-specific checks */
    (void) type_id; /* Suppress unused parameter warning */
    return payload != NULL;
}

/* ===== Internal Helper Functions Implementation ===== */

static void event_bus_lock(void)
{
    if (g_event_bus.thread_safe_mode && g_event_bus.mutex)
    {
        rogue_event_mutex_lock(g_event_bus.mutex);
    }
}

static void event_bus_unlock(void)
{
    if (g_event_bus.thread_safe_mode && g_event_bus.mutex)
    {
        rogue_event_mutex_unlock(g_event_bus.mutex);
    }
}

static RogueEvent* create_event(RogueEventTypeId type_id, const RogueEventPayload* payload,
                                RogueEventPriority priority, uint32_t source_system_id,
                                const char* source_name)
{
    RogueEvent* event = malloc(sizeof(RogueEvent));
    if (!event)
    {
        return NULL;
    }

    memset(event, 0, sizeof(RogueEvent));

    event->type_id = type_id;
    event->priority = priority;
    event->payload = *payload;
    event->source_system_id = source_system_id;
    event->timestamp_us = rogue_event_get_timestamp_us();
    event->sequence_number = g_event_bus.next_sequence_number++;
    event->max_retries = 3; /* Default retry count */

    if (source_name)
    {
#ifdef _MSC_VER
        strncpy_s(event->source_name, sizeof(event->source_name), source_name, _TRUNCATE);
#else
        strncpy(event->source_name, source_name, sizeof(event->source_name) - 1);
        event->source_name[sizeof(event->source_name) - 1] = '\0';
#endif
    }

    return event;
}

static void free_event(RogueEvent* event)
{
    if (event)
    {
        free(event);
    }
}

static void enqueue_event(RogueEvent* event)
{
    if (!event || event->priority >= ROGUE_EVENT_PRIORITY_COUNT)
    {
        return;
    }

    event->next = NULL;

    if (g_event_bus.event_queue_tails[event->priority])
    {
        g_event_bus.event_queue_tails[event->priority]->next = event;
    }
    else
    {
        g_event_bus.event_queue_heads[event->priority] = event;
    }

    g_event_bus.event_queue_tails[event->priority] = event;
    g_event_bus.queue_sizes[event->priority]++;
    g_event_bus.total_queue_size++;

    /* Update max queue depth statistic */
    if (g_event_bus.total_queue_size > g_event_bus.stats.max_queue_depth_reached)
    {
        g_event_bus.stats.max_queue_depth_reached = g_event_bus.total_queue_size;
    }
}

static RogueEvent* dequeue_event(RogueEventPriority priority)
{
    if (priority >= ROGUE_EVENT_PRIORITY_COUNT || !g_event_bus.event_queue_heads[priority])
    {
        return NULL;
    }

    RogueEvent* event = g_event_bus.event_queue_heads[priority];
    g_event_bus.event_queue_heads[priority] = event->next;

    if (!g_event_bus.event_queue_heads[priority])
    {
        g_event_bus.event_queue_tails[priority] = NULL;
    }

    g_event_bus.queue_sizes[priority]--;
    g_event_bus.total_queue_size--;
    g_event_bus.stats.current_queue_depth = g_event_bus.total_queue_size;

    event->next = NULL;
    return event;
}

static void record_event_for_replay(const RogueEvent* event)
{
    if (!g_event_bus.replay_history || !event)
    {
        return;
    }

    /* Make a copy of the event for replay storage */
    RogueEvent* replay_event = malloc(sizeof(RogueEvent));
    if (!replay_event)
    {
        return;
    }

    *replay_event = *event;
    replay_event->next = NULL;

    /* Free old event if history buffer is full */
    if (g_event_bus.replay_history[g_event_bus.replay_history_index])
    {
        free_event(g_event_bus.replay_history[g_event_bus.replay_history_index]);
    }

    g_event_bus.replay_history[g_event_bus.replay_history_index] = replay_event;
    g_event_bus.replay_history_index =
        (g_event_bus.replay_history_index + 1) % g_event_bus.config.replay_history_depth;

    if (g_event_bus.replay_history_size < g_event_bus.config.replay_history_depth)
    {
        g_event_bus.replay_history_size++;
    }
}

static void update_statistics_on_publish(void) { g_event_bus.stats.events_published++; }

static void update_statistics_on_process(const RogueEvent* event, uint64_t processing_time_us)
{
    g_event_bus.stats.events_processed++;
    g_event_bus.stats.total_processing_time_us += processing_time_us;

    /* Update latency statistics */
    uint64_t latency = rogue_event_get_timestamp_us() - event->timestamp_us;
    if (latency > g_event_bus.stats.peak_latency_us)
    {
        g_event_bus.stats.peak_latency_us = (double) latency;
    }

    /* Update average latency */
    if (g_event_bus.stats.events_processed > 0)
    {
        g_event_bus.stats.average_latency_us =
            (g_event_bus.stats.average_latency_us *
                 (double) (g_event_bus.stats.events_processed - 1) +
             (double) latency) /
            (double) g_event_bus.stats.events_processed;
    }
}

static bool is_subscription_rate_limited(RogueEventSubscription* subscription)
{
    if (!subscription || subscription->rate_limit_per_second == 0)
    {
        return false;
    }

    uint64_t current_time = rogue_event_get_timestamp_us();
    uint64_t one_second_us = 1000000ULL;

    /* Reset counter if more than a second has passed */
    if (current_time - subscription->last_callback_time_us >= one_second_us)
    {
        subscription->callback_count_this_second = 0;
    }

    /* Check if rate limit exceeded */
    if (subscription->callback_count_this_second >= subscription->rate_limit_per_second)
    {
        return true;
    }

    /* Update callback count */
    subscription->callback_count_this_second++;
    return false;
}

static uint32_t hash_event_type(RogueEventTypeId type_id)
{
    /* Simple hash function - could be improved */
    return type_id % ROGUE_MAX_EVENT_TYPES;
}
