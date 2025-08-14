#include "core/skills.h"
#include "core/app_state.h"
#include <assert.h>
#include <stdio.h>

/* Minimal harness: register one passive that contributes to a synergy bucket and one active that uses it. */

enum { SYNERGY_FIRE_POWER = 0 };

static int effect_fireball(const struct RogueSkillDef* def, struct RogueSkillState* st, const RogueSkillCtx* ctx){ (void)def;(void)ctx; st->uses++; return 1; }

int main(void){
    rogue_skills_init();
    /* Define passive skill (no activation, modifies global bonus via rank). */
    RogueSkillDef passive = {-1,"Pyromancy","icon_pyro",5,0,0,NULL,1,ROGUE_SKILL_TAG_FIRE,SYNERGY_FIRE_POWER,2};
    RogueSkillDef fireball = {-1,"Fireball","icon_fire",5,3000,250,effect_fireball,0,ROGUE_SKILL_TAG_FIRE,-1,0};
    int pid = rogue_skill_register(&passive);
    int fid = rogue_skill_register(&fireball);
    /* Grant talent points and rank up passive to rank 3 */
    g_app.talent_points = 3;
    assert(rogue_skill_rank_up(pid)==1);
    assert(rogue_skill_rank_up(pid)==2);
    assert(rogue_skill_rank_up(pid)==3);
    const RogueSkillState* pst = rogue_skill_get_state(pid);
    assert(pst && pst->rank==3);
    /* Synergy total should reflect passive contribution (rank 3 * 2 each = 6) */
    assert(rogue_skill_synergy_total(SYNERGY_FIRE_POWER)==6);
    RogueSkillCtx ctx={.now_ms=0};
    /* Rank up fireball */
    g_app.talent_points = 2;
    assert(rogue_skill_rank_up(fid)==1);
    const RogueSkillState* fst = rogue_skill_get_state(fid);
    assert(fst && fst->rank==1);
    /* Activate fireball (should succeed) */
    assert(rogue_skill_try_activate(fid,&ctx)==1);
    /* Fireball would read synergy at runtime; verify still accessible */
    assert(rogue_skill_synergy_total(SYNERGY_FIRE_POWER)==6);
    printf("PASSIVE_SKILL_TEST_OK\n");
    rogue_skills_shutdown();
    return 0;
}
