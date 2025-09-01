#define SDL_MAIN_HANDLED 1
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include "../../src/entities/player.h"
#include <assert.h>
#include <stdio.h>

static int cb_noop(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return ROGUE_ACT_CONSUMED; /* mark consumed for consistency */
}

int main(void)
{
    rogue_skills_init();
    /* deterministic baseline */
    g_app.player.level = 1;
    g_app.talent_points = 99; /* allow unlocks */
    g_app.player.max_action_points = 100;
    g_app.player.action_points = 100;
    g_app.player.max_mana = 100;
    g_app.player.mana = 100;
    g_app.game_time_ms = 0.0;

    /* Cast skill: costs at begin, interrupt refunds refund_on_cancel_pct of effective costs */
    RogueSkillDef cast = {0};
    cast.name = "PH1_3 Interrupt Cast";
    cast.max_rank = 1;
    cast.base_cooldown_ms = 250.0f;
    cast.on_activate = cb_noop;
    cast.cast_type = 1;             /* cast */
    cast.cast_time_ms = 400.0f;     /* 400ms cast */
    cast.action_point_cost = 10;    /* flat AP */
    cast.resource_cost_mana = 20;   /* flat mana */
    cast.refund_on_cancel_pct = 50; /* 50% refund on cancel/interrupt */

    int idc = rogue_skill_register(&cast);
    assert(rogue_skill_rank_up(idc) == 1);

    RogueSkillCtx ctx = {0};
    ctx.now_ms = 0.0;
    assert(rogue_skill_try_activate(idc, &ctx) == 1); /* begin cast + spend costs */
    assert(g_app.player.action_points == 90);
    assert(g_app.player.mana == 80);
    /* Interrupt mid-cast: expect +5 AP, +10 Mana (50% of 10/20) */
    RogueSkillCtx ictx = {0};
    ictx.now_ms = 100.0;
    assert(rogue_skill_interrupt(idc, &ictx) == 1);
    const RogueSkillState* stc = rogue_skill_get_state(idc);
    assert(stc->interrupted_active == 1);
    assert(rogue_skill_get_exec_state(idc) == ROGUE_SKEXEC_INTERRUPTED);
    assert(g_app.player.action_points == 95);
    assert(g_app.player.mana == 90);

    /* New activation should clear interrupted flag once cooldown elapses */
    RogueSkillCtx ctx2 = {0};
    ctx2.now_ms = stc->cooldown_end_ms + 1.0;
    assert(rogue_skill_try_activate(idc, &ctx2) == 1);
    stc = rogue_skill_get_state(idc);
    assert(stc->interrupted_active == 0);
    assert(rogue_skill_get_exec_state(idc) != ROGUE_SKEXEC_INTERRUPTED);

    /* Channel skill: same refund behavior */
    RogueSkillDef chan = cast;
    chan.name = "PH1_3 Interrupt Channel";
    chan.cast_type = 2;        /* channel */
    chan.cast_time_ms = 600.0; /* 600ms channel */
    int idh = rogue_skill_register(&chan);
    assert(rogue_skill_rank_up(idh) == 1);
    RogueSkillCtx hctx = {0};
    hctx.now_ms = ctx2.now_ms + 1.0;
    /* Reset resources for clean check */
    g_app.player.action_points = 100;
    g_app.player.mana = 100;
    assert(rogue_skill_try_activate(idh, &hctx) == 1);
    assert(g_app.player.action_points == 90);
    assert(g_app.player.mana == 80);
    RogueSkillCtx ihctx = {0};
    ihctx.now_ms = hctx.now_ms + 200.0;
    assert(rogue_skill_interrupt(idh, &ihctx) == 1);
    const RogueSkillState* sth = rogue_skill_get_state(idh);
    assert(sth->interrupted_active == 1);
    assert(rogue_skill_get_exec_state(idh) == ROGUE_SKEXEC_INTERRUPTED);
    assert(g_app.player.action_points == 95);
    assert(g_app.player.mana == 90);

    printf("PH1_3_INTERRUPT_OK ap=%d mana=%d\n", g_app.player.action_points, g_app.player.mana);
    fflush(stdout);
    rogue_skills_shutdown();
    return 0;
}
