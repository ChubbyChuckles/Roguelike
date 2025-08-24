#include "../../src/core/app_state.h"
#include "../../src/core/skills/skills.h"
#include <stdio.h>
#include <stdlib.h>

RogueAppState g_app;                    /* zero */
RoguePlayer g_exposed_player_for_stats; /* unused */

static int effect_counter(const RogueSkillDef* def, struct RogueSkillState* st,
                          const RogueSkillCtx* ctx)
{
    (void) def;
    (void) ctx;
    st->uses++;
    return 1;
}

int main(void)
{
    rogue_skills_init();
    RogueSkillDef d = {0};
    d.id = -1;
    d.name = "TestSkill";
    d.icon = "icon";
    d.max_rank = 3;
    d.skill_strength = 0;
    d.base_cooldown_ms = 1000.0f;
    d.cooldown_reduction_ms_per_rank = 100.0f;
    d.on_activate = effect_counter;
    int id = rogue_skill_register(&d);
    if (id < 0)
    {
        printf("register fail\n");
        return 1;
    }
    g_app.talent_points = 5;
    if (rogue_skill_rank_up(id) != 1)
    {
        printf("rank1 fail\n");
        return 1;
    }
    if (rogue_skill_rank_up(id) != 2)
    {
        printf("rank2 fail\n");
        return 1;
    }
    RogueSkillCtx ctx = {0, 1, g_app.talent_points};
    if (!rogue_skill_try_activate(id, &ctx))
    {
        printf("activate fail\n");
        return 1;
    }
    const RogueSkillState* st = rogue_skill_get_state(id);
    if (!st || st->rank != 2 || st->uses < 1)
    {
        printf("state mismatch\n");
        return 1;
    }
    if (rogue_skill_rank_up(id) != 3)
    {
        printf("rank3 fail\n");
        return 1;
    }
    if (rogue_skill_rank_up(id) != 3)
    {
        printf("over-rank fail\n");
        return 1;
    }
    rogue_skills_shutdown();
    return 0;
}
