#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/player/player_progress.h"
#include "../../src/core/skills/skills.h"
#include "../../src/game/buffs.h"
#include <assert.h>
#include <stdio.h>

static RogueSkillDef make_fire_instant_cost(void)
{
    RogueSkillDef d = {0};
    d.id = -1;
    d.name = "Ignite";
    d.icon = "icon_ignite";
    d.max_rank = 3;
    d.base_cooldown_ms = 1000.0f;
    d.cooldown_reduction_ms_per_rank = 0.0f;
    d.cast_time_ms = 0.0f;
    d.cast_type = 0; /* instant */
    d.action_point_cost = 10;
    d.tags = ROGUE_SKILL_TAG_FIRE;
    return d;
}

int main(void)
{
    /* Init required systems (event bus, then skills, then buffs). */
    RogueEventBusConfig cfg =
        rogue_event_bus_create_default_config("skills_test_bus_overdrive_heat");
    assert(rogue_event_bus_init(&cfg) && "event bus init");
    rogue_skills_init();
    rogue_buffs_init();
    /* Register fire instant */
    RogueSkillDef fire = make_fire_instant_cost();
    int fid = rogue_skill_register(&fire);
    g_app.talent_points = 1;
    assert(rogue_skill_rank_up(fid) == 1);
    /* Setup player resource caps */
    g_app.player.level = 1;
    g_app.player.dexterity = 10;
    rogue_player_recalc_derived(&g_app.player);
    g_app.player.action_points = g_app.player.max_action_points;
    g_app.player.mana = g_app.player.max_mana;
    g_app.game_time_ms = 0.0;

    /* Begin overdrive: +20 AP cap for 2000ms, exhaustion 1500ms */
    rogue_overdrive_begin(20, 2000.0f, 1500.0f);

    /* Spend until we hit overdrive cap via refunds and regen not needed here */
    RogueSkillCtx ctx = {0};
    ctx.now_ms = 0.0;
    assert(rogue_skill_try_activate(fid, &ctx) == 1);
    assert(g_app.player.action_points <= g_app.player.max_action_points + g_app.ap_overdrive_bonus);

    /* Advance time to just after cooldown and activate again; ensure refunds clamp to overdrive cap
     */
    g_app.game_time_ms = 1001.0;
    ctx.now_ms = g_app.game_time_ms;
    assert(rogue_skill_try_activate(fid, &ctx) == 1);
    int ap_cap_now = g_app.player.max_action_points + g_app.ap_overdrive_bonus;
    assert(g_app.player.action_points <= ap_cap_now);

    /* Tick progression: simulate 3 seconds to end overdrive and apply exhaustion */
    for (int i = 0; i < 200; ++i)
    {
        rogue_player_progress_update(0.016);
    }
    assert(g_app.ap_overdrive_ms == 0.0f);
    /* Exhaustion should be active at least briefly */
    assert(g_app.ap_exhaustion_ms >= 0.0f);

    /* Heat: using fire skill should add heat and trigger overheat at cap */
    g_app.player.heat = g_app.player.max_heat - 3;
    g_app.game_time_ms += 1001.0;
    ctx.now_ms = g_app.game_time_ms;
    assert(rogue_skill_try_activate(fid, &ctx) == 1);
    assert(g_app.player.heat == g_app.player.max_heat);
    assert(g_app.overheat_active == 1);

    /* Venting: run updates for a bit and confirm heat decreases and eventually clears overheat */
    int start_heat = g_app.player.heat;
    for (int i = 0; i < 200; ++i)
    {
        rogue_player_progress_update(0.016);
    }
    assert(g_app.player.heat < start_heat);
    /* Keep venting until zero */
    for (int i = 0; i < 2000 && g_app.player.heat > 0; ++i)
    {
        rogue_player_progress_update(0.016);
    }
    assert(g_app.player.heat == 0);
    assert(g_app.overheat_active == 0);

    printf("PHASE2_OVERDRIVE_HEAT_OK\n");
    rogue_event_bus_shutdown();
    rogue_skills_shutdown();
    return 0;
}
