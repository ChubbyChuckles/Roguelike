#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_coeffs.h"
#include "../../src/core/skills/skills_procs.h"
#include "../../src/core/skills/skills_validate.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reset_all(void)
{
    rogue_effect_reset();
    rogue_skills_procs_reset();
    rogue_skills_shutdown();
    rogue_skills_init();
}

int main(void)
{
    /* Init subsystems used by the validator */
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("validator_ext");
    assert(rogue_event_bus_init(&cfg));
    rogue_skills_procs_init();

    char err[256] = {0};
    int rc;

    /* Start from a clean slate */
    reset_all();

    /* Case A: invalid proc.effect_spec_id should be reported */
    RogueProcDef bad_proc = {0};
    bad_proc.event_type = ROGUE_EVENT_SKILL_CHANNEL_TICK;
    bad_proc.effect_spec_id = 987654; /* not registered */
    bad_proc.chance_pct = 100;
    rogue_skills_proc_register(&bad_proc);
    memset(err, 0, sizeof err);
    rc = rogue_skills_validate_all(err, (int) sizeof err);
    assert(rc == -1 && strstr(err, "invalid proc.effect_spec_id") != NULL);

    /* Reset and proceed with valid procs */
    reset_all();

    /* Register a non-offensive (passive) skill with no coeffs; should be OK */
    RogueSkillDef passive = {0};
    passive.name = "Passive OK";
    passive.is_passive = 1;
    passive.max_rank = 1;
    passive.effect_spec_id = -1; /* no effect */
    passive.action_point_cost = 0;
    passive.resource_cost_mana = 0;
    passive.cast_time_ms = 0.0f;
    rogue_skill_register(&passive);
    memset(err, 0, sizeof err);
    rc = rogue_skills_validate_all(err, (int) sizeof err);
    assert(rc == 0);

    /* Control: make an offensive skill and add coeffs so it passes */
    RogueEffectSpec eff = {0};
    eff.kind = ROGUE_EFFECT_STAT_BUFF;
    eff.magnitude = 1;
    int eff_id = rogue_effect_register(&eff);

    RogueSkillDef offensive = {0};
    offensive.name = "Offensive With Coeff";
    offensive.max_rank = 1;
    offensive.action_point_cost = 3;
    offensive.effect_spec_id = eff_id;
    int sid = rogue_skill_register(&offensive);

    RogueSkillCoeffParams p = {0};
    p.base_scalar = 1.0f;
    rogue_skill_coeff_register(sid, &p);

    memset(err, 0, sizeof err);
    rc = rogue_skills_validate_all(err, (int) sizeof err);
    assert(rc == 0);

    printf("validator_ext OK\n");
    rogue_skills_procs_shutdown();
    rogue_event_bus_shutdown();
    return 0;
}
