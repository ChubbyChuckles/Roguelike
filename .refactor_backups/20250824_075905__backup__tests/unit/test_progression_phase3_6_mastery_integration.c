#include "../../src/core/app/app_state.h"
#include "../../src/core/progression/progression_mastery.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/stat_cache.h"
#include "../../src/game/damage_calc.h"
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
    /* Init subsystems */
    rogue_skills_init();
    if (rogue_mastery_init(0, 0) < 0)
    {
        printf("mastery_init_fail\n");
        return 1;
    }

    /* Player baseline & stat cache */
    g_app.player.level = 20;
    g_app.player.intelligence = 16; /* int bonus = 4 */
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_force_update(&g_app.player);

    /* Define a tiny fire synergy passive and a Fireball active */
    RogueSkillDef fire_passive = {0};
    fire_passive.id = -1;
    fire_passive.name = "FirePassive";
    fire_passive.icon = "fp";
    fire_passive.max_rank = 1;
    fire_passive.is_passive = 1;
    fire_passive.synergy_id = ROGUE_SYNERGY_FIRE_POWER;
    fire_passive.synergy_value_per_rank = 2; /* +2 flat */

    RogueSkillDef fireball = {0};
    fireball.id = -1;
    fireball.name = "Fireball";
    fireball.icon = "fb";
    fireball.max_rank = 5;
    fireball.base_cooldown_ms = 6000.0f;
    fireball.cooldown_reduction_ms_per_rank = 400.0f;
    fireball.on_activate = effect_noop;
    fireball.tags = ROGUE_SKILL_TAG_FIRE;

    int pid = rogue_skill_register(&fire_passive);
    int fid = rogue_skill_register(&fireball);

    g_app.talent_points = 3;
    assert(rogue_skill_rank_up(pid) == 1); /* synergy +2 */
    assert(rogue_skill_rank_up(fid) == 1);

    /* Baseline damage (no mastery): base 3+2 + synergy 2 + INT 4 = 11 */
    int dmg0 = rogue_damage_fireball(fid);
    assert(dmg0 == 11);

    /* Add mastery XP to raise mastery rank and verify multiplicative bonus applies. */
    /* Rank 1 mastery bonus scalar = 1.01, but thresholds require 100 xp for rank 1. */
    unsigned int t = 0;
    rogue_mastery_add_xp(fid, 50, t);
    t += 10;
    rogue_mastery_add_xp(fid, 60, t); /* total 110 -> rank 1 */
    /* With 1.01 scalar, 11 * 1.01 = 11.11 -> rounds to 11 */
    int dmg1 = rogue_damage_fireball(fid);
    assert(dmg1 == 11);

    /* Push to rank >=3 for 1.03 scalar: need more xp. */
    while (rogue_mastery_rank(fid) < 3)
    {
        rogue_mastery_add_xp(fid, 300, t);
        t += 20;
    }
    int dmg2 = rogue_damage_fireball(fid);
    /* 11 * 1.03 = 11.33 -> rounds to 11; bump INT to amplify */
    g_app.player.intelligence = 28; /* int bonus now 7 -> base 3+2 +2 +7 = 14 */
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_force_update(&g_app.player);
    int dmg3 = rogue_damage_fireball(fid);
    /* 14 * 1.03 = 14.42 -> rounds to 14 */
    assert(dmg3 == 14);

    printf("PH3_6_MASTERY_INTEG_OK dmg0=%d dmg1=%d dmg2=%d dmg3=%d rank=%u\n", dmg0, dmg1, dmg2,
           dmg3, (unsigned) rogue_mastery_rank(fid));

    rogue_mastery_shutdown();
    rogue_skills_shutdown();
    return 0;
}
