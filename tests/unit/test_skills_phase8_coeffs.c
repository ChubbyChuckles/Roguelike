#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_coeffs.h"
#include "../../src/game/stat_cache.h"
#include <assert.h>
#include <stdio.h>

static int effect_noop(const struct RogueSkillDef* def, struct RogueSkillState* st,
                       const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return 1;
}

int main(void)
{
    rogue_skills_init();
    /* Baseline player stats */
    g_app.player.level = 20;
    g_app.player.strength = 30;     /* 30 STR */
    g_app.player.intelligence = 12; /* 12 INT */
    g_app.player.dexterity = 18;    /* 18 DEX */
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_force_update(&g_app.player);

    /* Register a skill and rank it */
    RogueSkillDef s = {0};
    s.id = -1;
    s.name = "CoeffSkill";
    s.icon = "icon";
    s.max_rank = 5;
    s.on_activate = effect_noop;
    int sid = rogue_skill_register(&s);
    assert(sid >= 0);
    /* Provide talent points for rank ups */
    g_app.talent_points = 3;
    assert(rogue_skill_rank_up(sid) == 1);
    assert(rogue_skill_rank_up(sid) == 2);

    /* Register coefficient params: base=1.1, +0.05 per rank beyond 1 -> rank2 => 1.15.
       Stat contributions: STR 2% per 10, DEX 1% per 10, soft cap 50% with softness 30. */
    RogueSkillCoeffParams p = {0};
    p.base_scalar = 1.10f;
    p.per_rank_scalar = 0.05f;
    p.str_pct_per10 = 2.0f;
    p.dex_pct_per10 = 1.0f;
    p.int_pct_per10 = 0.0f;
    p.stat_cap_pct = 50.0f;
    p.stat_softness = 30.0f;
    assert(rogue_skill_coeff_register(sid, &p) == 0);

    float coeff = skill_get_effective_coefficient(sid);
    /* Rank2 base scalar = 1.15. Stat adds: STR 30=> +6%, DEX 18=>+1.8% (pre-cap). */
    /* Total multiplier ~ 1.15 * (1 + 0.078) = 1.15 * 1.078 = 1.2397 */
    assert(coeff > 1.22f && coeff < 1.26f);

    /* Increase STR significantly to test soft cap behavior. */
    g_app.player.strength = 300; /* would be +60% pre-cap -> capped ~<50% */
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_force_update(&g_app.player);
    coeff = skill_get_effective_coefficient(sid);
    /* Expect capped contribution: ~1.15 * (1 + <=0.50 + small dex) < 1.80 */
    assert(coeff < 1.80f);

    /* Lower stats to near zero -> close to base scalar. */
    g_app.player.strength = 0;
    g_app.player.dexterity = 0;
    g_app.player.intelligence = 0;
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_force_update(&g_app.player);
    coeff = skill_get_effective_coefficient(sid);
    /* Approximately the rank2 base (no mastery/spec in this test path). */
    assert(coeff > 1.13f && coeff < 1.17f);

    printf("PH8_COEFFS_OK coeff=%.3f\n", coeff);
    rogue_skills_shutdown();
    return 0;
}
