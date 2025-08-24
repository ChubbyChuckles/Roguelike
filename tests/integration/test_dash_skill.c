/* Dash integration test */
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
static int test_dash_effect(const RogueSkillDef* def, RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) def;
    (void) ctx;
    float dist = 25.0f + st->rank * 10.0f;
    float nx = g_app.player.base.pos.x;
    float ny = g_app.player.base.pos.y;
    switch (g_app.player.facing)
    {
    case 0:
        ny += dist;
        break;
    case 1:
        nx -= dist;
        break;
    case 2:
        nx += dist;
        break;
    case 3:
        ny -= dist;
        break;
    }
    if (nx < 0)
        nx = 0;
    if (ny < 0)
        ny = 0;
    if (nx > g_app.world_map.width - 1)
        nx = (float) (g_app.world_map.width - 1);
    if (ny > g_app.world_map.height - 1)
        ny = (float) (g_app.world_map.height - 1);
    g_app.player.base.pos.x = nx;
    g_app.player.base.pos.y = ny;
    return 1;
}
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }
/* Baseline tree stub (not needed) */
void rogue_skill_tree_register_baseline(void) {}

int main(void)
{
    /* Initialize minimal world bounds so dash clamping logic has limits */
    g_app.world_map.width = 500;
    g_app.world_map.height = 500;
    rogue_skills_init();
    RogueSkillDef dash = {0};
    dash.id = -1;
    dash.name = "Dash";
    dash.icon = "icon_dash";
    dash.max_rank = 3;
    dash.skill_strength = 0;
    dash.base_cooldown_ms = 3000.0f;
    dash.cooldown_reduction_ms_per_rank = 400.0f;
    dash.on_activate = test_dash_effect;
    dash.is_passive = 0;
    dash.synergy_id = -1;
    dash.tags = 0;
    int id = rogue_skill_register(&dash);
    if (id != 0)
    {
        printf("id mismatch\n");
        return 1;
    }
    g_app.talent_points = 5;
    if (rogue_skill_rank_up(id) <= 0)
    {
        printf("rank fail\n");
        return 2;
    }
    g_app.player.base.pos.x = 10;
    g_app.player.base.pos.y = 10;
    g_app.player.facing = 2; /* right */
    RogueSkillCtx ctx = {0, 1, g_app.talent_points};
    if (!rogue_skill_try_activate(id, &ctx))
    {
        printf("activate fail\n");
        return 3;
    }
    float moved = g_app.player.base.pos.x - 10.0f;
    if (moved < 20.0f || moved > 60.0f)
    {
        printf("dash distance unexpected %.2f\n", moved);
        return 4;
    }
    double cd_end = rogue_skill_get_state(id)->cooldown_end_ms;
    if (cd_end <= 0)
    {
        printf("cooldown not set\n");
        return 5;
    }
    /* Attempt activation during cooldown - should fail */
    if (rogue_skill_try_activate(id, &ctx))
    {
        printf("cooldown gating fail\n");
        return 6;
    }
    /* Simulate time passing beyond cooldown */
    RogueSkillCtx ctx2 = {cd_end + 1, 1, g_app.talent_points};
    if (!rogue_skill_try_activate(id, &ctx2))
    {
        printf("post-cd activate fail\n");
        return 7;
    }
    return 0;
}
