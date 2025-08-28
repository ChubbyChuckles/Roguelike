#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h" /* public skills API (init/shutdown/register) */
#include "../../src/core/skills/skills_coeffs.h"
#include "../../src/core/skills/skills_procs.h"
#include "../../src/core/skills/skills_validate.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int make_effect_simple(void)
{
    RogueEffectSpec s;
    memset(&s, 0, sizeof s);
    s.kind = ROGUE_EFFECT_STAT_BUFF;
    s.buff_type = 1; /* arbitrary */
    s.magnitude = 5;
    s.duration_ms = 1000.0f;
    s.stack_rule = 1; /* ADD */
    return rogue_effect_register(&s);
}

static int make_skill_with_effect(int eff)
{
    RogueSkillDef d;
    memset(&d, 0, sizeof d);
    d.name = "Test Skill";
    d.max_rank = 3;
    d.action_point_cost = 5;
    d.effect_spec_id = eff;
    return rogue_skill_register(&d);
}

static void add_coeff_for_skill(int sid)
{
    RogueSkillCoeffParams p;
    memset(&p, 0, sizeof p);
    p.base_scalar = 1.0f;
    rogue_skill_coeff_register(sid, &p);
}

int main(void)
{
    /* Init event bus + procs so proc registration works. */
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("validator");
    assert(rogue_event_bus_init(&cfg));
    rogue_skills_procs_init();

    /* Clean registries that tests touch. */
    rogue_effect_reset();
    rogue_skills_procs_reset();
    /* Skills registry init/cleanup baked into test harness elsewhere; ensure empty state here. */
    rogue_skills_shutdown();
    rogue_skills_init();

    /* Case 1: invalid skill effect id should fail. */
    RogueSkillDef bad;
    memset(&bad, 0, sizeof bad);
    bad.name = "Bad";
    bad.effect_spec_id = 123; /* not registered */
    rogue_skill_register(&bad);
    char err[256] = {0};
    int rc = rogue_skills_validate_all(err, (int) sizeof err);
    assert(rc == -1 && strstr(err, "invalid skill.effect_spec_id") != NULL);

    /* Reset skills and proceed with valid entries. */
    rogue_skills_shutdown();
    rogue_skills_init();

    int eff = make_effect_simple();
    int sid = make_skill_with_effect(eff);

    /* Case 2: missing coefficients for offensive skill should fail. */
    memset(err, 0, sizeof err);
    rc = rogue_skills_validate_all(err, (int) sizeof err);
    assert(rc == -1 && strstr(err, "no coefficient entry") != NULL);

    /* Add coeffs to pass this gate. */
    add_coeff_for_skill(sid);

    /* Case 3: duplicate proc pair (same event/effect) should fail. */
    RogueProcDef p = {0};
    p.event_type = ROGUE_EVENT_SKILL_CHANNEL_TICK;
    p.effect_spec_id = eff;
    p.chance_pct = 100;
    rogue_skills_proc_register(&p);
    rogue_skills_proc_register(&p); /* duplicate */
    memset(err, 0, sizeof err);
    rc = rogue_skills_validate_all(err, (int) sizeof err);
    assert(rc == -1 && strstr(err, "duplicate proc pair") != NULL);

    /* Remove procs by reset, then full valid pass. */
    rogue_skills_procs_reset();
    memset(err, 0, sizeof err);
    rc = rogue_skills_validate_all(err, (int) sizeof err);
    assert(rc == 0);

    printf("PH10.3 validator OK\n");
    /* Shutdown subsystems initialized in this test. */
    rogue_skills_procs_shutdown();
    rogue_event_bus_shutdown();
    return 0;
}
