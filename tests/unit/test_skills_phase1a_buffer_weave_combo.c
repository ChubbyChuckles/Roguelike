#include "../../src/core/app_state.h"
#include "../../src/core/buffs.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int cb_noop(const RogueSkillDef* d, RogueSkillState* s, const RogueSkillCtx* c)
{
    (void) d;
    (void) s;
    (void) c;
    return 1;
}

/* Advance a monotonic simulated clock in ~16ms ticks (plus a final remainder). */
static double g_now_ms = 0.0;
static void advance(double ms)
{
    if (ms <= 0)
        return;
    int ticks = (int) (ms / 16.0);
    for (int i = 0; i < ticks; ++i)
    {
        g_now_ms += 16.0;
        rogue_skills_update(g_now_ms);
    }
    double rem = ms - (ticks * 16.0);
    if (rem > 0.0)
    {
        g_now_ms += rem;
        rogue_skills_update(g_now_ms);
    }
}

int main(void)
{
    /* Initialize systems required by skills & tests */
    rogue_skills_init();
    rogue_buffs_init();
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("skills_test_bus");
    assert(rogue_event_bus_init(&cfg) && "event bus init");
    /* Define two casted skills to test input buffer between casts */
    RogueSkillDef A;
    memset(&A, 0, sizeof A);
    A.name = "CastA";
    A.max_rank = 1;
    A.on_activate = cb_noop;
    A.cast_type = 1;
    A.cast_time_ms = 160.0f;
    A.input_buffer_ms = 120;
    RogueSkillDef B;
    memset(&B, 0, sizeof B);
    B.name = "CastB";
    B.max_rank = 1;
    B.on_activate = cb_noop;
    B.cast_type = 1;
    B.cast_time_ms = 160.0f;
    B.min_weave_ms = 120;
    int idA = rogue_skill_register(&A);
    int idB = rogue_skill_register(&B);
    g_app.talent_points = 2;
    assert(rogue_skill_rank_up(idA) == 1);
    assert(rogue_skill_rank_up(idB) == 1);
    RogueSkillCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    /* Start casting A */
    assert(rogue_skill_try_activate(idA, &ctx) == 1);
    /* Queue B within buffer while A still casting */
    ctx.now_ms = 80.0; /* mid-cast */
    int q = rogue_skill_try_activate(idB, &ctx);
    assert(q == 1); /* accepted into buffer (not executed yet) */
    /* Finish cast; B should fire due to queued trigger. Advance well past both casts completing. */
    advance(360.0);
    const RogueSkillState* stB = rogue_skill_get_state(idB);
    /* After execution, last_cast_ms should be > 0 */
    assert(stB->last_cast_ms > 0.0);
    /* Ensure B's cast actually completes before testing weave rules */
    if (stB->casting_active)
        advance(240.0);

    /* Weave gate: use a fresh skill E and set last_cast via early cancel to avoid cooldown gate */
    RogueSkillDef E;
    memset(&E, 0, sizeof E);
    E.name = "WeaveE";
    E.max_rank = 1;
    E.on_activate = cb_noop;
    E.cast_type = 1;
    E.cast_time_ms = 160.0f;
    E.min_weave_ms = 120;
    E.early_cancel_min_pct = 0;
    int idE = rogue_skill_register(&E);
    g_app.talent_points = 1;
    assert(rogue_skill_rank_up(idE) == 1);
    /* Start and immediately cancel to stamp last_cast_ms without starting cooldown. */
    ctx.now_ms = g_now_ms;
    assert(rogue_skill_try_activate(idE, &ctx) == 1);
    assert(rogue_skill_try_cancel(idE, &ctx) == 1);
    const RogueSkillState* stE = rogue_skill_get_state(idE);
    /* Clear cooldown so weave gate is isolated (activation starts CD immediately). */
    ((struct RogueSkillState*) stE)->cooldown_end_ms = 0.0;
    assert(stE->last_cast_ms == ctx.now_ms);
    /* Below weave window -> blocked */
    ctx.now_ms = stE->last_cast_ms + 100.0; /* below min_weave */
    int blocked = rogue_skill_try_activate(idE, &ctx);
    assert(blocked == 0);
    /* Apply haste to bypass weave */
    rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE, 12, 5000.0, ctx.now_ms, ROGUE_BUFF_STACK_REFRESH, 0);
    int allowed = rogue_skill_try_activate(idE, &ctx);
    if (allowed != 1)
    {
        int haste_total = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
        double delta = ctx.now_ms - stE->last_cast_ms;
        fprintf(stderr,
                "DEBUG weave-bypass failed(E): haste=%d delta=%.2f min_weave=%u now=%.2f last=%.2f "
                "cd_end=%.2f casting_active=%d\n",
                haste_total, delta, E.min_weave_ms, ctx.now_ms, stE->last_cast_ms,
                stE->cooldown_end_ms, stE->casting_active);
    }
    assert(allowed == 1);

    /* Combo builder/spender: define two instants */
    RogueSkillDef C;
    memset(&C, 0, sizeof C);
    C.name = "Builder";
    C.max_rank = 1;
    C.on_activate = cb_noop;
    C.cast_type = 0;
    C.combo_builder = 1;
    RogueSkillDef D;
    memset(&D, 0, sizeof D);
    D.name = "Spender";
    D.max_rank = 1;
    D.on_activate = cb_noop;
    D.cast_type = 0;
    D.combo_spender = 1;
    int idC = rogue_skill_register(&C);
    int idD = rogue_skill_register(&D);
    g_app.talent_points = 2;
    assert(rogue_skill_rank_up(idC) == 1);
    assert(rogue_skill_rank_up(idD) == 1);
    ctx.now_ms += 10.0;
    assert(rogue_skill_try_activate(idC, &ctx) == 1);
    assert(g_app.player_combat.combo >= 1);
    ctx.now_ms += 10.0;
    assert(rogue_skill_try_activate(idD, &ctx) == 1);
    assert(g_app.player_combat.combo == 0);

    printf("PH1A_BUFFER_WEAVE_COMBO_OK combo=%d\n", g_app.player_combat.combo);
    rogue_event_bus_shutdown();
    rogue_skills_shutdown();
    return 0;
}
