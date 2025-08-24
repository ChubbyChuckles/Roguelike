#define SDL_MAIN_HANDLED 1
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include "../../src/core/skills/skills_internal.h"
#include <assert.h>
#include <stdio.h>

/* Minimal effect callback that always consumes and has AP cost via def */
static int consume_on_activate(const struct RogueSkillDef* def, struct RogueSkillState* st,
                               const RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return 1;
}

int main(void)
{
    /* Register two simple instant skills with AP costs and short cooldowns */
    RogueSkillDef a = {0};
    a.name = "A";
    a.max_rank = 1;
    a.base_cooldown_ms = 200.0f;
    a.on_activate = consume_on_activate;
    a.action_point_cost = 10;
    int sid_a = rogue_skill_register(&a);

    RogueSkillDef b = {0};
    b.name = "B";
    b.max_rank = 1;
    b.base_cooldown_ms = 300.0f;
    b.on_activate = consume_on_activate;
    b.action_point_cost = 15;
    int sid_b = rogue_skill_register(&b);

    (void) sid_a;
    (void) sid_b;

    g_app.game_time_ms = 0.0;
    char out[256];
    int rc = skill_simulate_rotation(
        "{\"duration_ms\":1000,\"tick_ms\":50,\"ap_regen_per_sec\":50,\"priority\":[0,1]}", out,
        (int) sizeof out);
    assert(rc == 0);
    /* Expect some casts and a valid JSON string */
    assert(out[0] == '{');
    assert(strstr(out, "\"total_casts\":") != NULL);
    printf("%s\n", out);
    return 0;
}
