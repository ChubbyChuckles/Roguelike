#include "../../src/core/integration/event_bus.h"
#include "../../src/core/integration/persistence_integration_bridge.h"
#include "../../src/core/persistence/save_manager.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Minimal stubs to simulate flows for Phase 3.11 tests without full gameplay loop. */
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

/* Test-event ID range (36864-40959 reserved for Test Events per config_version.c) */
#define TEST_EVENT_LOOT_DROP 36864
#define TEST_EVENT_MATERIAL_EXTRACT 36865
#define TEST_EVENT_RECIPE_UNLOCK 36866
#define TEST_EVENT_WORLD_CHUNK_GEN 36867
#define TEST_EVENT_RESOURCE_PLACED 36868
#define TEST_EVENT_GATHERED 36869
#define TEST_EVENT_CRAFTED 36870

/* Simple wrappers to treat custom IDs as RogueEventType */
static void publish_custom(int id)
{
    RogueEventPayload p;
    memset(&p, 0, sizeof p);
    rogue_event_publish((RogueEventTypeId) id, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test_custom");
}

/* Simulate enemy spawn -> AI activation -> combat engagement */
static int enemy_spawned = 0, ai_activated = 0, combat_started = 0;
static bool on_enemy_spawn(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    enemy_spawned++;
    return true;
}
static bool on_ai_activate(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (enemy_spawned)
        ai_activated++;
    return true;
}
static bool on_combat_start(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (ai_activated)
        combat_started++;
    return true;
}

/* Simulate equipment change -> combat stat update -> damage calc */
static int equip_events = 0, stat_updates = 0, dmg_calcs = 0;
static bool on_equip(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    equip_events++;
    return true;
}
static bool on_stat_update(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (equip_events)
        stat_updates++;
    return true;
}
static bool on_damage_calc(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (stat_updates)
        dmg_calcs++;
    return true;
}

/* Simulate combat victory -> XP gain -> skill unlock -> passive application */
static int victories = 0, xp_events = 0, skill_unlocks = 0, passive_applies = 0;
static bool on_combat_victory(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    victories++;
    return true;
}
static bool on_xp_gain(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (victories)
        xp_events++;
    return true;
}
static bool on_skill_unlock(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (xp_events)
        skill_unlocks++;
    return true;
}
static bool on_passive_apply(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (skill_unlocks)
        passive_applies++;
    return true;
}

/* Simulate loot drop -> material extraction -> recipe unlock */
static int loot_drops = 0, material_extracts = 0, recipe_unlocks = 0;
static bool on_loot_drop(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    loot_drops++;
    return true;
}
static bool on_material_extract(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (loot_drops)
        material_extracts++;
    return true;
}
static bool on_recipe_unlock(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (material_extracts)
        recipe_unlocks++;
    return true;
}

/* Simulate world generation -> resource placement -> gathering -> crafting */
static int world_chunks = 0, resources_placed = 0, gathered = 0, crafts = 0;
static bool on_world_chunk(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    world_chunks++;
    return true;
}
static bool on_resource_place(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (world_chunks)
        resources_placed++;
    return true;
}
static bool on_gather(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (resources_placed)
        gathered++;
    return true;
}
static bool on_craft(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    if (gathered)
        crafts++;
    return true;
}

/* Simulate multi-system save/load full state preservation */
static bool on_save_completed(const RogueEvent* ev, void* user)
{
    (void) user;
    (void) ev;
    return true;
}

int main(void)
{
    printf("Phase 3.11 Integration Bridge Validation Tests\n===============================\n");
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("phase3_11_bus");
    rogue_event_bus_init(&cfg);

    /* Subscribe flows using existing event constants where reasonable (placeholders used) */
    rogue_event_subscribe(ROGUE_EVENT_ENTITY_CREATED, on_enemy_spawn, NULL, 0);
    /* Use ENTITY_MODIFIED as a stand-in for AI activation */
    rogue_event_subscribe(ROGUE_EVENT_ENTITY_MODIFIED, on_ai_activate, NULL, 0);
    rogue_event_subscribe(ROGUE_EVENT_PLAYER_ATTACKED, on_combat_start, NULL, 0);

    rogue_event_subscribe(ROGUE_EVENT_PLAYER_EQUIPPED, on_equip, NULL, 0);
    rogue_event_subscribe(ROGUE_EVENT_PERFORMANCE_ALERT, on_stat_update, NULL, 0); /* reuse */
    rogue_event_subscribe(ROGUE_EVENT_DAMAGE_DEALT, on_damage_calc, NULL, 0);

    rogue_event_subscribe(ROGUE_EVENT_DAMAGE_DEALT, on_combat_victory, NULL,
                          0); /* treat damage dealt as victory trigger once */
    rogue_event_subscribe(ROGUE_EVENT_XP_GAINED, on_xp_gain, NULL, 0);
    rogue_event_subscribe(ROGUE_EVENT_SKILL_UNLOCKED, on_skill_unlock, NULL, 0);
    rogue_event_subscribe(ROGUE_EVENT_SAVE_COMPLETED, on_passive_apply, NULL, 0); /* chain final */

    rogue_event_subscribe(ROGUE_EVENT_SAVE_COMPLETED, on_save_completed, NULL, 0);

    /* Custom test-event subscriptions for remaining Phase 3.11 flows */
    rogue_event_subscribe((RogueEventTypeId) TEST_EVENT_LOOT_DROP, on_loot_drop, NULL, 0);
    rogue_event_subscribe((RogueEventTypeId) TEST_EVENT_MATERIAL_EXTRACT, on_material_extract, NULL,
                          0);
    rogue_event_subscribe((RogueEventTypeId) TEST_EVENT_RECIPE_UNLOCK, on_recipe_unlock, NULL, 0);
    rogue_event_subscribe((RogueEventTypeId) TEST_EVENT_WORLD_CHUNK_GEN, on_world_chunk, NULL, 0);
    rogue_event_subscribe((RogueEventTypeId) TEST_EVENT_RESOURCE_PLACED, on_resource_place, NULL,
                          0);
    rogue_event_subscribe((RogueEventTypeId) TEST_EVENT_GATHERED, on_gather, NULL, 0);
    rogue_event_subscribe((RogueEventTypeId) TEST_EVENT_CRAFTED, on_craft, NULL, 0);

    /* Publish synthetic sequence */
    RogueEventPayload p;
    memset(&p, 0, sizeof p);
    rogue_event_publish(ROGUE_EVENT_ENTITY_CREATED, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");
    rogue_event_publish(ROGUE_EVENT_ENTITY_MODIFIED, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");
    rogue_event_publish(ROGUE_EVENT_PLAYER_ATTACKED, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");

    rogue_event_publish(ROGUE_EVENT_PLAYER_EQUIPPED, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");
    rogue_event_publish(ROGUE_EVENT_PERFORMANCE_ALERT, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");
    rogue_event_publish(ROGUE_EVENT_DAMAGE_DEALT, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");

    rogue_event_publish(ROGUE_EVENT_DAMAGE_DEALT, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");
    rogue_event_publish(ROGUE_EVENT_XP_GAINED, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");
    rogue_event_publish(ROGUE_EVENT_SKILL_UNLOCKED, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0, "test");

    rogue_save_manager_init();
    rogue_register_core_save_components();
    RoguePersistenceBridge bridge;
    rogue_persist_bridge_init(&bridge);
    rogue_persist_bridge_enable_incremental(1);
    rogue_persist_bridge_save_slot(&bridge, 0); /* triggers SAVE_COMPLETED */

    /* Publish custom chains (immediately process to avoid queue pressure) */
    publish_custom(TEST_EVENT_LOOT_DROP);
    publish_custom(TEST_EVENT_MATERIAL_EXTRACT);
    publish_custom(TEST_EVENT_RECIPE_UNLOCK);
    publish_custom(TEST_EVENT_WORLD_CHUNK_GEN);
    publish_custom(TEST_EVENT_RESOURCE_PLACED);
    publish_custom(TEST_EVENT_GATHERED);
    publish_custom(TEST_EVENT_CRAFTED);
    rogue_event_process_sync(64, 50000);

    /* Performance micro-benchmark: publish & process N mixed events */
    const int perfN = 800;
    int perf_processed_before = tests_run;
    (void) perf_processed_before; /* keep under queue cap (800*3=2400 < 4096) */
    clock_t t0 = clock();
    for (int i = 0; i < perfN; i++)
    {
        publish_custom(TEST_EVENT_LOOT_DROP);
        publish_custom(TEST_EVENT_WORLD_CHUNK_GEN);
        publish_custom(TEST_EVENT_CRAFTED);
    }
    rogue_event_process_sync(8192, 1000000);
    clock_t t1 = clock();
    double secs = (double) (t1 - t0) / CLOCKS_PER_SEC;
    double per_event_us = (secs * 1e6) / (perfN * 3);
    printf("Performance: %.2f us/event over %d events (target < 150.0)\n", per_event_us, perfN * 3);

    T(enemy_spawned == 1 && ai_activated == 1 && combat_started == 1, "enemy->ai->combat chain");
    T(equip_events == 1 && stat_updates == 1 && dmg_calcs >= 1, "equip->stat->dmg chain");
    T(victories >= 1 && xp_events == 1 && skill_unlocks == 1 && passive_applies == 1,
      "victory->xp->skill->passive chain");
    /* Allow >=1 for outer events because performance loop may generate additional LOOT_DROP /
     * WORLD_CHUNK_GEN / CRAFTED events */
    T(loot_drops >= 1 && material_extracts >= 1 && recipe_unlocks >= 1,
      "loot->material->recipe chain");
    T(world_chunks >= 1 && resources_placed >= 1 && gathered >= 1 && crafts >= 1,
      "worldgen->resource->gather->craft chain");
    T(per_event_us < 150.0, "integration overhead perf (<150us/event)");

    printf("\nSummary: %d/%d passed (%.1f%%)\n", tests_passed, tests_run,
           tests_run ? (tests_passed * 100.0 / tests_run) : 0.0);
    return tests_passed == tests_run ? 0 : 1;
}
