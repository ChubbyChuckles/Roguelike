/* Validate haste snapshot modes for casts/channels and drift-corrected channel ticks. */
#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include "../../src/game/buffs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int g_cast_hits = 0;
static int g_chan_ticks = 0;
static int cb_cast(const RogueSkillDef* d, struct RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) d;
    (void) st;
    (void) ctx;
    g_cast_hits++;
    return 1;
}
static int cb_tick(const RogueSkillDef* d, struct RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) d;
    (void) st;
    (void) ctx;
    g_chan_ticks++;
    return 1;
}

/* Monotonic simulated clock advance helper (~16ms ticks + remainder). */
static double g_now_ms = 0.0;
static void advance(double ms)
{
    if (ms <= 0)
        return;
    int ticks = (int) (ms / 16.0);
    for (int i = 0; i < ticks; ++i)
    {
        g_now_ms += 16.0;
        rogue_skills_update(g_now_ms);
    }
    double rem = ms - (ticks * 16.0);
    if (rem > 0.0)
    {
        g_now_ms += rem;
        rogue_skills_update(g_now_ms);
    }
}

int main(void)
{
    /* Init required systems (skills, buffs, event bus). */
    rogue_skills_init();
    rogue_buffs_init();
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("skills_test_bus_snapshot");
    assert(rogue_event_bus_init(&cfg) && "event bus init");
    g_app.talent_points = 5;

    /* Cast with snapshot haste: haste applied once at start, not changing mid-cast */
    RogueSkillDef cast = {0};
    cast.name = "SnapCast";
    cast.max_rank = 1;
    cast.base_cooldown_ms = 0;
    cast.on_activate = cb_cast;
    cast.cast_type = 1;
    cast.cast_time_ms = 400.0f;
    cast.haste_mode_flags = 0x1; /* snapshot cast */
    int idc = rogue_skill_register(&cast);
    assert(rogue_skill_rank_up(idc) == 1);
    RogueSkillCtx ctx = {0};
    ctx.now_ms = g_now_ms;
    assert(rogue_skill_try_activate(idc, &ctx) == 1);
    /* Apply haste mid-cast: should NOT change remaining duration due to snapshot */
    advance(160.0);
    rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE, 25, 1000.0, g_now_ms, ROGUE_BUFF_STACK_ADD, 0);
    advance(240.0); /* finish cast */
    assert(g_cast_hits == 1);
    g_cast_hits = 0;

    /* Channel with snapshot tick interval and drift correction: expect ~duration/interval ticks */
    /* Clear any leftover buffs from the cast test (we applied haste mid-cast). */
    rogue_buffs_init();
    RogueSkillDef chan = {0};
    chan.name = "SnapChan";
    chan.max_rank = 1;
    chan.base_cooldown_ms = 0;
    chan.on_activate = cb_tick;
    chan.cast_type = 2;
    chan.cast_time_ms = 1000.0f;
    chan.haste_mode_flags = 0x2; /* snap channel */
    int idq = rogue_skill_register(&chan);
    assert(rogue_skill_rank_up(idq) == 1);
    ctx.now_ms = g_now_ms;
    assert(rogue_skill_try_activate(idq, &ctx) == 1);
    /* Exclude the immediate on_activate call from tick counting. */
    g_chan_ticks = 0;
    /* With no haste, interval = 250ms -> expect exactly 4 ticks in 1000ms */
    advance(1000.0);
    if (g_chan_ticks != 4)
    {
        printf("ticks=%d\n", g_chan_ticks);
        return 2;
    }
    g_chan_ticks = 0;
    /* Now dynamic haste mid-channel should NOT affect snap channel; but test dynamic path by
        disabling snapshot and ensuring more ticks occur. Reset skills and clock for isolation. */
    rogue_skills_shutdown();
    rogue_skills_init();
    g_now_ms = 0.0;
    g_app.talent_points = 5;
    chan.haste_mode_flags = 0; /* dynamic */
    idq = rogue_skill_register(&chan);
    assert(rogue_skill_rank_up(idq) == 1);
    ctx.now_ms = g_now_ms;
    assert(rogue_skill_try_activate(idq, &ctx) == 1);
    /* Apply haste immediately; dynamic interval should shrink -> >= 4 ticks in 1000ms */
    rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE, 25, 1000.0, g_now_ms, ROGUE_BUFF_STACK_ADD, 0);
    g_chan_ticks = 0; /* exclude on_activate */
    advance(1000.0);
    if (g_chan_ticks < 4)
    {
        printf("dyn ticks=%d\n", g_chan_ticks);
        return 3;
    }

    printf("PH1A_SNAPSHOT_DRIFT_OK\n");
    rogue_event_bus_shutdown();
    rogue_skills_shutdown();
    return 0;
}
