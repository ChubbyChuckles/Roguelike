#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/player/player_progress.h"
#include "../../src/core/progression/progression_award.h"

static int g_xp_events = 0;
static int g_level_events = 0;
static uint32_t g_last_xp_amount = 0;

static bool on_xp(const RogueEvent* evt, void* user)
{
    (void) user;
    g_xp_events++;
    g_last_xp_amount = evt->payload.xp_gained.xp_amount;
    return true;
}

static bool on_level(const RogueEvent* evt, void* user)
{
    (void) user;
    g_level_events++;
    (void) evt;
    return true;
}

int main(void)
{
    /* Init minimal state */
    memset(&g_app, 0, sizeof g_app);
    g_app.player.level = 1;
    g_app.player.xp = 0;
    g_app.player.xp_to_next = 10; /* small threshold */

    /* Init event bus */
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("TestBus");
    assert(rogue_event_bus_init(&cfg));

    uint32_t sub_xp = rogue_event_subscribe(ROGUE_EVENT_XP_GAINED, on_xp, NULL, 0);
    uint32_t sub_lv = rogue_event_subscribe(ROGUE_EVENT_LEVEL_UP, on_level, NULL, 0);
    (void) sub_xp;
    (void) sub_lv;

    /* Award less than threshold: should publish XP_GAINED, no level */
    rogue_award_xp(3u, 1u, 7u);
    /* Pump events */
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_HIGH, 100000);
    assert(g_xp_events == 1);
    assert(g_last_xp_amount == 3u);
    assert(g_level_events == 0);

    /* Tick progression with no level-ups */
    rogue_player_progress_update(0.016);
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_HIGH, 100000);
    assert(g_level_events == 0);

    /* Now award enough to level: event + level up publish */
    rogue_award_xp(20u, 1u, 7u);
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);
    rogue_player_progress_update(0.016);
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_HIGH, 100000);
    assert(g_xp_events >= 2);
    assert(g_app.player.level == 2);
    assert(g_level_events >= 1);

    printf("XP_EVENTS_OK xp=%d lvl=%d\n", g_xp_events, g_app.player.level);
    rogue_event_bus_shutdown();
    return 0;
}
