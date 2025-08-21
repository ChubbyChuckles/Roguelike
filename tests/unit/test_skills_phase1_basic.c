#include "core/app_state.h"
#include "core/skills.h"
#include "entities/player.h"
#include <assert.h>
#include <stdio.h>

static int cb_dummy(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) def;
    (void) ctx;
    return 1;
}

int main(void)
{
    rogue_skills_init();
    g_app.talent_points = 5;
    g_app.player.mana = 50;
    RogueSkillDef s = {0};
    s.name = "Charged Bolt";
    s.max_rank = 3;
    s.base_cooldown_ms = 1200;
    s.cooldown_reduction_ms_per_rank = 200;
    s.on_activate = cb_dummy;
    s.is_passive = 0;
    s.tags = ROGUE_SKILL_TAG_ARCANE;
    s.max_charges = 2;
    s.charge_recharge_ms = 800;
    s.resource_cost_mana = 5;
    int id = rogue_skill_register(&s);
    assert(id >= 0);
    assert(rogue_skill_rank_up(id) == 1);
    RogueSkillCtx ctx = {0};
    ctx.now_ms = 0;
    ctx.player_level = 1;
    ctx.talent_points = g_app.talent_points;
    ctx.rng_state = 0;
    /* First activation */
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    const RogueSkillState* st = rogue_skill_get_state(id);
    assert(st->charges_cur == 1);
    /* Attempt during cooldown should fail */
    ctx.now_ms = 100;
    assert(rogue_skill_try_activate(id, &ctx) == 0);
    /* Advance to just before recharge (cooldown still running) */
    ctx.now_ms = 700;
    rogue_skills_update(ctx.now_ms);
    st = rogue_skill_get_state(id);
    assert(st->charges_cur == 1);
    /* Advance beyond first recharge time (charge should regen though cooldown not done) */
    ctx.now_ms = 820;
    rogue_skills_update(ctx.now_ms);
    st = rogue_skill_get_state(id);
    assert(st->charges_cur == 2);
    /* Cooldown still not done => activation still blocked */
    assert(rogue_skill_try_activate(id, &ctx) == 0);
    /* Advance beyond cooldown end */
    ctx.now_ms = 1300;
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    st = rogue_skill_get_state(id);
    assert(st->charges_cur == 1);
    /* Drain mana to below cost and ensure rejection */
    g_app.player.mana = 4;
    ctx.now_ms = 2600;
    assert(rogue_skill_try_activate(id, &ctx) == 0);
    printf("PH1_BASIC_OK uses=%d charges=%d mana=%d cooldown_end=%.0f\n", st->uses, st->charges_cur,
           g_app.player.mana, st->cooldown_end_ms);
    fflush(stdout);
    rogue_skills_shutdown();
    return 0;
}
