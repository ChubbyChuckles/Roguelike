/* Phase 2.1–2.3: cost mapping (percent max, per-rank, surcharge) and refunds (miss/resist/cancel)
 */
#define SDL_MAIN_HANDLED 1
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include "../../src/entities/player.h"
#include "../../src/game/buffs.h"
#include <assert.h>
#include <stdio.h>

static int g_flags = 0;
static int cb_flags(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return g_flags; /* direct return of flags */
}

int main(void)
{
    rogue_buffs_init();
    rogue_skills_init();
    /* Ensure derived stats */
    g_app.player.level = 1;
    g_app.talent_points = 99; /* allow rank ups in tests */
    rogue_player_recalc_derived(&g_app.player);
    /* Normalize caps so percent-of-max math and refunds don't clamp unexpectedly */
    g_app.player.max_action_points = 100;
    g_app.player.max_mana = 100;
    g_app.player.action_points = 100; /* deterministic baseline */
    g_app.player.mana = 100;

    /* Define a skill with 10% AP cost + per-rank + AP surcharge when below 50; mana flat 20 +
     * per-rank */
    RogueSkillDef s = {0};
    s.name = "P2 Test";
    s.max_rank = 3;
    s.on_activate = cb_flags;
    s.base_cooldown_ms = 0;
    s.ap_cost_pct_max = 10; /* 10 AP at 100 max */
    s.ap_cost_per_rank = 2; /* +2 per rank beyond 1 */
    s.ap_cost_surcharge_threshold = 50;
    s.ap_cost_surcharge_amount = 5;
    s.mana_cost_pct_max = 0;
    s.resource_cost_mana = 20;
    s.mana_cost_per_rank = 5; /* +5 per rank beyond 1 */
    s.refund_on_miss_pct = 40;
    s.refund_on_resist_pct = 60;
    s.refund_on_cancel_pct = 50;
    int id = rogue_skill_register(&s);
    assert(rogue_skill_rank_up(id) == 1);

    RogueSkillCtx ctx = {0};
    ctx.now_ms = 0.0;
    ctx.player_level = 1;

    /* Rank 1 costs: AP=10, Mana=20. Ensure gating and spend. */
    g_flags = ROGUE_ACT_CONSUMED;
    g_app.player.action_points = 9;
    g_app.player.mana = 100;
    assert(rogue_skill_try_activate(id, &ctx) == 0); /* insufficient AP */
    g_app.player.action_points = 100;
    g_app.player.mana = 19;
    assert(rogue_skill_try_activate(id, &ctx) == 0); /* insufficient mana */
    g_app.player.action_points = 100;
    g_app.player.mana = 100;
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    assert(g_app.player.action_points == 90);
    assert(g_app.player.mana == 80);
    /* Advance past cooldown/global cooldown before next activation */
    ctx.now_ms = rogue_skill_get_state(id)->cooldown_end_ms + 1.0;
    printf("P2: rank1 gating/spend OK\n");
    fflush(stdout);

    /* Rank up to 3: AP cost 10 + (2*(3-1)) = 14; Mana 20 + (5*(3-1)) = 30. */
    assert(rogue_skill_rank_up(id) == 2);
    assert(rogue_skill_rank_up(id) == 3);
    g_app.player.action_points = 100;
    g_app.player.mana = 100;
    printf("DBG: now=%.1f cd_end=%.1f ap=%d max_ap=%d mana=%d max_mana=%d\n", ctx.now_ms,
           rogue_skill_get_state(id)->cooldown_end_ms, g_app.player.action_points,
           g_app.player.max_action_points, g_app.player.mana, g_app.player.max_mana);
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    assert(g_app.player.action_points == 86); /* 100-14 */
    assert(g_app.player.mana == 70);          /* 100-30 */
    ctx.now_ms = rogue_skill_get_state(id)->cooldown_end_ms + 1.0;
    printf("P2: rank3 costs OK\n");
    fflush(stdout);

    /* Surcharge when AP below threshold (<50) adds +5 AP cost. */
    g_app.player.action_points = 49;
    g_app.player.mana = 100;
    assert(rogue_skill_try_activate(id, &ctx) == 1); /* surcharge applies, still enough AP */
    assert(g_app.player.action_points == 30);        /* 49 - (14+5) = 30 */
    assert(g_app.player.mana == 70);                 /* 100 - 30 */
    ctx.now_ms = rogue_skill_get_state(id)->cooldown_end_ms + 1.0;
    g_app.player.action_points = 18;
    g_app.player.mana = 100;
    assert(rogue_skill_try_activate(id, &ctx) == 0);
    ctx.now_ms = rogue_skill_get_state(id)->cooldown_end_ms + 1.0;
    g_app.player.action_points = 100;
    g_app.player.mana = 100;
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    ctx.now_ms = rogue_skill_get_state(id)->cooldown_end_ms + 1.0;
    printf("P2: surcharge and gating OK\n");
    fflush(stdout);

    /* Refunds: set flags to MISSED; expect 40% refund of effective costs. */
    g_flags = ROGUE_ACT_CONSUMED | ROGUE_ACT_MISSED;
    g_app.player.action_points = 100;
    g_app.player.mana = 100;
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    /* Effective costs at rank 3 with AP=100: 14 AP, 30 Mana. 40% refund => +5 AP, +12 Mana. */
    assert(g_app.player.action_points == (100 - 14 + 5));
    assert(g_app.player.mana == (100 - 30 + 12));
    ctx.now_ms = rogue_skill_get_state(id)->cooldown_end_ms + 1.0;
    printf("P2: refund on miss OK\n");
    fflush(stdout);

    /* Resist refund 60% */
    g_flags = ROGUE_ACT_CONSUMED | ROGUE_ACT_RESISTED;
    g_app.player.action_points = 100;
    g_app.player.mana = 100;
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    assert(g_app.player.action_points == (100 - 14 + 8)); /* 60% of 14 -> 8 (int) */
    assert(g_app.player.mana == (100 - 30 + 18));         /* 60% of 30 -> 18 */
    ctx.now_ms = rogue_skill_get_state(id)->cooldown_end_ms + 1.0;
    printf("P2: refund on resist OK\n");
    fflush(stdout);

    /* Early cancel refund scaled by progress: start a cast skill with cancel.
       Reuse same def but as cast with 400ms and cancel at 200ms => 50% of refund% (50%). */
    RogueSkillDef c = s;
    c.cast_type = 1;
    c.cast_time_ms = 400.0f;
    c.on_activate = cb_flags;
    int idc = rogue_skill_register(&c);
    assert(rogue_skill_rank_up(idc) == 1);
    g_flags = ROGUE_ACT_CONSUMED; /* no miss/resist here */
    g_app.player.action_points = 100;
    g_app.player.mana = 100;
    /* Ensure we're past any global cooldown before starting the cast */
    ctx.now_ms = rogue_skill_get_state(id)->cooldown_end_ms + 1.0;
    int began = rogue_skill_try_activate(idc, &ctx);
    printf("DBG: begin cast=%d at now=%.1f cd_end(c)=%.1f\n", began, ctx.now_ms,
           rogue_skill_get_state(idc)->cooldown_end_ms);
    assert(began == 1); /* begin cast */
    /* Advance update half cast duration and cancel */
    RogueSkillCtx cctx = {0};
    cctx.now_ms = ctx.now_ms + 200.0;
    int canceled = rogue_skill_try_cancel(idc, &cctx);
    printf("DBG: cancel=%d at now=%.1f (progress=~50%%) ap=%d mana=%d\n", canceled, cctx.now_ms,
           g_app.player.action_points, g_app.player.mana);
    assert(canceled == 1);
    /* Base costs from def for cancel refund: at rank1 AP=10, Mana=20, refund_on_cancel=50% ->
     * refund 5 AP, 10 Mana scaled by unspent 50% -> 2 and 5 */
    assert(g_app.player.action_points >= 92 && g_app.player.action_points <= 95);
    assert(g_app.player.mana >= 85 && g_app.player.mana <= 90);
    printf("P2: cancel refund OK\n");
    fflush(stdout);

    rogue_skills_shutdown();
    return 0;
}
