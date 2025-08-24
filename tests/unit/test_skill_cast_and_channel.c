/* Test basic casting (delayed effect) and channel (immediate + duration) */
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include "../../src/entities/player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int g_instant_hits = 0;
static int cb_record(const RogueSkillDef* def, struct RogueSkillState* st,
                     const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    g_instant_hits++;
    return 1;
}

static void advance_time(double ms)
{
    /* crude: call update enough times to simulate ms assuming 16ms steps */
    double steps = ms / 16.0;
    for (int i = 0; i < (int) steps + 1; i++)
        rogue_skills_update(i * 16.0);
}

int main(void)
{
    rogue_skills_init();
    g_app.talent_points = 2;
    RogueSkillDef castSkill;
    memset(&castSkill, 0, sizeof castSkill);
    castSkill.name = "Fire Chant";
    castSkill.max_rank = 1;
    castSkill.base_cooldown_ms = 0;
    castSkill.on_activate = cb_record;
    castSkill.cast_type = 1;
    castSkill.cast_time_ms = 160.0f; /* 10 frames */
    RogueSkillDef chanSkill;
    memset(&chanSkill, 0, sizeof chanSkill);
    chanSkill.name = "Beam";
    chanSkill.max_rank = 1;
    chanSkill.base_cooldown_ms = 0;
    chanSkill.on_activate = cb_record;
    chanSkill.cast_type = 2;
    chanSkill.cast_time_ms = 160.0f;
    int id_cast = rogue_skill_register(&castSkill);
    int id_chan = rogue_skill_register(&chanSkill);
    assert(rogue_skill_rank_up(id_cast) == 1);
    assert(rogue_skill_rank_up(id_chan) == 1);
    RogueSkillCtx ctx = {0};
    int before = g_instant_hits;
    assert(rogue_skill_try_activate(id_cast, &ctx) == 1); /* starts casting, no immediate hit */
    assert(g_instant_hits == before);
    advance_time(200.0); /* finish cast */
    assert(g_instant_hits == before + 1);
    before = g_instant_hits;
    assert(rogue_skill_try_activate(id_chan, &ctx) ==
           1); /* channel start triggers initial effect */
    assert(g_instant_hits == before + 1);
    advance_time(200.0); /* channel end passes */
    /* no extra tick yet */
    assert(g_instant_hits == before + 1);
    printf("CAST_CHANNEL_OK hits=%d\n", g_instant_hits);
    rogue_skills_shutdown();
    return 0;
}
