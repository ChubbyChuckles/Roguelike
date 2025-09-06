#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include "../../src/entities/player.h"
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

/* Verifies hierarchical scheduling: root -> child chain with relative delays and repeat window */
static int cb_consume(const RogueSkillDef* def, struct RogueSkillState* st,
                      const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return 1;
}

static int reg_buff(unsigned short bt, int mag, float dur)
{
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_STAT_BUFF;
    es.buff_type = bt;
    es.magnitude = mag;
    es.duration_ms = dur;
    int id = rogue_effect_register(&es);
    assert(id >= 0);
    return id;
}

int main(void)
{
    rogue_skills_init();
    rogue_buffs_init();
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("skill_tree_sched_bus");
    assert(rogue_event_bus_init(&cfg));
    rogue_effect_reset();
    g_app.talent_points = 1;
    rogue_player_recalc_derived(&g_app.player);

    /* Use non-zero durations so applied buffs persist for assertions. */
    int root_eff = reg_buff(ROGUE_BUFF_POWER_STRIKE, 1, 1000);
    int child1_eff = reg_buff(ROGUE_BUFF_STAT_STRENGTH, 2, 1000);
    /* Use another STRENGTH buff node to represent a distinct child; stacking adds magnitude */
    int child2_eff = reg_buff(ROGUE_BUFF_STAT_STRENGTH, 3, 1000);

    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = "TreeSched";
    def.max_rank = 1;
    def.base_cooldown_ms = 100;
    def.on_activate = cb_consume;
    def.effect_spec_id = root_eff;
    def.effect_tree_node_count = 3;
    /* node0: root effect immediate +0 */
    def.effect_tree_nodes[0].effect_spec_id = root_eff;
    def.effect_tree_nodes[0].delay_ms = 0.0f;
    def.effect_tree_nodes[0].parent_index = -1;
    def.effect_tree_nodes[0].repeat_count = 1;
    /* node1: child of 0, delay 100ms, window periodic (duration based) 250ms window every 100ms */
    def.effect_tree_nodes[1].effect_spec_id = child1_eff;
    def.effect_tree_nodes[1].parent_index = 0;
    def.effect_tree_nodes[1].delay_ms = 100.0f;
    def.effect_tree_nodes[1].duration_ms = 250.0f;
    def.effect_tree_nodes[1].repeat_count = 0;
    def.effect_tree_nodes[1].repeat_interval_ms = 100.0f;
    /* node2: chained after child1 via parent link with its own delay 50ms (absolute start =
     * start(child1)+50) */
    def.effect_tree_nodes[2].effect_spec_id = child2_eff;
    def.effect_tree_nodes[2].parent_index = 1;
    def.effect_tree_nodes[2].delay_ms = 50.0f;
    def.effect_tree_nodes[2].repeat_count = 1; /* one shot */

    int sid = rogue_skill_register(&def);
    assert(sid >= 0);
    assert(rogue_skill_rank_up(sid) == 1);
    RogueSkillCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.now_ms = 0.0;
    assert(rogue_skill_try_activate(sid, &ctx) == 1);

    /* t=0 root applied */
    rogue_effects_update(0.0f);
    assert(rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE) == 1);
    /* before 100 -> no child1 */
    rogue_effects_update(99.0f);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 0);
    /* at 100 first child1 tick */
    rogue_effects_update(100.0f);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 2);
    /* child2 scheduled at child1 start +50 => 150 */
    rogue_effects_update(150.0f);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 5); /* +3 from child2 */
    /* next child1 ticks at 200 and 300 (<= window start+250=350) */
    rogue_effects_update(200.0f);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 7); /* +2 periodic tick */
    rogue_effects_update(300.0f);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 9);
    /* beyond window */
    rogue_effects_update(400.0f);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 9);

    printf("EFFECT_TREE_SCHEDULE_OK\n");
    rogue_skills_shutdown();
    rogue_effect_reset();
    return 0;
}
