#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/core/app/app_state.h" /* for g_app */
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_coeffs.h"
#include "../../src/core/skills/skills_coeffs_load.h"
#include "../../src/entities/player.h" /* for rogue_player_recalc_derived */
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include "../../src/graphics/effect_spec_load.h"
#include <assert.h>
#include <stdio.h>

static int noop(const struct RogueSkillDef* def, struct RogueSkillState* st,
                const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return 1;
}

int main(void)
{
    /* Init minimal subsystems */
    if (!rogue_event_bus_get_instance())
    {
        RogueEventBusConfig cfg = rogue_event_bus_create_default_config("test_bus");
        cfg.worker_thread_count = 0;
        cfg.enable_replay_recording = false;
        rogue_event_bus_init(&cfg);
    }
    rogue_skills_init();
    rogue_buffs_init();
    rogue_buffs_set_dampening(0.0);

    /* Register one skill to allow coeff binding */
    RogueSkillDef def = {0};
    def.name = "S";
    def.max_rank = 1;
    def.on_activate = noop;
    int sid = rogue_skill_register(&def);
    assert(sid == 0);
    g_app.player.level = 5;
    g_app.talent_points = 1;
    rogue_player_recalc_derived(&g_app.player);
    assert(rogue_skill_rank_up(sid) == 1);

    /* Coeffs JSON: missing skill_id should hard-fail (return <0) */
    const char* BAD_COEFF_JSON = "[{\"base_scalar\":1.0}]";
    int rc = rogue_skill_coeffs_parse_json_text(BAD_COEFF_JSON);
    assert(rc < 0);

    /* Coeffs JSON: valid entry should succeed */
    const char* OK_COEFF_JSON = "[{\"skill_id\":0,\"base_scalar\":1.1}]";
    rc = rogue_skill_coeffs_parse_json_text(OK_COEFF_JSON);
    assert(rc == 1);
    assert(rogue_skill_coeff_exists(0) == 1);

    /* EffectSpec JSON: reject invalid references */
    const char* EFFECTS_JSON =
        "["
        /* Unknown buff type for STAT_BUFF -> reject */
        "{\"kind\":\"STAT_BUFF\",\"buff_type\":\"UNKNOWN\",\"magnitude\":3,\"duration_ms\":500},"
        /* Unknown stack rule -> reject */
        "{\"kind\":\"STAT_BUFF\",\"buff_type\":\"STAT_STRENGTH\",\"stack_rule\":\"NOPE\","
        "\"magnitude\":2,\"duration_ms\":300},"
        /* Missing buff_type for STAT_BUFF -> reject */
        "{\"kind\":\"STAT_BUFF\",\"magnitude\":1,\"duration_ms\":100},"
        /* Valid DOT with known damage type */
        "{\"kind\":\"DOT\",\"damage_type\":\"FIRE\",\"magnitude\":5,\"duration_ms\":600,\"pulse_"
        "period_ms\":200}"
        "]";
    int ids[8] = {0};
    int n = rogue_effects_load_from_json_text(EFFECTS_JSON, ids, 8);
    assert(n == 1);

    /* Apply the only valid effect and ensure behavior is sane */
    rogue_effect_apply(ids[0], 0.0);
    /* No buffs should be present since it's a DOT; ensure buff totals unchanged */
    int str_total = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    assert(str_total == 0);

    puts("PH10.5 parser edges OK");
    return 0;
}
