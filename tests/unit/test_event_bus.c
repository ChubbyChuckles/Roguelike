#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/integration/event_bus.h"

/* Test counters and flags */
static uint32_t g_test_callback_count = 0;
static uint32_t g_test_last_event_type = 0;
static uint32_t g_test_last_source_system = 0;
static bool g_test_callback_called = false;
static bool g_test_predicate_result = true;

/* Test callback functions */
static bool test_event_callback_basic(const RogueEvent* event, void* user_data)
{
    g_test_callback_called = true;
    g_test_callback_count++;
    g_test_last_event_type = event->type_id;
    g_test_last_source_system = event->source_system_id;

    /* Verify user_data if provided */
    if (user_data)
    {
        uint32_t* expected_value = (uint32_t*) user_data;
        assert(*expected_value == 0xDEADBEEF);
    }

    return true;
}

static bool test_event_callback_failing(const RogueEvent* event, void* user_data)
{
    g_test_callback_called = true;
    g_test_callback_count++;
    return false; /* Simulate processing failure */
}

static bool test_event_predicate(const RogueEvent* event) { return g_test_predicate_result; }

/* Reset test state */
static void reset_test_state(void)
{
    g_test_callback_count = 0;
    g_test_last_event_type = 0;
    g_test_last_source_system = 0;
    g_test_callback_called = false;
    g_test_predicate_result = true;
}

/* Create test payload */
static RogueEventPayload create_test_payload(uint32_t test_value)
{
    RogueEventPayload payload = {0};
    payload.entity.entity_id = test_value;
    payload.entity.entity_type = test_value * 2;
    return payload;
}

/* ===== Event Bus Initialization Tests ===== */

static bool test_event_bus_initialization(void)
{
    printf("Testing event bus initialization...\n");

    /* Test default configuration creation */
    RogueEventBusConfig config = rogue_event_bus_create_default_config("TestBus");
    assert(strcmp(config.name, "TestBus") == 0);
    assert(config.processing_strategy == ROGUE_EVENT_STRATEGY_PRIORITY);
    assert(config.max_queue_size == ROGUE_MAX_EVENT_QUEUE_SIZE);
    assert(config.enable_analytics == true);
    assert(config.enable_replay_recording == true);

    /* Test initialization */
    assert(rogue_event_bus_init(&config) == true);

    RogueEventBus* bus = rogue_event_bus_get_instance();
    assert(bus != NULL);

    /* Test double initialization */
    assert(rogue_event_bus_init(&config) == true); /* Should succeed (already initialized) */

    /* Test statistics access */
    const RogueEventBusStats* stats = rogue_event_bus_get_stats();
    assert(stats != NULL);
    assert(stats->events_published == 0);
    assert(stats->events_processed == 0);

    rogue_event_bus_shutdown();
    printf("  ✓ Event bus initialization passed\n");
    return true;
}

static bool test_event_bus_configuration(void)
{
    printf("Testing event bus configuration...\n");

    /* Test custom configuration */
    RogueEventBusConfig config = rogue_event_bus_create_default_config("ConfigTest");
    config.max_queue_size = 1000;
    config.max_processing_time_per_frame_us = 10000;
    config.enable_analytics = false;
    config.replay_history_depth = 500;

    assert(rogue_event_bus_init(&config) == true);

    const RogueEventBusConfig* current_config = rogue_event_bus_get_config();
    assert(current_config != NULL);
    assert(current_config->max_queue_size == 1000);
    assert(current_config->enable_analytics == false);

    /* Test configuration update */
    RogueEventBusConfig new_config = *current_config;
    new_config.enable_analytics = true;
    new_config.max_processing_time_per_frame_us = 15000;

    assert(rogue_event_bus_update_config(&new_config) == true);

    current_config = rogue_event_bus_get_config();
    assert(current_config->enable_analytics == true);
    assert(current_config->max_processing_time_per_frame_us == 15000);

    rogue_event_bus_shutdown();
    printf("  ✓ Event bus configuration passed\n");
    return true;
}

/* ===== Event Type Registry Tests ===== */

static bool test_event_type_registry(void)
{
    printf("Testing event type registry...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("RegistryTest");
    assert(rogue_event_bus_init(&config) == true);

    /* Test pre-registered core types */
    const char* name = rogue_event_get_type_name(ROGUE_EVENT_ENTITY_CREATED);
    assert(strcmp(name, "ENTITY_CREATED") == 0);

    name = rogue_event_get_type_name(ROGUE_EVENT_PLAYER_MOVED);
    assert(strcmp(name, "PLAYER_MOVED") == 0);

    name = rogue_event_get_type_name(ROGUE_EVENT_DAMAGE_DEALT);
    assert(strcmp(name, "DAMAGE_DEALT") == 0);

    /* Test custom type registration */
    const uint32_t CUSTOM_EVENT_TYPE = 0x9001; /* Updated to use reserved test range */
    assert(rogue_event_register_type(CUSTOM_EVENT_TYPE, "CUSTOM_TEST_EVENT") == true);

    name = rogue_event_get_type_name(CUSTOM_EVENT_TYPE);
    assert(strcmp(name, "CUSTOM_TEST_EVENT") == 0);

    /* Test unknown type */
    name = rogue_event_get_type_name(0xFFFF);
    assert(strcmp(name, "UNKNOWN_EVENT_TYPE") == 0);

    /* Test double registration */
    assert(rogue_event_register_type(CUSTOM_EVENT_TYPE, "DIFFERENT_NAME") == true);
    name = rogue_event_get_type_name(CUSTOM_EVENT_TYPE);
    assert(strcmp(name, "CUSTOM_TEST_EVENT") == 0); /* Should keep original name */

    rogue_event_bus_shutdown();
    printf("  ✓ Event type registry passed\n");
    return true;
}

/* ===== Event Publishing Tests ===== */

static bool test_event_publishing(void)
{
    printf("Testing event publishing...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("PublishTest");
    assert(rogue_event_bus_init(&config) == true);

    reset_test_state();

    /* Test basic event publishing */
    RogueEventPayload payload = create_test_payload(123);
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_NORMAL, 1,
                               "TestSystem") == true);

    const RogueEventBusStats* stats = rogue_event_bus_get_stats();
    assert(stats->events_published == 1);
    assert(stats->current_queue_depth > 0);

    /* Test publishing with deadline */
    uint64_t deadline = rogue_event_get_timestamp_us() + 5000000; /* 5 seconds */
    assert(rogue_event_publish_with_deadline(ROGUE_EVENT_PLAYER_MOVED, &payload,
                                             ROGUE_EVENT_PRIORITY_HIGH, deadline, 2,
                                             "PlayerSystem") == true);

    assert(stats->events_published == 2);

    /* Test queue depth by priority */
    uint32_t normal_depth = rogue_event_bus_get_queue_depth(ROGUE_EVENT_PRIORITY_NORMAL);
    uint32_t high_depth = rogue_event_bus_get_queue_depth(ROGUE_EVENT_PRIORITY_HIGH);
    assert(normal_depth == 1);
    assert(high_depth == 1);

    /* Test null payload rejection */
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, NULL, ROGUE_EVENT_PRIORITY_NORMAL, 1,
                               "TestSystem") == false);

    /* Test invalid priority rejection */
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, (RogueEventPriority) 999, 1,
                               "TestSystem") == false);

    rogue_event_bus_shutdown();
    printf("  ✓ Event publishing passed\n");
    return true;
}

static bool test_event_batch_publishing(void)
{
    printf("Testing event batch publishing...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("BatchTest");
    assert(rogue_event_bus_init(&config) == true);

    /* Create batch of events */
#define BATCH_SIZE 5
    RogueEvent events[BATCH_SIZE];

    for (uint32_t i = 0; i < BATCH_SIZE; i++)
    {
        memset(&events[i], 0, sizeof(RogueEvent));
        events[i].type_id = ROGUE_EVENT_ENTITY_CREATED;
        events[i].priority = ROGUE_EVENT_PRIORITY_NORMAL;
        events[i].payload = create_test_payload(i);
        events[i].source_system_id = 1;
        strcpy(events[i].source_name, "BatchTest");
    }

    assert(rogue_event_publish_batch(events, BATCH_SIZE) == true);

    const RogueEventBusStats* stats = rogue_event_bus_get_stats();
    assert(stats->events_published == BATCH_SIZE);

    uint32_t queue_depth = rogue_event_bus_get_queue_depth(ROGUE_EVENT_PRIORITY_NORMAL);
    assert(queue_depth == BATCH_SIZE);

    /* Test empty batch */
    assert(rogue_event_publish_batch(NULL, 0) == false);
    assert(rogue_event_publish_batch(events, 0) == false);

    rogue_event_bus_shutdown();
    printf("  ✓ Event batch publishing passed\n");
    return true;
}

/* ===== Event Subscription Tests ===== */

static bool test_event_subscription_basic(void)
{
    printf("Testing basic event subscription...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("SubTest");
    assert(rogue_event_bus_init(&config) == true);

    reset_test_state();

    uint32_t test_value = 0xDEADBEEF;

    /* Test basic subscription */
    uint32_t sub_id = rogue_event_subscribe(ROGUE_EVENT_ENTITY_CREATED, test_event_callback_basic,
                                            &test_value, 1);
    assert(sub_id != 0);

    const RogueEventBusStats* stats = rogue_event_bus_get_stats();
    assert(stats->active_subscribers == 1);

    /* Test event processing */
    RogueEventPayload payload = create_test_payload(456);
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_NORMAL, 2,
                               "Publisher") == true);

    assert(rogue_event_process_sync(10, 1000000) == 1);

    assert(g_test_callback_called == true);
    assert(g_test_callback_count == 1);
    assert(g_test_last_event_type == ROGUE_EVENT_ENTITY_CREATED);
    assert(g_test_last_source_system == 2);

    /* Test unsubscription */
    assert(rogue_event_unsubscribe(sub_id) == true);
    assert(rogue_event_unsubscribe(sub_id) == false); /* Should fail second time */

    stats = rogue_event_bus_get_stats();
    assert(stats->active_subscribers == 0);

    rogue_event_bus_shutdown();
    printf("  ✓ Basic event subscription passed\n");
    return true;
}

static bool test_event_subscription_conditional(void)
{
    printf("Testing conditional event subscription...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("ConditionalTest");
    assert(rogue_event_bus_init(&config) == true);

    reset_test_state();

    /* Subscribe with predicate */
    uint32_t sub_id = rogue_event_subscribe_conditional(
        ROGUE_EVENT_ENTITY_CREATED, test_event_callback_basic, NULL, test_event_predicate, 1);
    assert(sub_id != 0);

    /* Test with predicate returning true */
    g_test_predicate_result = true;
    RogueEventPayload payload = create_test_payload(789);
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_NORMAL, 1,
                               "Test") == true);

    assert(rogue_event_process_sync(10, 1000000) == 1);
    assert(g_test_callback_count == 1);

    reset_test_state();

    /* Test with predicate returning false */
    g_test_predicate_result = false;
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_NORMAL, 1,
                               "Test") == true);

    assert(rogue_event_process_sync(10, 1000000) == 1);
    assert(g_test_callback_count == 0); /* Should not be called */

    rogue_event_unsubscribe(sub_id);
    rogue_event_bus_shutdown();
    printf("  ✓ Conditional event subscription passed\n");
    return true;
}

static bool test_event_subscription_rate_limiting(void)
{
    printf("Testing rate-limited event subscription...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("RateLimitTest");
    assert(rogue_event_bus_init(&config) == true);

    reset_test_state();

    /* Subscribe with rate limit of 2 events per second */
    uint32_t sub_id = rogue_event_subscribe_rate_limited(ROGUE_EVENT_ENTITY_CREATED,
                                                         test_event_callback_basic, NULL, 2, 1);
    assert(sub_id != 0);

    /* Publish 5 events rapidly */
    RogueEventPayload payload = create_test_payload(100);
    for (int i = 0; i < 5; i++)
    {
        assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload,
                                   ROGUE_EVENT_PRIORITY_NORMAL, 1, "Test") == true);
    }

    /* Process all events - should only process first 2 due to rate limiting */
    uint32_t processed = rogue_event_process_sync(10, 1000000);
    assert(processed == 5);             /* All events processed, but only 2 callbacks made */
    assert(g_test_callback_count == 2); /* Rate limited to 2 */

    rogue_event_unsubscribe(sub_id);
    rogue_event_bus_shutdown();
    printf("  ✓ Rate-limited event subscription passed\n");
    return true;
}

static bool test_system_unsubscribe(void)
{
    printf("Testing system-wide unsubscription...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("SystemUnsubTest");
    assert(rogue_event_bus_init(&config) == true);

    /* Subscribe to multiple events with the same system ID */
    uint32_t sub1 =
        rogue_event_subscribe(ROGUE_EVENT_ENTITY_CREATED, test_event_callback_basic, NULL, 100);
    uint32_t sub2 =
        rogue_event_subscribe(ROGUE_EVENT_PLAYER_MOVED, test_event_callback_basic, NULL, 100);
    uint32_t sub3 =
        rogue_event_subscribe(ROGUE_EVENT_DAMAGE_DEALT, test_event_callback_basic, NULL, 101);

    assert(sub1 != 0 && sub2 != 0 && sub3 != 0);

    const RogueEventBusStats* stats = rogue_event_bus_get_stats();
    assert(stats->active_subscribers == 3);

    /* Unsubscribe all subscriptions for system 100 */
    rogue_event_unsubscribe_system(100);

    stats = rogue_event_bus_get_stats();
    assert(stats->active_subscribers == 1); /* Only system 101 subscription should remain */

    /* Verify system 101 subscription still works */
    reset_test_state();
    RogueEventPayload payload = create_test_payload(200);
    assert(rogue_event_publish(ROGUE_EVENT_DAMAGE_DEALT, &payload, ROGUE_EVENT_PRIORITY_NORMAL, 1,
                               "Test") == true);

    assert(rogue_event_process_sync(10, 1000000) == 1);
    assert(g_test_callback_count == 1);

    rogue_event_unsubscribe(sub3);
    rogue_event_bus_shutdown();
    printf("  ✓ System-wide unsubscription passed\n");
    return true;
}

/* ===== Event Processing Tests ===== */

static bool test_event_processing_priority(void)
{
    printf("Testing priority-based event processing...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("PriorityTest");
    config.processing_strategy = ROGUE_EVENT_STRATEGY_PRIORITY;
    assert(rogue_event_bus_init(&config) == true);

    reset_test_state();

    /* Subscribe to entity events */
    uint32_t sub_id =
        rogue_event_subscribe(ROGUE_EVENT_ENTITY_CREATED, test_event_callback_basic, NULL, 1);
    assert(sub_id != 0);

    RogueEventPayload payload = create_test_payload(300);

    /* Publish events with different priorities */
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_LOW, 1,
                               "Low") == true);
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_CRITICAL,
                               1, "Critical") == true);
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_HIGH, 1,
                               "High") == true);
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_NORMAL, 1,
                               "Normal") == true);

    /* Process all events - should process in priority order */
    uint32_t processed = rogue_event_process_sync(10, 1000000);
    assert(processed == 4);
    assert(g_test_callback_count == 4);

    /* Test processing specific priority level */
    reset_test_state();
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_HIGH, 1,
                               "HighOnly") == true);
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_LOW, 1,
                               "LowOnly") == true);

    /* Process only high priority events */
    uint32_t high_processed = rogue_event_process_priority(ROGUE_EVENT_PRIORITY_HIGH, 1000000);
    assert(high_processed == 1);
    assert(g_test_callback_count == 1);

    /* Process remaining low priority events */
    uint32_t low_processed = rogue_event_process_priority(ROGUE_EVENT_PRIORITY_LOW, 1000000);
    assert(low_processed == 1);
    assert(g_test_callback_count == 2);

    rogue_event_unsubscribe(sub_id);
    rogue_event_bus_shutdown();
    printf("  ✓ Priority-based event processing passed\n");
    return true;
}

static bool test_event_processing_time_budget(void)
{
    printf("Testing time budget event processing...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("TimeBudgetTest");
    assert(rogue_event_bus_init(&config) == true);

    reset_test_state();

    uint32_t sub_id =
        rogue_event_subscribe(ROGUE_EVENT_ENTITY_CREATED, test_event_callback_basic, NULL, 1);
    assert(sub_id != 0);

    /* Publish many events */
    RogueEventPayload payload = create_test_payload(400);
    for (int i = 0; i < 100; i++)
    {
        assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload,
                                   ROGUE_EVENT_PRIORITY_NORMAL, 1, "TimeTest") == true);
    }

    /* Process with very small time budget (should process only a few) */
    uint32_t processed = rogue_event_process_sync(1000, 1); /* 1 microsecond budget */
    assert(processed < 100); /* Should not process all due to time limit */

    /* Process remaining events with larger budget */
    uint32_t remaining = rogue_event_process_sync(1000, 100000); /* 100ms budget */
    assert(processed + remaining == 100);
    assert(g_test_callback_count == 100);

    rogue_event_unsubscribe(sub_id);
    rogue_event_bus_shutdown();
    printf("  ✓ Time budget event processing passed\n");
    return true;
}

static bool test_event_processing_retry(void)
{
    printf("Testing event processing retry mechanism...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("RetryTest");
    assert(rogue_event_bus_init(&config) == true);

    reset_test_state();

    /* Subscribe with failing callback */
    uint32_t sub_id =
        rogue_event_subscribe(ROGUE_EVENT_ENTITY_CREATED, test_event_callback_failing, NULL, 1);
    assert(sub_id != 0);

    RogueEventPayload payload = create_test_payload(500);
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_NORMAL, 1,
                               "RetryTest") == true);

    const RogueEventBusStats* stats_before = rogue_event_bus_get_stats();
    uint64_t failed_before = stats_before->events_failed;

    /* Process - should fail and retry */
    uint32_t processed = rogue_event_process_sync(10, 1000000);
    assert(processed == 0);            /* No successful processing */
    assert(g_test_callback_count > 1); /* Should have been called multiple times due to retries */

    const RogueEventBusStats* stats_after = rogue_event_bus_get_stats();
    assert(stats_after->events_failed > failed_before); /* Should have failed events */

    rogue_event_unsubscribe(sub_id);
    rogue_event_bus_shutdown();
    printf("  ✓ Event processing retry passed\n");
    return true;
}

/* ===== Event Bus Statistics Tests ===== */

static bool test_event_bus_statistics(void)
{
    printf("Testing event bus statistics...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("StatsTest");
    config.enable_analytics = true;
    assert(rogue_event_bus_init(&config) == true);

    reset_test_state();

    uint32_t sub_id =
        rogue_event_subscribe(ROGUE_EVENT_ENTITY_CREATED, test_event_callback_basic, NULL, 1);
    assert(sub_id != 0);

    /* Initial statistics */
    const RogueEventBusStats* stats = rogue_event_bus_get_stats();
    assert(stats != NULL);
    assert(stats->events_published == 0);
    assert(stats->events_processed == 0);
    assert(stats->active_subscribers == 1);

    /* Publish and process some events */
    RogueEventPayload payload = create_test_payload(600);
    for (int i = 0; i < 10; i++)
    {
        assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload,
                                   ROGUE_EVENT_PRIORITY_NORMAL, 1, "StatsTest") == true);
    }

    uint32_t processed = rogue_event_process_sync(20, 1000000);
    assert(processed == 10);

    /* Check updated statistics */
    stats = rogue_event_bus_get_stats();
    assert(stats->events_published == 10);
    assert(stats->events_processed == 10);
    assert(stats->total_processing_time_us > 0);
    assert(stats->average_latency_us > 0);
    assert(stats->max_queue_depth_reached >= 10);

    /* Test statistics reset */
    rogue_event_bus_reset_stats();
    stats = rogue_event_bus_get_stats();
    assert(stats->events_published == 0);
    assert(stats->events_processed == 0);
    assert(stats->total_processing_time_us == 0);
    assert(stats->active_subscribers == 1); /* Should still show current subscribers */

    rogue_event_unsubscribe(sub_id);
    rogue_event_bus_shutdown();
    printf("  ✓ Event bus statistics passed\n");
    return true;
}

static bool test_event_bus_overload_detection(void)
{
    printf("Testing event bus overload detection...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("OverloadTest");
    config.max_queue_size = 10; /* Small queue for testing */
    assert(rogue_event_bus_init(&config) == true);

    /* Fill queue to near capacity */
    RogueEventPayload payload = create_test_payload(700);
    for (int i = 0; i < 9; i++)
    {
        assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload,
                                   ROGUE_EVENT_PRIORITY_NORMAL, 1, "OverloadTest") == true);
    }

    /* Should not be overloaded yet */
    assert(rogue_event_bus_is_overloaded() == true); /* 9/10 = 90% capacity */

    /* Add one more event to fill queue completely */
    assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload, ROGUE_EVENT_PRIORITY_NORMAL, 1,
                               "OverloadTest") == true);

    /* Should definitely be overloaded now */
    assert(rogue_event_bus_is_overloaded() == true);

    /* Try to add beyond capacity - should fail */
    bool result = rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload,
                                      ROGUE_EVENT_PRIORITY_NORMAL, 1, "OverloadTest");
    assert(result == false); /* Should be rejected due to full queue */

    const RogueEventBusStats* stats = rogue_event_bus_get_stats();
    assert(stats->events_dropped > 0);

    rogue_event_bus_shutdown();
    printf("  ✓ Event bus overload detection passed\n");
    return true;
}

/* ===== Event Replay Tests ===== */

static bool test_event_replay_recording(void)
{
    printf("Testing event replay recording...\n");

    RogueEventBusConfig config = rogue_event_bus_create_default_config("ReplayTest");
    config.enable_replay_recording = true;
    config.replay_history_depth = 5;
    assert(rogue_event_bus_init(&config) == true);

    /* Enable replay recording */
    rogue_event_bus_set_replay_recording(true);

    /* Publish some events */
    RogueEventPayload payload = create_test_payload(800);
    for (int i = 0; i < 3; i++)
    {
        assert(rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &payload,
                                   ROGUE_EVENT_PRIORITY_NORMAL, i, "ReplayTest") == true);
    }

    /* Check replay history */
    uint32_t history_size = 0;
    const RogueEvent** history = rogue_event_bus_get_replay_history(&history_size);
    assert(history != NULL);
    assert(history_size == 3);

    /* Verify recorded events */
    for (int i = 0; i < 3; i++)
    {
        assert(history[i] != NULL);
        assert(history[i]->type_id == ROGUE_EVENT_ENTITY_CREATED);
        assert(history[i]->source_system_id == (uint32_t) i);
    }

    /* Test replay history overflow */
    for (int i = 0; i < 10; i++)
    {
        assert(rogue_event_publish(ROGUE_EVENT_PLAYER_MOVED, &payload, ROGUE_EVENT_PRIORITY_NORMAL,
                                   i + 100, "OverflowTest") == true);
    }

    history = rogue_event_bus_get_replay_history(&history_size);
    assert(history_size == 5); /* Should be limited by replay_history_depth */

    /* Clear replay history */
    rogue_event_bus_clear_replay_history();
    history = rogue_event_bus_get_replay_history(&history_size);
    assert(history_size == 0);

    rogue_event_bus_shutdown();
    printf("  ✓ Event replay recording passed\n");
    return true;
}

/* ===== Utility Function Tests ===== */

static bool test_utility_functions(void)
{
    printf("Testing utility functions...\n");

    /* Test timestamp generation */
    uint64_t ts1 = rogue_event_get_timestamp_us();
    assert(ts1 > 0);

    /* Small delay and check timestamp increased */
    for (volatile int i = 0; i < 1000; i++)
        ; /* Simple delay */
    uint64_t ts2 = rogue_event_get_timestamp_us();
    assert(ts2 >= ts1);

    /* Test payload validation */
    RogueEventPayload payload = create_test_payload(900);
    assert(rogue_event_validate_payload(ROGUE_EVENT_ENTITY_CREATED, &payload) == true);
    assert(rogue_event_validate_payload(ROGUE_EVENT_ENTITY_CREATED, NULL) == false);

    printf("  ✓ Utility functions passed\n");
    return true;
}

/* ===== Main Test Runner ===== */

int main(void)
{
    printf("=== Event Bus Unit Tests ===\n\n");

    int tests_passed = 0;
    int tests_failed = 0;

    /* Array of test functions */
    struct
    {
        const char* name;
        bool (*test_func)(void);
    } tests[] = {{"Event Bus Initialization", test_event_bus_initialization},
                 {"Event Bus Configuration", test_event_bus_configuration},
                 {"Event Type Registry", test_event_type_registry},
                 {"Event Publishing", test_event_publishing},
                 {"Event Batch Publishing", test_event_batch_publishing},
                 {"Basic Event Subscription", test_event_subscription_basic},
                 {"Conditional Event Subscription", test_event_subscription_conditional},
                 {"Rate-Limited Event Subscription", test_event_subscription_rate_limiting},
                 {"System Unsubscription", test_system_unsubscribe},
                 {"Priority-Based Event Processing", test_event_processing_priority},
                 {"Time Budget Event Processing", test_event_processing_time_budget},
                 {"Event Processing Retry", test_event_processing_retry},
                 {"Event Bus Statistics", test_event_bus_statistics},
                 {"Event Bus Overload Detection", test_event_bus_overload_detection},
                 {"Event Replay Recording", test_event_replay_recording},
                 {"Utility Functions", test_utility_functions}};

    const int num_tests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < num_tests; i++)
    {
        printf("\nRunning test: %s...\n", tests[i].name);

        if (tests[i].test_func())
        {
            tests_passed++;
            printf("  PASS\n");
        }
        else
        {
            tests_failed++;
            printf("  FAIL\n");
        }
    }

    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", num_tests);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);

    if (tests_failed == 0)
    {
        printf("\n🎉 All event bus tests passed!\n");
        return 0;
    }
    else
    {
        printf("\n❌ %d test(s) failed!\n", tests_failed);
        return 1;
    }
}
