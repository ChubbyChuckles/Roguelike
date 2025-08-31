#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

static int cb_consume(const RogueSkillDef* def, struct RogueSkillState* st,
                      const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return 1;
}

static int register_buff_spec(unsigned short buff_type, int magnitude, float duration_ms)
{
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_STAT_BUFF;
    es.buff_type = buff_type;
    es.magnitude = magnitude;
    es.duration_ms = duration_ms;
    int id = rogue_effect_register(&es);
    assert(id >= 0);
    return id;
}

int main(void)
{
    rogue_skills_init();
    rogue_buffs_init();
    rogue_effect_reset();
    g_app.talent_points = 1;
    rogue_player_recalc_derived(&g_app.player);

    int primary = register_buff_spec(ROGUE_BUFF_POWER_STRIKE, 1, 1000.0f);
    int node = register_buff_spec(ROGUE_BUFF_STAT_STRENGTH, 2, 1000.0f);

    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = "DurNode";
    def.max_rank = 1;
    def.base_cooldown_ms = 100;
    def.on_activate = cb_consume;
    def.effect_spec_id = primary;
    def.effect_node_count = 1;
    def.effect_nodes[0].effect_spec_id = node;
    def.effect_nodes[0].delay_ms = 50.0f;
    def.effect_nodes[0].repeat_count = 0; /* implicit */
    def.effect_nodes[0].repeat_interval_ms = 100.0f;
    def.effect_nodes[0].duration_ms = 220.0f; /* repeats at 150 and 250 (<= 270?), but window from
                                                 t0=50: 50+220=270 => schedules at 150,250 only */

    int sid = rogue_skill_register(&def);
    assert(sid >= 0);
    assert(rogue_skill_rank_up(sid) == 1);

    RogueSkillCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.now_ms = 0.0;
    assert(rogue_skill_try_activate(sid, &ctx) == 1);

    /* before 50ms none */
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 0);
    rogue_effects_update(49.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 0);
    /* at 50ms first */
    rogue_effects_update(50.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 2);
    /* at 150 second */
    rogue_effects_update(150.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 4);
    /* at 250 third (250 <= 270) */
    rogue_effects_update(250.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 6);
    /* at 350, beyond window, no more */
    rogue_effects_update(350.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 6);

    printf("EFFECT_NODES_DURATION_OK\n");
    rogue_skills_shutdown();
    rogue_effect_reset();
    return 0;
}
