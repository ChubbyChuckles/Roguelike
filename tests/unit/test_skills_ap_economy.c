/* Phase 1.5 Action Point economy basic test */
#include <assert.h>
#include <stdio.h>
#include "core/skills.h"
#include "core/app_state.h"
#include "core/player_progress.h"
#include "entities/player.h"

static int cb_dummy(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx){(void)def;(void)st;(void)ctx; return 1;}

int main(void){
    rogue_skills_init();
    g_app.talent_points = 2; g_app.player.mana = 0; /* no mana cost path */
    /* Ensure player derived stats (including AP) are initialized */
    rogue_player_recalc_derived(&g_app.player);
    assert(g_app.player.action_points > 0);
    int start_ap = g_app.player.action_points;
    RogueSkillDef s = {0};
    s.name="Whirl"; s.max_rank=1; s.base_cooldown_ms=0; s.cooldown_reduction_ms_per_rank=0; s.on_activate=cb_dummy; s.is_passive=0; s.action_point_cost=30; s.resource_cost_mana=0; s.max_charges=0;
    int id = rogue_skill_register(&s); assert(id>=0);
    assert(rogue_skill_rank_up(id)==1);
    RogueSkillCtx ctx={0}; ctx.now_ms=0; ctx.player_level=1; ctx.talent_points=g_app.talent_points; ctx.rng_state=0;
    /* Direct gating checks */
    g_app.player.action_points = 29; /* below cost */
    assert(rogue_skill_try_activate(id,&ctx)==0);
    g_app.player.action_points = 30; /* exact cost */
    assert(rogue_skill_try_activate(id,&ctx)==1); /* first activation */
    int after_first = g_app.player.action_points; /* should be 0 */
    assert(after_first <= 0);
    /* Regen some AP */
    for(int i=0;i<50;i++){ rogue_player_progress_update(0.1); }
    int regen_ap_mid = g_app.player.action_points; assert(regen_ap_mid > after_first);
    /* Spend again after regen if enough */
    if(g_app.player.action_points >= 30){
        ctx.now_ms += 1100; /* ensure cooldown over under test macro */
        assert(rogue_skill_try_activate(id,&ctx)==1);
    }
    int after_second = g_app.player.action_points;
    printf("AP_ECON_OK start=%d after_first=%d regen_mid=%d after_second=%d uses=%d ap_spent=%d\n", start_ap, after_first, regen_ap_mid, after_second, rogue_skill_get_state(id)->uses, rogue_skill_get_state(id)->action_points_spent_session);
    fflush(stdout);
    rogue_skills_shutdown();
    return 0;
}
