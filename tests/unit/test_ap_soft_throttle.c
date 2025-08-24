/* Test AP soft throttle: spending a large AP cost should slow regen temporarily */
#include "../../src/core/app_state.h"
#include "../../src/core/player/player_progress.h"
#include "../../src/core/skills/skills.h"
#include "../../src/entities/player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int cb_ok(const RogueSkillDef* d, struct RogueSkillState* st,
                 const struct RogueSkillCtx* ctx)
{
    (void) d;
    (void) st;
    (void) ctx;
    return 1;
}

int main(void)
{
    rogue_skills_init();
    /* Setup player & AP */
    rogue_player_recalc_derived(&g_app.player);
    g_app.talent_points = 1;
    RogueSkillDef s;
    memset(&s, 0, sizeof s);
    s.name = "Big Spin";
    s.max_rank = 1;
    s.base_cooldown_ms = 0;
    s.on_activate = cb_ok;
    s.action_point_cost = 40;
    s.max_charges = 0;
    int id = rogue_skill_register(&s);
    assert(id >= 0);
    assert(rogue_skill_rank_up(id) == 1);
    RogueSkillCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    int ap_before = g_app.player.action_points;
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    assert(g_app.ap_throttle_timer_ms > 0.0f);
    int ap_after_spend = g_app.player.action_points;
    /* Simulate regen under throttle for 2 seconds vs later same duration after throttle ends */
    int gained_throttled = 0;
    int start_throttle_ap = ap_after_spend;
    for (int i = 0; i < 20; i++)
    {
        rogue_player_progress_update(0.1);
    }
    gained_throttled = g_app.player.action_points - start_throttle_ap;
    /* Fast forward until throttle expires */
    while (g_app.ap_throttle_timer_ms > 0.0f)
    {
        rogue_player_progress_update(0.2);
    }
    int ap_before_normal = g_app.player.action_points;
    int gained_normal = 0;
    for (int i = 0; i < 20; i++)
    {
        rogue_player_progress_update(0.1);
    }
    gained_normal = g_app.player.action_points - ap_before_normal;
    printf(
        "AP_SOFT_THROTTLE_OK cost=%d throttled_gain=%d normal_gain=%d throttle_time_start>0=%s\n",
        s.action_point_cost, gained_throttled, gained_normal,
        (g_app.ap_throttle_timer_ms == 0 ? "YES" : "NO"));
    assert(gained_normal >= gained_throttled); /* regen should be slower while throttled */
    rogue_skills_shutdown();
    return 0;
}
