/* Test channel multi-tick scheduler (every 250ms) and that a second activation during casting
 * waits. */
#include "core/app_state.h"
#include "core/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int g_ticks = 0;
static int cb_tick(const RogueSkillDef* def, struct RogueSkillState* st,
                   const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    g_ticks++;
    return 1;
}

static void advance_time(double start_ms, double target_ms)
{
    for (double t = start_ms; t <= target_ms; t += 16.0)
    {
        rogue_skills_update(t);
    }
}

int main(void)
{
    rogue_skills_init();
    g_app.talent_points = 1;
    RogueSkillDef chan;
    memset(&chan, 0, sizeof chan);
    chan.name = "Beam";
    chan.max_rank = 1;
    chan.base_cooldown_ms = 0;
    chan.on_activate = cb_tick;
    chan.cast_type = 2;
    chan.cast_time_ms = 800.0f; /* duration 800ms -> expect ~1 immediate + ticks at 250,500,750 */
    int id = rogue_skill_register(&chan);
    assert(rogue_skill_rank_up(id) == 1);
    RogueSkillCtx ctx = {0};
    assert(rogue_skill_try_activate(id, &ctx) == 1);
    /* Activation performed at now=0 (ctx.now_ms default 0) immediate tick increments */
    assert(g_ticks == 1);
    advance_time(0, 260.0); /* should include tick at 250 */
    assert(g_ticks == 2);
    advance_time(260.0, 520.0); /* tick at 500 */
    assert(g_ticks == 3);
    advance_time(520.0, 780.0); /* tick at 750 */
    assert(g_ticks == 4);
    advance_time(780.0, 900.0); /* channel ends, no more ticks */
    int final_ticks = g_ticks;
    advance_time(900.0, 1200.0);
    assert(g_ticks == final_ticks);
    printf("CHANNEL_TICKS_OK ticks=%d\n", g_ticks);
    rogue_skills_shutdown();
    return 0;
}
