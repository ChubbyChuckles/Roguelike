/* Test input buffering: start a long cast, attempt a second cast skill shortly before completion,
 * ensure it fires immediately after. */
#include "core/app_state.h"
#include "core/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int g_hits_a = 0, g_hits_b = 0;
static int cb_a(const RogueSkillDef* d, struct RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) d;
    (void) st;
    (void) ctx;
    g_hits_a++;
    return 1;
}
static int cb_b(const RogueSkillDef* d, struct RogueSkillState* st, const RogueSkillCtx* ctx)
{
    (void) d;
    (void) st;
    (void) ctx;
    g_hits_b++;
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
    RogueSkillDef A;
    memset(&A, 0, sizeof A);
    A.name = "LongCast";
    A.max_rank = 1;
    A.base_cooldown_ms = 0;
    A.on_activate = cb_a;
    A.cast_type = 1;
    A.cast_time_ms = 400.0f;
    A.input_buffer_ms = 150; /* allow next skill queue inside 150ms */
    RogueSkillDef B;
    memset(&B, 0, sizeof B);
    B.name = "FollowCast";
    B.max_rank = 1;
    B.base_cooldown_ms = 0;
    B.on_activate = cb_b;
    B.cast_type = 1;
    B.cast_time_ms = 50.0f;
    B.input_buffer_ms = 0;
    int idA = rogue_skill_register(&A);
    int idB = rogue_skill_register(&B);
    assert(rogue_skill_rank_up(idA) == 1);
    assert(rogue_skill_rank_up(idB) == 1);
    RogueSkillCtx ctx = {0};
    assert(rogue_skill_try_activate(idA, &ctx) == 1); /* start long cast */
    advance(0, 300);                                  /* progress to 300ms (100ms left) */
    /* Now queue instant follow cast within buffer window */
    assert(rogue_skill_try_activate(idB, &ctx) == 1); /* should buffer */
    assert(g_hits_b == 0);                            /* B not fired yet */
    advance(300, 420);     /* A finishes around 400, B should start and finish by 420 */
    assert(g_hits_a == 1); /* A completed */
    assert(g_hits_b == 1); /* B fired after A */
    printf("INPUT_BUFFER_OK A=%d B=%d\n", g_hits_a, g_hits_b);
    rogue_skills_shutdown();
    return 0;
}
