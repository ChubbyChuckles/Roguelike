#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Minimal init */
    rogue_skills_init();

    /* Create a new active skill */
    int idx = rogue_skill_debug_create("Test Active", 3, 1500.0f, -50.0f, 250.0f, 0);
    assert(idx >= 0);
    const struct RogueSkillDef* d = rogue_skill_get_def(idx);
    assert(d && d->id == idx);
    assert(d->max_rank == 3);
    assert(d->cast_time_ms == 250.0f);

    /* Create a passive with same name should fail (duplicate) */
    int dup = rogue_skill_debug_create("Test Active", 1, 0.0f, 0.0f, 0.0f, 1);
    assert(dup == -2);

    /* Create a passive skill */
    int idx2 = rogue_skill_debug_create("Test Passive", 1, 0.0f, 0.0f, 0.0f, 1);
    assert(idx2 >= 0);
    const struct RogueSkillDef* d2 = rogue_skill_get_def(idx2);
    assert(d2 && d2->is_passive == 1);

    rogue_skills_shutdown();
    printf("OK test_skill_creation_wizard idx=%d idx2=%d count=%d\n", idx, idx2, g_app.skill_count);
    return 0;
}
