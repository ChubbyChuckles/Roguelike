/* Phase 3.6.2: Skill prerequisite gating with progression level gates + SKILL_UNLOCKED emission */
#include "../../src/core/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int s_unlocked_skill_id = -1;
static bool on_skill_unlocked(const RogueEvent* ev, void* user)
{
    (void) user;
    if (!ev)
        return false;
    s_unlocked_skill_id = (int) ev->payload.xp_gained.source_id;
    return true;
}

static int cb_ok(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return 1;
}

int main(void)
{
    /* Ensure event bus exists */
    if (!rogue_event_bus_get_instance())
    {
        RogueEventBusConfig cfg = rogue_event_bus_create_default_config("skill_gating_bus");
        rogue_event_bus_init(&cfg);
    }
    RogueEventSubscription* sub =
        rogue_event_subscribe(ROGUE_EVENT_SKILL_UNLOCKED, on_skill_unlocked, NULL, 0);
    /* Init skills and player */
    rogue_skills_init();
    g_app.talent_points = 2;
    g_app.player.level = 3; /* below gate for strength=1 (requires 5) */
    RogueSkillDef s = {0};
    s.name = "Gated Skill";
    s.max_rank = 3;
    s.on_activate = cb_ok;
    s.skill_strength = 1; /* gate => min level 5 */
    int sid = rogue_skill_register(&s);
    assert(sid >= 0);
    /* Attempt unlock should fail due to level gate */
    assert(rogue_skill_rank_up(sid) == -1);
    assert(g_app.talent_points == 2);
    /* Raise level; unlock should pass and emit event */
    g_app.player.level = 6;
    assert(rogue_skill_rank_up(sid) == 1);
    /* Process event queue synchronously */
    rogue_event_process_sync(32, 10000);
    assert(s_unlocked_skill_id == sid);
    /* Subsequent rank-ups shouldn't emit SKILL_UNLOCKED again */
    s_unlocked_skill_id = -1;
    assert(rogue_skill_rank_up(sid) == 2);
    rogue_event_process_sync(16, 10000);
    assert(s_unlocked_skill_id == -1);
    printf("PH3_6_SKILL_GATING_OK id=%d level=%d rank=%d tp=%d\n", sid, g_app.player.level,
           (int) rogue_skill_get_state(sid)->rank, g_app.talent_points);
    rogue_event_unsubscribe(sub);
    rogue_skills_shutdown();
    return 0;
}
