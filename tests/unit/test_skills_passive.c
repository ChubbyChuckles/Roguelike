#include "core/app_state.h"
#include "core/skills.h"
#include <assert.h>
#include <stdio.h>

/* Minimal harness: register one passive that contributes to a synergy bucket and one active that
 * uses it. */

enum
{
    SYNERGY_FIRE_POWER = 0
};

static int effect_fireball(const struct RogueSkillDef* def, struct RogueSkillState* st,
                           const RogueSkillCtx* ctx)
{
    (void) def;
    (void) ctx;
    st->uses++;
    return 1;
}

int main(void)
{
    rogue_skills_init();
    /* Define passive skill (no activation, modifies global bonus via rank). */
    RogueSkillDef passive = {0};
    passive.id = -1;
    passive.name = "Pyromancy";
    passive.icon = "icon_pyro";
    passive.max_rank = 5;
    passive.skill_strength = 0;
    passive.base_cooldown_ms = 0.0f;
    passive.cooldown_reduction_ms_per_rank = 0.0f;
    passive.on_activate = NULL;
    passive.is_passive = 1;
    passive.tags = ROGUE_SKILL_TAG_FIRE;
    passive.synergy_id = SYNERGY_FIRE_POWER;
    passive.synergy_value_per_rank = 2;
    RogueSkillDef fireball = {0};
    fireball.id = -1;
    fireball.name = "Fireball";
    fireball.icon = "icon_fire";
    fireball.max_rank = 5;
    fireball.skill_strength = 0;
    fireball.base_cooldown_ms = 3000.0f;
    fireball.cooldown_reduction_ms_per_rank = 250.0f;
    fireball.on_activate = effect_fireball;
    fireball.is_passive = 0;
    fireball.tags = ROGUE_SKILL_TAG_FIRE;
    fireball.synergy_id = -1;
    fireball.synergy_value_per_rank = 0;
    int pid = rogue_skill_register(&passive);
    int fid = rogue_skill_register(&fireball);
    /* Grant talent points and rank up passive to rank 3 */
    g_app.talent_points = 3;
    assert(rogue_skill_rank_up(pid) == 1);
    assert(rogue_skill_rank_up(pid) == 2);
    assert(rogue_skill_rank_up(pid) == 3);
    const RogueSkillState* pst = rogue_skill_get_state(pid);
    assert(pst && pst->rank == 3);
    /* Synergy total should reflect passive contribution (rank 3 * 2 each = 6) */
    assert(rogue_skill_synergy_total(SYNERGY_FIRE_POWER) == 6);
    RogueSkillCtx ctx = {.now_ms = 0};
    /* Rank up fireball */
    g_app.talent_points = 2;
    assert(rogue_skill_rank_up(fid) == 1);
    const RogueSkillState* fst = rogue_skill_get_state(fid);
    assert(fst && fst->rank == 1);
    /* Activate fireball (should succeed) */
    assert(rogue_skill_try_activate(fid, &ctx) == 1);
    /* Fireball would read synergy at runtime; verify still accessible */
    assert(rogue_skill_synergy_total(SYNERGY_FIRE_POWER) == 6);
    printf("PASSIVE_SKILL_TEST_OK\n");
    rogue_skills_shutdown();
    return 0;
}
