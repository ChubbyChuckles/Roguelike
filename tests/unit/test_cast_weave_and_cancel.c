/* Test cast weaving restriction and early cancel scaling */
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static float g_last_scalar = 0.0f;
static int g_uses = 0;
static int cb_cast(const RogueSkillDef* d, struct RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) d;
    (void) st;
    g_last_scalar = ctx ? ctx->partial_scalar : 1.0f;
    g_uses++;
    return 1;
}

static void advance(double start, double end)
{
    for (double t = start; t <= end; t += 16.0)
    {
        rogue_skills_update(t);
    }
}

int main(void)
{
    rogue_skills_init();
    g_app.talent_points = 2;
    RogueSkillDef skill;
    memset(&skill, 0, sizeof skill);
    skill.name = "WeaveSpell";
    skill.max_rank = 1;
    skill.base_cooldown_ms = 0;
    skill.on_activate = cb_cast;
    skill.cast_type = 1;
    skill.cast_time_ms = 400.0f;
    skill.min_weave_ms = 300;
    skill.early_cancel_min_pct = 25;
    int id = rogue_skill_register(&skill);
    assert(rogue_skill_rank_up(id) == 1);
    RogueSkillCtx ctx = {0};
    assert(rogue_skill_try_activate(id, &ctx) == 1); /* start cast */
    advance(0, 160);                                 /* 40% progress */
    assert(rogue_skill_try_cancel(id, &ctx) == 1);   /* early cancel allowed (>=25%) */
    ((struct RogueSkillState*) rogue_skill_get_state(id))->cooldown_end_ms = 0.0;
    assert(g_uses == 1);
    assert(g_last_scalar > 0.35f && g_last_scalar < 0.5f); /* ~0.4 */
    /* Attempt immediate re-cast within weave window (<300ms since last_cast_ms=cancel time ~160ms)
     * should fail */
    assert(rogue_skill_try_activate(id, &ctx) == 0);
    /* Advance time past weave window */
    advance(160, 500);
    ctx.now_ms = 500.0; /* ensure context time advanced */
    ((struct RogueSkillState*) rogue_skill_get_state(id))->cooldown_end_ms = 0.0;
    assert(rogue_skill_try_activate(id, &ctx) == 1); /* allowed now */
    printf("WEAVE_CANCEL_OK scalar=%.2f uses=%d\n", g_last_scalar, g_uses);
    rogue_skills_shutdown();
    return 0;
}
