#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_coeffs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    rogue_skills_init();
    /* Two skills */
    RogueSkillDef a = {0};
    a.name = "A";
    a.max_rank = 2;
    a.base_cooldown_ms = 500.0f;
    RogueSkillDef b = {0};
    b.name = "B";
    b.max_rank = 3;
    b.base_cooldown_ms = 800.0f;
    int ia = rogue_skill_register(&a);
    int ib = rogue_skill_register(&b);
    assert(ia == 0 && ib == 1);

    /* Set coeff for A */
    RogueSkillCoeffParams p = {0};
    p.base_scalar = 1.25f;
    p.per_rank_scalar = 0.05f;
    p.str_pct_per10 = 3.0f;
    p.stat_cap_pct = 40.0f;
    p.stat_softness = 15.0f;
    assert(rogue_skill_debug_set_coeff(ia, &p) == 0);
    /* Modify timings for B */
    assert(rogue_skill_debug_set_timing(ib, 600.0f, -25.0f, 200.0f) == 0);

    /* Export */
    char json[2048];
    int n = rogue_skill_debug_export_overrides_json(json, (int) sizeof json);
    assert(n > 0);
    assert(json[0] == '[');

    /* Zero out defs, then reload from JSON and verify restore */
    g_app.skill_defs[0].base_cooldown_ms = 0.f;
    g_app.skill_defs[0].cooldown_reduction_ms_per_rank = 0.f;
    g_app.skill_defs[0].cast_time_ms = 0.f;
    g_app.skill_defs[1].base_cooldown_ms = 0.f;
    g_app.skill_defs[1].cooldown_reduction_ms_per_rank = 0.f;
    g_app.skill_defs[1].cast_time_ms = 0.f;

    int applied = rogue_skill_debug_load_overrides_text(json);
    assert(applied >= 2);

    float base_cd = 0, cd_red = 0, cast_ms = 0;
    assert(rogue_skill_debug_get_timing(1, &base_cd, &cd_red, &cast_ms) == 0);
    assert(base_cd == 600.0f);
    RogueSkillCoeffParams q;
    assert(rogue_skill_debug_get_coeff(0, &q) == 0);
    assert(q.base_scalar == p.base_scalar);

    rogue_skills_shutdown();
    printf("OK overrides_roundtrip\n");
    return 0;
}
