#include "../../src/core/integration/event_bus.h"
#include "../../src/core/integration/ui_integration_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(cond, msg)                                                                            \
    do                                                                                             \
    {                                                                                              \
        tests_run++;                                                                               \
        if (cond)                                                                                  \
        {                                                                                          \
            printf("PASS %s\n", msg);                                                              \
            tests_passed++;                                                                        \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            printf("FAIL %s\n", msg);                                                              \
        }                                                                                          \
    } while (0)

static void ensure_event_bus()
{
    if (!rogue_event_bus_get_instance())
    {
        RogueEventBusConfig cfg = rogue_event_bus_create_default_config("ui_test_bus");
        rogue_event_bus_init(&cfg);
    }
}

static void test_initialization()
{
    printf("\n-- test_initialization --\n");
    ensure_event_bus();
    RogueUIBridge bridge;
    TEST(rogue_ui_bridge_init(&bridge), "Bridge initializes");
    TEST(rogue_ui_bridge_is_operational(&bridge), "Bridge operational");
    RogueUIBridgeMetrics m = rogue_ui_bridge_get_metrics(&bridge);
    TEST(m.total_events_processed == 0, "No events processed yet");
    rogue_ui_bridge_shutdown(&bridge);
    TEST(!rogue_ui_bridge_is_operational(&bridge), "Bridge shutdown sets non-operational");
}

static void publish_damage_event()
{
    RogueEventPayload p;
    memset(&p, 0, sizeof(p));
    p.damage_event.source_entity_id = 1;
    p.damage_event.target_entity_id = 2;
    p.damage_event.damage_amount = 42.0f;
    p.damage_event.is_critical = true;
    rogue_event_publish(ROGUE_EVENT_DAMAGE_DEALT, &p, ROGUE_EVENT_PRIORITY_NORMAL,
                        ROGUE_UI_SOURCE_COMBAT, "combat");
}

static void test_event_flow()
{
    printf("\n-- test_event_flow --\n");
    ensure_event_bus();
    RogueUIBridge bridge;
    rogue_ui_bridge_init(&bridge);
    publish_damage_event();
    /* Publish XP gained */
    RogueEventPayload xp;
    memset(&xp, 0, sizeof(xp));
    xp.xp_gained.player_id = 1;
    xp.xp_gained.xp_amount = 50;
    xp.xp_gained.source_type = 1;
    xp.xp_gained.source_id = 99;
    rogue_event_publish(ROGUE_EVENT_XP_GAINED, &xp, ROGUE_EVENT_PRIORITY_NORMAL,
                        ROGUE_UI_SOURCE_PLAYER, "xp");
    /* Publish currency change */
    RogueEventPayload cur;
    memset(&cur, 0, sizeof(cur));
    cur.raw_data[0] = 0; /* placeholder */
    rogue_event_publish(ROGUE_EVENT_CURRENCY_CHANGED, &cur, ROGUE_EVENT_PRIORITY_NORMAL,
                        ROGUE_UI_SOURCE_VENDOR, "currency");
    /* Publish area entered */
    RogueEventPayload area;
    memset(&area, 0, sizeof(area));
    area.area_transition.area_id = 5;
    area.area_transition.player_id = 1;
    area.area_transition.previous_area_id = 2;
    rogue_event_publish(ROGUE_EVENT_AREA_ENTERED, &area, ROGUE_EVENT_PRIORITY_NORMAL,
                        ROGUE_UI_SOURCE_WORLDMAP, "area");
    /* Publish resource spawned */
    RogueEventPayload res;
    memset(&res, 0, sizeof(res));
    res.area_transition.area_id = 5;
    res.area_transition.player_id = 1;
    res.area_transition.previous_area_id = 2;
    rogue_event_publish(ROGUE_EVENT_RESOURCE_SPAWNED, &res, ROGUE_EVENT_PRIORITY_NORMAL,
                        ROGUE_UI_SOURCE_WORLDMAP, "resource");
    rogue_event_process_sync(64, 10000);
    TEST(bridge.metrics.total_events_processed > 0, "Events processed updates metric");
    RogueUICombatLogEntry entries[4];
    uint32_t n = rogue_ui_bridge_get_combat_log(&bridge, entries, 4);
    TEST(n > 0, "Combat log entry captured");
    if (n > 0)
    {
        TEST(entries[0].value == 42.0f, "Damage value recorded");
    }
    RogueUIBinding health;
    rogue_ui_bridge_get_binding(&bridge, ROGUE_UI_BIND_HEALTH, &health);
    TEST(health.dirty, "Health binding marked dirty after damage");
    RogueUIBinding xp_bind;
    rogue_ui_bridge_get_binding(&bridge, ROGUE_UI_BIND_XP, &xp_bind);
    TEST(xp_bind.dirty, "XP binding dirty after xp event");
    RogueUIBinding gold_bind;
    rogue_ui_bridge_get_binding(&bridge, ROGUE_UI_BIND_GOLD, &gold_bind);
    TEST(gold_bind.dirty, "Gold binding dirty after currency event");
    RogueUIWorldMapUpdate wm[4];
    uint32_t wm_n = rogue_ui_bridge_get_worldmap_updates(&bridge, wm, 4);
    TEST(wm_n > 0, "World map updates captured");
    rogue_ui_bridge_shutdown(&bridge);
}

static void test_binding_force()
{
    printf("\n-- test_binding_force --\n");
    ensure_event_bus();
    RogueUIBridge bridge;
    rogue_ui_bridge_init(&bridge);
    TEST(rogue_ui_bridge_force_binding(&bridge, ROGUE_UI_BIND_GOLD, 1234, 0.0f),
         "Force binding gold");
    RogueUIBinding gold;
    rogue_ui_bridge_get_binding(&bridge, ROGUE_UI_BIND_GOLD, &gold);
    TEST(gold.last_value_u32 == 1234, "Gold value set");
    RogueUIBinding dirty_arr[8];
    uint32_t d = rogue_ui_bridge_get_dirty_bindings(&bridge, dirty_arr, 8);
    TEST(d > 0, "Dirty bindings enumerated");
    rogue_ui_bridge_shutdown(&bridge);
}

int main(void)
{
    printf("Phase 3.9 UI Integration Bridge Tests\n===============================\n");
    test_initialization();
    test_event_flow();
    test_binding_force();
    printf("\nSummary: %d/%d passed (%.1f%%)\n", tests_passed, tests_run,
           tests_run ? (tests_passed * 100.0 / tests_run) : 0.0);
    return tests_passed == tests_run ? 0 : 1;
}
