#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_coeffs.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Minimal init */
    rogue_skills_init();

    /* Create an active template skill with known timing and coeffs */
    int tid = rogue_skill_debug_create("Tmpl Active A", 4, 1200.0f, -25.0f, 300.0f, 0);
    assert(tid >= 0);
    RogueSkillCoeffParams cpA;
    cpA.base_scalar = 1.25f;
    cpA.per_rank_scalar = 0.10f;
    cpA.str_pct_per10 = 5.0f;
    cpA.int_pct_per10 = 0.0f;
    cpA.dex_pct_per10 = 2.0f;
    cpA.stat_cap_pct = 60.0f;
    cpA.stat_softness = 2.5f;
    assert(rogue_skill_debug_set_coeff(tid, &cpA) == 0);

    /* Emulate the overlay template prefill → create → copy coeffs flow */
    int max_rank = 0, is_passive = -1;
    assert(rogue_skill_debug_get_meta(tid, &max_rank, &is_passive) == 0);
    float bcd = 0.f, red = 0.f, cst = 0.f;
    assert(rogue_skill_debug_get_timing(tid, &bcd, &red, &cst) == 0);

    int cid = rogue_skill_debug_create("Clone From A", max_rank, bcd, red, cst, is_passive);
    assert(cid >= 0);

    RogueSkillCoeffParams fetched;
    assert(rogue_skill_debug_get_coeff(tid, &fetched) == 0);
    assert(rogue_skill_debug_set_coeff(cid, &fetched) == 0);

    /* Verify meta/timing equality */
    const struct RogueSkillDef* dT = rogue_skill_get_def(tid);
    const struct RogueSkillDef* dC = rogue_skill_get_def(cid);
    assert(dT && dC);
    assert(dC->max_rank == dT->max_rank);
    assert(dC->is_passive == dT->is_passive);
    assert(dC->base_cooldown_ms == dT->base_cooldown_ms);
    assert(dC->cooldown_reduction_ms_per_rank == dT->cooldown_reduction_ms_per_rank);
    assert(dC->cast_time_ms == dT->cast_time_ms);

    /* Verify coeffs equality */
    RogueSkillCoeffParams cpc;
    assert(rogue_skill_debug_get_coeff(cid, &cpc) == 0);
    assert(cpc.base_scalar == cpA.base_scalar);
    assert(cpc.per_rank_scalar == cpA.per_rank_scalar);
    assert(cpc.str_pct_per10 == cpA.str_pct_per10);
    assert(cpc.int_pct_per10 == cpA.int_pct_per10);
    assert(cpc.dex_pct_per10 == cpA.dex_pct_per10);
    assert(cpc.stat_cap_pct == cpA.stat_cap_pct);
    assert(cpc.stat_softness == cpA.stat_softness);

    /* Create another clone without copying coeffs and ensure coeffs are absent */
    int cid2 = rogue_skill_debug_create("Clone No Coeff", max_rank, bcd, red, cst, is_passive);
    assert(cid2 >= 0);
    assert(rogue_skill_coeff_exists(cid2) == 0);

    /* Passive template preservation */
    int tpid = rogue_skill_debug_create("Tmpl Passive P", 2, 0.0f, 0.0f, 0.0f, 1);
    assert(tpid >= 0);
    int mr2 = 0, pass2 = 0;
    assert(rogue_skill_debug_get_meta(tpid, &mr2, &pass2) == 0);
    float b2 = 0.f, r2 = 0.f, c2 = 0.f;
    assert(rogue_skill_debug_get_timing(tpid, &b2, &r2, &c2) == 0);
    int cidp = rogue_skill_debug_create("Clone P", mr2, b2, r2, c2, pass2);
    assert(cidp >= 0);
    const struct RogueSkillDef* dP = rogue_skill_get_def(cidp);
    assert(dP && dP->is_passive == 1);

    rogue_skills_shutdown();
    printf("OK test_skill_template_creation tid=%d cid=%d cid2=%d tpid=%d cidp=%d count=%d\n", tid,
           cid, cid2, tpid, cidp, g_app.skill_count);
    return 0;
}
