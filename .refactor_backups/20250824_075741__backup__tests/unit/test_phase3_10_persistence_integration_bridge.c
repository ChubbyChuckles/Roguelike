#include "../../src/core/integration/event_bus.h"
#include "../../src/core/integration/persistence_integration_bridge.h"
#include "../../src/core/save_manager.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0, tests_passed = 0;
#define T(cond, msg)                                                                               \
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

static void ensure_bus()
{
    if (!rogue_event_bus_get_instance())
    {
        RogueEventBusConfig cfg = rogue_event_bus_create_default_config("persist_test_bus");
        rogue_event_bus_init(&cfg);
    }
}
static void publish_simple_events()
{
    RogueEventPayload xp;
    memset(&xp, 0, sizeof xp);
    xp.xp_gained.player_id = 1;
    xp.xp_gained.xp_amount = 25;
    xp.xp_gained.source_type = 1;
    xp.xp_gained.source_id = 1;
    rogue_event_publish(ROGUE_EVENT_XP_GAINED, &xp, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");
    RogueEventPayload it;
    memset(&it, 0, sizeof it);
    it.item_picked_up.item_id = 42;
    it.item_picked_up.player_id = 1;
    it.item_picked_up.auto_pickup = 1;
    rogue_event_publish(ROGUE_EVENT_ITEM_PICKED_UP, &it, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");
}
int main(void)
{
    printf("Phase 3.10 Persistence Integration Bridge Tests\n===============================\n");
    ensure_bus();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    RoguePersistenceBridge bridge;
    T(rogue_persist_bridge_init(&bridge) == 0, "bridge init");
    T(rogue_persist_bridge_is_initialized(&bridge), "bridge initialized flag");
    rogue_persist_bridge_enable_incremental(1);
    rogue_persist_bridge_enable_compression(1, 32);
    T(rogue_persist_bridge_save_slot(&bridge, 0) == 0, "initial save slot0");
    RoguePersistenceBridgeMetrics m1 = rogue_persist_bridge_get_metrics(&bridge);
    T(m1.sections_written > 0, "initial save wrote sections");
    T(rogue_persist_bridge_save_slot(&bridge, 0) == 0, "second save slot0");
    RoguePersistenceBridgeMetrics m2 = rogue_persist_bridge_get_metrics(&bridge);
    T(m2.sections_reused > 0, "second save reused sections");
    publish_simple_events();
    rogue_event_process_sync(64, 10000);
    T(rogue_persist_bridge_component_dirty(ROGUE_SAVE_COMP_PLAYER) == 1,
      "player component dirty after xp");
    T(rogue_persist_bridge_component_dirty(ROGUE_SAVE_COMP_INVENTORY) == 1,
      "inventory dirty after pickup");
    T(rogue_persist_bridge_save_slot(&bridge, 0) == 0, "third save after events");
    RoguePersistenceBridgeMetrics m3 = rogue_persist_bridge_get_metrics(&bridge);
    T(m3.sections_written >= 1, "third save wrote some sections");
    T(m3.sections_reused >= 1, "third save reused some sections");
    int section_count = rogue_persist_bridge_validate_slot(0);
    T(section_count > 0, "validate slot enumerates sections");
    printf("\nSummary: %d/%d passed (%.1f%%)\n", tests_passed, tests_run,
           tests_run ? (tests_passed * 100.0 / tests_run) : 0.0);
    return tests_passed == tests_run ? 0 : 1;
}
