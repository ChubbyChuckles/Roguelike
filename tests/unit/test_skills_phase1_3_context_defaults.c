#define SDL_MAIN_HANDLED 1
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include "../../src/entities/player.h"
#include <assert.h>
#include <stdio.h>

/* Capture the last ctx passed to on_activate for inspection. */
static RogueSkillCtx g_last_ctx;
static int cb_capture_ctx(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    if (ctx)
        g_last_ctx = *ctx;
    return ROGUE_ACT_CONSUMED;
}

int main(void)
{
    rogue_skills_init();
    g_app.player.level = 1;
    g_app.talent_points = 99; /* allow unlocks */
    g_app.player.base.pos.x = 123.0f;
    g_app.player.base.pos.y = 456.0f;
    g_app.player.max_action_points = 100;
    g_app.player.action_points = 100;
    g_app.player.max_mana = 100;
    g_app.player.mana = 100;

    RogueSkillDef s = {0};
    s.name = "PH1_3 Ctx Defaults";
    s.max_rank = 1;
    s.on_activate = cb_capture_ctx;
    s.base_cooldown_ms = 0;
    int id = rogue_skill_register(&s);
    assert(rogue_skill_rank_up(id) == 1);

    /* Empty ctx should default positions to player pos and clamp affected list. */
    RogueSkillCtx ctx = {0};
    ctx.now_ms = 0.0;
    /* Deliberately set affected count > 8 to test clamp */
    ctx.affected_entity_count = 250; /* will be clamped */
    for (int i = 0; i < 8; ++i)
        ctx.affected_entity_ids[i] = i + 1;
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    /* Verify captured ctx fields */
    assert((int) g_last_ctx.cast_pos_x == (int) g_app.player.base.pos.x);
    assert((int) g_last_ctx.cast_pos_y == (int) g_app.player.base.pos.y);
    assert((int) g_last_ctx.target_pos_x == (int) g_last_ctx.cast_pos_x);
    assert((int) g_last_ctx.target_pos_y == (int) g_last_ctx.cast_pos_y);
    assert(g_last_ctx.affected_entity_count == 8);

    /* Provide explicit positions; ensure they pass through unchanged. */
    RogueSkillCtx ctx2 = {0};
    ctx2.now_ms = 10.0;
    ctx2.cast_pos_x = 10.0f;
    ctx2.cast_pos_y = 20.0f;
    ctx2.target_pos_x = 30.0f;
    ctx2.target_pos_y = 40.0f;
    ctx2.affected_entity_count = 3;
    ctx2.affected_entity_ids[0] = 42;
    ctx2.affected_entity_ids[1] = 1337;
    ctx2.affected_entity_ids[2] = 7;
    assert(rogue_skill_try_activate(id, &ctx2) == 1);
    assert((int) g_last_ctx.cast_pos_x == 10);
    assert((int) g_last_ctx.cast_pos_y == 20);
    assert((int) g_last_ctx.target_pos_x == 30);
    assert((int) g_last_ctx.target_pos_y == 40);
    assert(g_last_ctx.affected_entity_count == 3);
    assert(g_last_ctx.affected_entity_ids[0] == 42);
    assert(g_last_ctx.affected_entity_ids[1] == 1337);
    assert(g_last_ctx.affected_entity_ids[2] == 7);

    printf("PH1_3_CTX_DEFAULTS_OK cast=(%.0f,%.0f) target=(%.0f,%.0f) n=%u\n",
           g_last_ctx.cast_pos_x, g_last_ctx.cast_pos_y, g_last_ctx.target_pos_x,
           g_last_ctx.target_pos_y, g_last_ctx.affected_entity_count);
    fflush(stdout);
    rogue_skills_shutdown();
    return 0;
}
