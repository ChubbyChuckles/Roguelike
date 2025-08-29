#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_coeffs.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Minimal init: skills init populates globals; register two dummy skills */
    rogue_skills_init();

    RogueSkillDef d1 = {0};
    d1.name = "Fireball";
    d1.max_rank = 5;
    d1.base_cooldown_ms = 1000.f;
    RogueSkillDef d2 = {0};
    d2.name = "Frostbolt";
    d2.max_rank = 3;
    d2.base_cooldown_ms = 800.f;
    int id1 = rogue_skill_register(&d1);
    int id2 = rogue_skill_register(&d2);
    assert(id1 == 0 && id2 == 1);

    assert(rogue_skill_debug_count() >= 2);
    assert(rogue_skill_debug_name(0) != NULL);

    /* Coeff get/set */
    RogueSkillCoeffParams p = {0};
    p.base_scalar = 1.2f;
    p.per_rank_scalar = 0.1f;
    p.str_pct_per10 = 5.0f;
    p.stat_cap_pct = 50.0f;
    p.stat_softness = 2.0f;
    assert(rogue_skill_debug_set_coeff(id1, &p) == 0);
    RogueSkillCoeffParams q;
    assert(rogue_skill_debug_get_coeff(id1, &q) == 0);
    assert(q.base_scalar == p.base_scalar);

    /* Timing get/set */
    float base_cd = 0, cd_red = 0, cast_ms = 0;
    assert(rogue_skill_debug_get_timing(id2, &base_cd, &cd_red, &cast_ms) == 0);
    assert(base_cd == 800.0f);
    assert(rogue_skill_debug_set_timing(id2, 600.0f, -50.0f, 250.0f) == 0);
    base_cd = cd_red = cast_ms = 0;
    assert(rogue_skill_debug_get_timing(id2, &base_cd, &cd_red, &cast_ms) == 0);
    assert(base_cd == 600.0f);
    assert(cast_ms == 250.0f);

    /* Simulate wrapper (empty profile should fail; valid should succeed) */
    char out[256];
    assert(rogue_skill_debug_simulate("{\"duration_ms\":200,\"priority\":[0,1]}", out,
                                      (int) sizeof(out)) == 0);

    rogue_skills_shutdown();
    printf("OK skill_debug_api\n");
    return 0;
}
