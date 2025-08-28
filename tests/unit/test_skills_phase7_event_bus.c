#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include "../../src/game/buffs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int s_channel_tick_count = 0;
static int s_combo_spend_count = 0;
static uint16_t s_last_tick_skill = 0xFFFF;
static uint16_t s_last_spend_skill = 0xFFFF;

static bool on_channel_tick(const RogueEvent* ev, void* user)
{
    (void) user;
    assert(ev && ev->type_id == ROGUE_EVENT_SKILL_CHANNEL_TICK);
    s_channel_tick_count++;
    s_last_tick_skill = ev->payload.skill_channel_tick.skill_id;
    /* basic sanity: tick_index >=1 and when_ms >=0 */
    assert(ev->payload.skill_channel_tick.tick_index >= 1);
    assert(ev->payload.skill_channel_tick.when_ms >= 0.0);
    return true;
}

static bool on_combo_spend(const RogueEvent* ev, void* user)
{
    (void) user;
    assert(ev && ev->type_id == ROGUE_EVENT_SKILL_COMBO_SPEND);
    s_combo_spend_count++;
    s_last_spend_skill = ev->payload.skill_combo_spend.skill_id;
    assert(ev->payload.skill_combo_spend.amount > 0);
    return true;
}

/* Minimal channel skill that emits ticks via on_activate callback */
static int cb_noop(const struct RogueSkillDef* def, struct RogueSkillState* st,
                   const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return ROGUE_ACT_CONSUMED;
}

int main(void)
{
    /* Init event bus */
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("skills_phase7_bus");
    assert(rogue_event_bus_init(&cfg));

    /* Subscribe */
    uint32_t sub1 =
        rogue_event_subscribe(ROGUE_EVENT_SKILL_CHANNEL_TICK, on_channel_tick, NULL, 0x534B494C);
    uint32_t sub2 =
        rogue_event_subscribe(ROGUE_EVENT_SKILL_COMBO_SPEND, on_combo_spend, NULL, 0x534B494C);
    assert(sub1 && sub2);

    /* Init skills */
    rogue_skills_init();
    rogue_buffs_init();
    /* Provide talent points and a sane player level for rank gating */
    g_app.talent_points = 2;
    g_app.player.level = 1;

    /* Register a channel skill and a spender */
    RogueSkillDef chan = {0};
    chan.name = "Chan7";
    chan.max_rank = 1;
    chan.base_cooldown_ms = 0;
    chan.on_activate = cb_noop;
    chan.cast_type = 2;          /* channel */
    chan.cast_time_ms = 1000.0f; /* expect ~4 ticks */
    int id_chan = rogue_skill_register(&chan);
    RogueSkillDef spend = {0};
    spend.name = "Spend7";
    spend.max_rank = 1;
    spend.base_cooldown_ms = 0;
    spend.on_activate = cb_noop;
    spend.cast_type = 0; /* instant */
    spend.combo_spender = 1;
    int id_spend = rogue_skill_register(&spend);
    assert(rogue_skill_rank_up(id_chan) == 1);
    assert(rogue_skill_rank_up(id_spend) == 1);

    RogueSkillCtx ctx = {0};
    ctx.now_ms = 0.0;
    assert(rogue_skill_try_activate(id_chan, &ctx) == 1);

    /* Simulate time to produce ticks */
    for (int t = 0; t <= 1000; t += 50)
    {
        rogue_skills_update((double) t);
        /* Process published events so subscribers receive them */
        rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);
    }
    /* Expect some channel tick events */
    if (s_channel_tick_count <= 0)
    {
        fprintf(stderr, "no channel ticks captured\n");
        return 1;
    }

    /* Build some combo then spend to fire spend event */
    g_app.player_combat.combo = 3;
    ctx.now_ms = 1100.0;
    assert(rogue_skill_try_activate(id_spend, &ctx) == 1);
    rogue_skills_update(1100.0);
    /* Pump events to deliver combo spend */
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);
    if (s_combo_spend_count <= 0)
    {
        fprintf(stderr, "no combo spend events captured\n");
        return 2;
    }

    printf("PH7_EVENT_BUS_OK ticks=%d spends=%d\n", s_channel_tick_count, s_combo_spend_count);
    rogue_event_bus_shutdown();
    rogue_skills_shutdown();
    return 0;
}
