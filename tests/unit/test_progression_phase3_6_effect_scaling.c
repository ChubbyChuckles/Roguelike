#include "../../src/core/app_state.h"
#include "../../src/core/damage_calc.h"
#include "../../src/core/progression/progression_synergy.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/stat_cache.h"
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
    /* Initialize minimal systems used */
    rogue_skills_init();
    /* Player baseline */
    g_app.player.level = 10;
    g_app.player.intelligence = 12; /* base INT */
    g_app.player.crit_rating = 0;
    g_app.player.haste_rating = 0;
    /* Force stat cache recompute to populate totals */
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_force_update(&g_app.player);

    /* Define a passive that contributes to FIRE_POWER synergy */
    RogueSkillDef fire_mastery = {0};
    fire_mastery.id = -1;
    fire_mastery.name = "FireMastery";
    fire_mastery.icon = "fm";
    fire_mastery.max_rank = 3;
    fire_mastery.is_passive = 1;
    fire_mastery.synergy_id = ROGUE_SYNERGY_FIRE_POWER;
    fire_mastery.synergy_value_per_rank = 2; /* per rank adds flat fire damage */

    /* Active fireball */
    RogueSkillDef fireball = {0};
    fireball.id = -1;
    fireball.name = "Fireball";
    fireball.icon = "fb";
    fireball.max_rank = 5;
    fireball.base_cooldown_ms = 6000.0f;
    fireball.cooldown_reduction_ms_per_rank = 400.0f;
    fireball.on_activate = effect_noop;
    fireball.tags = ROGUE_SKILL_TAG_FIRE;

    int mastery_id = rogue_skill_register(&fire_mastery);
    int fireball_id = rogue_skill_register(&fireball);
    g_app.talent_points = 5;
    assert(rogue_skill_rank_up(mastery_id) == 1);
    assert(rogue_skill_rank_up(mastery_id) == 2); /* synergy = 4 */
    assert(rogue_skill_rank_up(fireball_id) == 1);

    /* With base INT=12, expected INT bonus = floor(12*0.25)=3. */
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_force_update(&g_app.player);
    int dmg = rogue_damage_fireball(fireball_id);
    /* base 3 + rank*2 (2) + synergy(4) + int_bonus(3) = 12 */
    assert(dmg == 12);

    /* Increase INT and ensure scaling applies */
    g_app.player.intelligence = 28; /* +16 -> int bonus 7 */
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_force_update(&g_app.player);
    int dmg2 = rogue_damage_fireball(fireball_id);
    /* base (5) + synergy (4) + int (7) = 16 */
    assert(dmg2 == 16);

    /* Cooldown scaling: set effective haste percent via stat cache fields. */
    /* We emulate 40% effective CDR (below soft cap), expect cd * 0.6 after rank reduction. */
    const RogueSkillState* st = rogue_skill_get_state(fireball_id);
    assert(st && st->rank == 1);
    /* Base cd for rank1 = 6000ms; haste_eff=40 -> cd*0.6 -> 3600ms */
    g_player_stat_cache.rating_haste_eff_pct = 40;
    float cd = rogue_cooldown_fireball_ms(fireball_id);
    assert((int) cd == 3600);

    printf("PH3_6_EFFECT_SCALING_OK dmg1=%d dmg2=%d cd=%d\n", dmg, dmg2, (int) cd);
    rogue_skills_shutdown();
    return 0;
}
