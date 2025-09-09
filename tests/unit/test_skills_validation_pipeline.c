#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_coeffs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    rogue_skills_init();

    /* Create an offensive-looking skill (cast_time_ms > 0) with no coeffs */
    RogueSkillDef d = {0};
    d.name = "ValSkill";
    d.max_rank = 1;
    d.cast_time_ms = 100.0f; /* offensive heuristic */
    int id = rogue_skill_register(&d);
    if (id < 0)
    {
        printf("skill register failed\n");
        rogue_skills_shutdown();
        return 99;
    }

    char err[256] = {0};
    int rc = rogue_skill_debug_validate(err, (int) sizeof err);
    /* Expect failure due to missing coefficient entry */
    if (rc == 0)
    {
        printf("unexpected validation success: %s\n", err);
        return 1;
    }

    /* Add a coefficient entry, then validation should succeed */
    RogueSkillCoeffParams cp = {0};
    cp.base_scalar = 1.0f;
    if (rogue_skill_coeff_register(id, &cp) != 0)
    {
        printf("coeff register failed\n");
        rogue_skills_shutdown();
        return 98;
    }

    memset(err, 0, sizeof err);
    rc = rogue_skill_debug_validate(err, (int) sizeof err);
    if (rc != 0)
    {
        printf("validation still failing: %s\n", err);
        return 2;
    }

    rogue_skills_shutdown();
    printf("OK skills_validation_pipeline\n");
    return 0;
}
