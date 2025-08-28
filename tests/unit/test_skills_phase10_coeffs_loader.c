/* Ensure SDL doesn't redefine main on Windows builds */
#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_coeffs.h"
#include "../../src/core/skills/skills_coeffs_load.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

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
    /* Minimal event bus init for tests that publish SKILL_UNLOCKED on first rank-up. */
    if (!rogue_event_bus_get_instance())
    {
        RogueEventBusConfig cfg = rogue_event_bus_create_default_config("test_bus");
        cfg.worker_thread_count = 0; /* synchronous */
        cfg.enable_replay_recording = false;
        rogue_event_bus_init(&cfg);
    }
    rogue_skills_init();
    /* Register two skills so coeffs can attach */
    RogueSkillDef a = {0};
    a.name = "A";
    a.max_rank = 3;
    a.on_activate = effect_noop;
    int idA = rogue_skill_register(&a);
    RogueSkillDef b = {0};
    b.name = "B";
    b.max_rank = 2;
    b.on_activate = effect_noop;
    int idB = rogue_skill_register(&b);
    assert(idA >= 0 && idB >= 0);
    /* Ensure sufficient points and level for three rank-ups across A (2) and B (1). */
    g_app.player.level = 10; /* bypass any level gating */
    g_app.talent_points = 3; /* allow rank-ups: A x2, B x1 */
    assert(rogue_skill_rank_up(idA) == 1);
    assert(rogue_skill_rank_up(idA) == 2);
    assert(rogue_skill_rank_up(idB) == 1);

    const char* JSON = "[\n"
                       " {\"skill_id\":0,\"base_scalar\":1.10,\"per_rank_scalar\":0.05,\"str_pct_"
                       "per10\":2,\"dex_pct_per10\":1,\"stat_cap_pct\":50,\"stat_softness\":30},\n"
                       " {\"skill_id\":1,\"base_scalar\":1.00,\"per_rank_scalar\":0.10,\"int_pct_"
                       "per10\":3,\"stat_cap_pct\":40,\"stat_softness\":20}\n"
                       "]\n";
    int n = rogue_skill_coeffs_parse_json_text(JSON);
    assert(n == 2);
    assert(rogue_skill_coeff_exists(0) == 1);
    assert(rogue_skill_coeff_exists(1) == 1);

    /* Baseline player stats */
    g_app.player.strength = 30;     /* +6% if 2% per 10 */
    g_app.player.dexterity = 18;    /* +1.8% if 1% per 10 */
    g_app.player.intelligence = 12; /* +3.6% if 3% per 10 */
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_force_update(&g_app.player);

    float cA = skill_get_effective_coefficient(0);
    assert(cA > 1.22f && cA < 1.26f); /* similar to Phase 8 test */

    float cB = skill_get_effective_coefficient(1);
    assert(cB > 0.95f && cB < 1.10f);

    puts("PH10_COEFFS_LOADER_OK");
    return 0;
}
