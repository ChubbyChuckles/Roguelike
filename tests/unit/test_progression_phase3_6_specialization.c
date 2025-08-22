#include "core/app_state.h"
#include "core/damage_calc.h"
#include "core/progression/progression_attributes.h"
#include "core/progression/progression_specialization.h"
#include "core/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    memset(&g_app, 0, sizeof g_app);
    /* Minimal setup: one active skill with base cooldown and rank */
    rogue_skills_init();
    RogueSkillDef d = {0};
    d.name = "SpecFireball";
    d.max_rank = 5;
    d.base_cooldown_ms = 4000.0f;
    int sid = rogue_skill_register(&d);
    assert(sid >= 0);
    /* Meet initial unlock gate (Phase 3.6.2 requires level >= 1 when strength==0) */
    g_app.player.level = 1;
    g_app.talent_points = 1;
    int r = rogue_skill_rank_up(sid);
    assert(r == 1);

    /* Initialize specialization system */
    assert(rogue_specialization_init(0) == 0);

    /* Baseline: no spec => neutral scalars */
    int dmg0 = rogue_damage_fireball(sid);
    float cd0 = rogue_cooldown_fireball_ms(sid);

    /* Choose POWER path: +10% damage */
    assert(rogue_specialization_choose(sid, ROGUE_SPEC_POWER) == 0);
    int dmg1 = rogue_damage_fireball(sid);
    assert(dmg1 >= dmg0);

    /* Switch to CONTROL: should fail without respec (already chosen) */
    assert(rogue_specialization_choose(sid, ROGUE_SPEC_CONTROL) == -2);

    /* Respec: grant one token and clear */
    g_attr_state.respec_tokens = 1;
    assert(rogue_specialization_respec(sid) == 0);
    assert(rogue_specialization_get(sid) == ROGUE_SPEC_NONE);

    /* Choose CONTROL and verify cooldown reduced */
    assert(rogue_specialization_choose(sid, ROGUE_SPEC_CONTROL) == 0);
    float cd1 = rogue_cooldown_fireball_ms(sid);
    assert(cd1 < cd0);

    printf("PH3_6_SPECIALIZATION_OK dmg0=%d dmg1=%d cd0=%.0f cd1=%.0f\n", dmg0, dmg1, cd0, cd1);
    rogue_specialization_shutdown();
    rogue_skills_shutdown();
    return 0;
}
