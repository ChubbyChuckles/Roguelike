/* Test minimal EffectSpec integration: registers a strength buff spec and links via skill */
#include "../../src/core/app_state.h"
#include "../../src/core/buffs.h"
#include "../../src/core/effect_spec.h"
#include "../../src/core/skills/skills.h"
#include "../../src/entities/player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int cb_noop(const RogueSkillDef* def, struct RogueSkillState* st,
                   const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return 1;
}

int main(void)
{
    rogue_skills_init();
    rogue_buffs_init();
    rogue_effect_reset();
    g_app.talent_points = 1; /* allow rank up */
    rogue_player_recalc_derived(&g_app.player);
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_STAT_BUFF;
    es.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    es.magnitude = 7;
    es.duration_ms = 3000.0f;
    int eff_id = rogue_effect_register(&es);
    assert(eff_id >= 0);
    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = "Battle Cry";
    def.max_rank = 1;
    def.base_cooldown_ms = 500;
    def.on_activate = cb_noop;
    def.effect_spec_id = eff_id;
    def.action_point_cost = 0;
    def.resource_cost_mana = 0;
    int sid = rogue_skill_register(&def);
    assert(sid >= 0);
    assert(rogue_skill_rank_up(sid) == 1);
    RogueSkillCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.now_ms = 0.0;
    ctx.player_level = 1;
    ctx.talent_points = 0;
    assert(rogue_skill_try_activate(sid, &ctx) == 1);
    int total = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    assert(total == 7);
    printf("EFFECTSPEC_OK buff_total=%d id=%d\n", total, eff_id);
    rogue_skills_shutdown();
    rogue_effect_reset();
    return 0;
}
