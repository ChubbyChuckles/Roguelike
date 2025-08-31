#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_validate.h"
#include "../../src/entities/player.h"
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int cb_consume(const RogueSkillDef* def, struct RogueSkillState* st,
                      const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return 1; /* consumed */
}

static int register_buff_spec(unsigned short buff_type, int magnitude, float duration_ms)
{
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_STAT_BUFF;
    es.buff_type = buff_type;
    es.magnitude = magnitude;
    es.duration_ms = duration_ms;
    /* default stack rule for STAT_BUFF is overridden to ADD in register() */
    int id = rogue_effect_register(&es);
    assert(id >= 0);
    return id;
}

static int make_skill_with_node(int primary_eff, int node_eff, float node_delay_ms, int repeats,
                                float repeat_interval_ms, unsigned char hp_gate,
                                unsigned char cast_type, float cast_time_ms)
{
    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = "NodeSkill";
    def.max_rank = 1;
    def.base_cooldown_ms = 100; /* minimal */
    def.on_activate = cb_consume;
    def.effect_spec_id = primary_eff;
    def.action_point_cost = 0;
    def.resource_cost_mana = 0;
    def.cast_type = cast_type;
    def.cast_time_ms = cast_time_ms;
    def.effect_node_count = 1;
    def.effect_nodes[0].effect_spec_id = node_eff;
    def.effect_nodes[0].delay_ms = node_delay_ms;
    def.effect_nodes[0].repeat_count = repeats;
    def.effect_nodes[0].repeat_interval_ms = repeat_interval_ms;
    def.effect_nodes[0].require_player_health_below_pct = hp_gate;
    int sid = rogue_skill_register(&def);
    assert(sid >= 0);
    assert(rogue_skill_rank_up(sid) == 1);
    return sid;
}

static void reset_core(void)
{
    rogue_skills_shutdown();
    rogue_effect_reset();
    /* Also reset buffs so previous subtests don't leak totals */
    rogue_buffs_init();
    rogue_skills_init();
    /* minimal player derived stats */
    rogue_player_recalc_derived(&g_app.player);
}

int main(void)
{
    /* Common init */
    rogue_skills_init();
    rogue_buffs_init();
    /* Initialize event bus required by skills */
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("skills_phase1_2_bus");
    assert(rogue_event_bus_init(&cfg) && "event bus init");
    rogue_effect_reset();
    g_app.talent_points = 1; /* allow rank up */
    rogue_player_recalc_derived(&g_app.player);

    /* Test A: activation schedules node with delay and repeats */
    {
        int primary = register_buff_spec(ROGUE_BUFF_POWER_STRIKE, 1, 1000.0f);
        int node = register_buff_spec(ROGUE_BUFF_STAT_STRENGTH, 3, 1000.0f);
        int sid = make_skill_with_node(primary, node, 50.0f, 2, 100.0f, 0, 0, 0.0f);
        RogueSkillCtx ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.now_ms = 0.0;
        assert(rogue_skill_try_activate(sid, &ctx) == 1);
        /* No node apply before 50ms */
        assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 0);
        rogue_effects_update(49.0);
        assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 0);
        /* first scheduled apply at 50ms */
        rogue_effects_update(50.0);
        assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 3);
        /* second at 150ms */
        rogue_effects_update(150.0);
        assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 6);
        /* third at 250ms */
        rogue_effects_update(250.0);
        assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 9);
    }

    /* Reset effect queue and skills for next tests */
    reset_core();
    g_app.talent_points = 1;

    /* Test B: health gate prevents node application when above threshold; allows below */
    {
        int primary = register_buff_spec(ROGUE_BUFF_POWER_STRIKE, 1, 1000.0f);
        int node = register_buff_spec(ROGUE_BUFF_STAT_STRENGTH, 5, 1000.0f);
        int sid = make_skill_with_node(primary, node, 0.0f, 0, 0.0f, 50 /* <50% */, 0, 0.0f);
        /* Sanity: ensure node and gate registered correctly */
        const RogueSkillDef* dchk = rogue_skill_get_def(sid);
        assert(dchk != NULL);
        assert(dchk->effect_node_count == 1);
        assert(dchk->effect_nodes[0].effect_spec_id == node);
        assert(dchk->effect_nodes[0].require_player_health_below_pct == 50);
        /* Gate ON: 100/100 health */
        g_app.player.max_health = 100;
        g_app.player.health = 100;
        RogueSkillCtx ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.now_ms = 0.0;
        assert(rogue_skill_try_activate(sid, &ctx) == 1);
        /* Node should not apply due to gate */
        assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 0);
        /* Gate OFF: lower health */
        g_app.player.health = 40; /* 40% < 50% */
        ctx.now_ms = 100.0;
        assert(rogue_skill_try_activate(sid, &ctx) == 1);
        /* delay=0 => immediate apply */
        assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 5);
    }

    /* Reset for cast completion scheduling */
    reset_core();
    g_app.talent_points = 1;
    rogue_player_recalc_derived(&g_app.player);

    /* Test C: cast-type skill applies node on cast completion (delay=0) */
    {
        int primary = register_buff_spec(ROGUE_BUFF_POWER_STRIKE, 1, 500.0f);
        int node = register_buff_spec(ROGUE_BUFF_STAT_STRENGTH, 2, 500.0f);
        int sid = make_skill_with_node(primary, node, 0.0f, 0, 0.0f, 0, 1 /* cast */, 100.0f);
        RogueSkillCtx ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.now_ms = 0.0;
        assert(rogue_skill_try_activate(sid, &ctx) == 1);
        /* During cast, before completion, node not yet applied */
        assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 0);
        /* Advance time until cast completes; update skills to trigger completion */
        rogue_skills_update(100.0);
        /* Node applied immediately on completion */
        assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 2);
    }

    printf("EFFECT_NODES_OK\n");
    rogue_skills_shutdown();
    rogue_effect_reset();
    return 0;
}
