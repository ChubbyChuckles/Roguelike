#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int s_skill_unlocked_count = 0;
static uint32_t s_last_skill_id = 0xFFFFFFFFu;

static bool on_skill_unlocked(const RogueEvent* ev, void* user)
{
    (void) user;
    if (!ev)
        return false;
    s_skill_unlocked_count++;
    s_last_skill_id = ev->payload.xp_gained.source_id;
    return true;
}

int main(void)
{
    /* Init minimal app + event bus */
    memset(&g_app, 0, sizeof g_app);
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("skill_unlock_test_bus");
    rogue_event_bus_init(&cfg);

    /* Subscribe to SKILL_UNLOCKED */
    rogue_event_subscribe(ROGUE_EVENT_SKILL_UNLOCKED, on_skill_unlocked, NULL, 0);

    /* Init skills and register one with a ring strength gate */
    rogue_skills_init();
    g_app.talent_points = 1;
    g_app.player.level = 1; /* too low for strength=1 if rule is 5*strength */

    RogueSkillDef d = {0};
    d.name = "GatedSkill";
    d.max_rank = 1;
    d.skill_strength = 1; /* implies min level 5 in our simple rule */
    int sid = rogue_skill_register(&d);
    assert(sid >= 0);

    /* First attempt should be gated by level */
    int r = rogue_skill_rank_up(sid);
    assert(r == -1);
    assert(s_skill_unlocked_count == 0);

    /* Raise player level to meet gate and grant a point */
    g_app.player.level = 5;
    g_app.talent_points = 1;
    r = rogue_skill_rank_up(sid);
    assert(r == 1);

    /* Pump events so the SKILL_UNLOCKED is dispatched */
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);

    /* Event should have fired carrying skill id via xp_gained.source_id */
    assert(s_skill_unlocked_count == 1);
    assert(s_last_skill_id == (uint32_t) sid);

    printf("PH3_6_SKILL_UNLOCK_OK id=%u events=%d\n", sid, s_skill_unlocked_count);
    rogue_skills_shutdown();
    rogue_event_bus_shutdown();
    return 0;
}
